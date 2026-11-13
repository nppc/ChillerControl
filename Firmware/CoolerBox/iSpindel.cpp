#include "iSpindel.h"
#include "CoolerBox.h"
#include "TelemetryQueue.h"
#include "TemperatureControl.h"
#include <ArduinoJson.h>

// Global iSpindel variables
float iSpindelTemperature = 0.0f;
float iSpindelPlato = -1.0f; // last received Plato reading; < 0 = no data
unsigned long lastISpindelMillis = 0;
int iSpindelRssi = 0;
int iSpindelIntervalSec = 900; // default 15 minutes

// -----------------------------------------------------------------------------
// Plato 48-hour Stability Tracking (Option A: 48 hourly buckets stored as int16_t)
// Scale: 100 (e.g. 13.42 Plato -> 1342), range clamped to [-32767, 32767]
// -----------------------------------------------------------------------------
static const uint8_t PLATO_HISTORY_CAPACITY = 48;
static int16_t platoHourlyHistory[PLATO_HISTORY_CAPACITY];
static uint8_t platoHistoryHead = 0;   // index of next write
static uint8_t platoHistoryCount = 0;  // count of filled hourly buckets
static float currentHourPlatoSum = 0.0f;
static uint16_t currentHourPlatoSamples = 0;
static unsigned long currentHourStartMillis = 0;

static bool fermentationFinished = false;
static uint8_t stableHoursCount = 0;
static float lastCalculatedSlope = 0.0f; // °P per hour
static float lastCalculatedDelta = 0.0f; // oldest - newest °P over window

static int16_t clampAndScalePlato(float plato) {
	if (!isfinite(plato)) {
		return INT16_MIN;
	}
	float scaled = plato * 100.0f;
	if (scaled <= -32767.0f) {
		return -32767;
	}
	if (scaled >= 32767.0f) {
		return 32767;
	}
	return static_cast<int16_t>(roundf(scaled));
}

void recordPlatoReading(float plato) {
	if (!isfinite(plato)) {
		return;
	}
	// Clamp individual reading to prevent extreme manual movements from corrupting the sum
	float clamped = constrain(plato, -327.67f, 327.67f);
	currentHourPlatoSum += clamped;
	currentHourPlatoSamples++;

	if (currentHourStartMillis == 0) {
		currentHourStartMillis = millis();
	}

	updatePlatoStability();
}

void updatePlatoStability() {
	unsigned long now = millis();
	if (currentHourStartMillis == 0) {
		currentHourStartMillis = now;
		return;
	}

	// 1 hour = 3600000 ms
	if ((now - currentHourStartMillis) >= 3600000UL) {
		if (currentHourPlatoSamples > 0) {
			float hourAvg = currentHourPlatoSum / static_cast<float>(currentHourPlatoSamples);
			int16_t scaledVal = clampAndScalePlato(hourAvg);

			platoHourlyHistory[platoHistoryHead] = scaledVal;
			platoHistoryHead = (platoHistoryHead + 1) % PLATO_HISTORY_CAPACITY;
			if (platoHistoryCount < PLATO_HISTORY_CAPACITY) {
				platoHistoryCount++;
			}

			// Calculate consecutive stable hours looking backwards
			if (platoHistoryCount >= 2) {
				uint8_t count = 0;
				// Newest bucket index
				int newest = (platoHistoryHead + PLATO_HISTORY_CAPACITY - 1) % PLATO_HISTORY_CAPACITY;
				int16_t currentVal = platoHourlyHistory[newest];

				// Step backward through history and check if earlier hours are within ±0.20 °P
				for (uint8_t h = 1; h < platoHistoryCount; ++h) {
					int prev = (newest - h + PLATO_HISTORY_CAPACITY) % PLATO_HISTORY_CAPACITY;
					int16_t prevVal = platoHourlyHistory[prev];
					int16_t diff = prevVal - currentVal; // positive if it was higher (dropped)
					// If previous hour was higher than current by more than 0.20 °P, stability broke
					if (diff > 20 || diff < -50) {
						break;
					}
					count = h;
				}
				stableHoursCount = count;
			} else {
				stableHoursCount = 0;
			}

			// Evaluate stability once we have the full 48-hour window
			if (platoHistoryCount >= PLATO_HISTORY_CAPACITY) {
				uint8_t oldestIdx = platoHistoryHead;
				uint8_t newestIdx = (platoHistoryHead + PLATO_HISTORY_CAPACITY - 1) % PLATO_HISTORY_CAPACITY;

				float oldestPlato = static_cast<float>(platoHourlyHistory[oldestIdx]) / 100.0f;
				float newestPlato = static_cast<float>(platoHourlyHistory[newestIdx]) / 100.0f;
				lastCalculatedDelta = oldestPlato - newestPlato; // positive if Plato fell

				// Linear regression over 48 points: x = 0..47, y = hourly Plato
				// Denominator: 48 * sum(x^2) - (sum(x))^2 = 48 * 35720 - (1128)^2 = 442176
				int64_t sumY = 0;
				int64_t sumXY = 0;
				for (uint8_t i = 0; i < PLATO_HISTORY_CAPACITY; ++i) {
					uint8_t idx = (oldestIdx + i) % PLATO_HISTORY_CAPACITY;
					int32_t y = platoHourlyHistory[idx];
					sumY += y;
					sumXY += static_cast<int64_t>(i) * y;
				}

				int64_t numerator = (48LL * sumXY) - (1128LL * sumY);
				lastCalculatedSlope = static_cast<float>(numerator) / 44217600.0f;

				bool flatDelta = (lastCalculatedDelta < 0.20f) && (lastCalculatedDelta > -0.50f);
				bool flatSlope = (lastCalculatedSlope > -0.005f);

				if (flatDelta && flatSlope) {
					if (!fermentationFinished) {
						fermentationFinished = true;
						Debug.addLine(String(F("Fermentation complete: 48h delta=")) +
						              String(lastCalculatedDelta, 2) + F("P slope=") +
						              String(lastCalculatedSlope, 4) + F("P/h"));
					}
				} else {
					fermentationFinished = false;
				}
			}
		}

		currentHourPlatoSum = 0.0f;
		currentHourPlatoSamples = 0;
		currentHourStartMillis = now;
	}
}

bool isFermentationComplete() {
	return fermentationFinished;
}

uint8_t getPlatoHistoryHours() {
	return platoHistoryCount;
}

uint8_t getPlatoStableHours() {
	return stableHoursCount;
}

float getPlato48hSlope() {
	return lastCalculatedSlope;
}

float getPlato48hDelta() {
	return lastCalculatedDelta;
}

// -----------------------------------------------------------------------------
// Web Handler for incoming iSpindel HTTP reports
// -----------------------------------------------------------------------------
void handleISpindel() {
#ifdef DEBUG_TEMPERATURE_SENSORS
	httpServer.send(200, "text/plain", "DEBUG temperature simulation active; iSpindel report ignored");
	return;
#endif

	if (!httpServer.hasArg("plain")) {
		httpServer.send(400, "text/plain", "Missing body");
		return;
	}

	JsonDocument localDoc;
	DeserializationError error = deserializeJson(localDoc, httpServer.arg("plain"));
	if (error) {
		httpServer.send(400, "text/plain", "JSON parse error");
		return;
	}

	// Update the values used by control and wake-window scheduling.
	if (localDoc["temperature"].is<float>()) {
		iSpindelTemperature = localDoc["temperature"].as<float>();
		lastISpindelMillis = millis();
	}
	if (localDoc["RSSI"].is<int>()) {
		iSpindelRssi = localDoc["RSSI"].as<int>();
	} else if (localDoc["rssi"].is<int>()) {
		iSpindelRssi = localDoc["rssi"].as<int>();
	}
	if (localDoc["gravity"].is<float>()) {
		iSpindelPlato = localDoc["gravity"].as<float>();
		recordPlatoReading(iSpindelPlato);
	}
	uint32_t reportedInterval = 0;
	if (localDoc["interval"].is<int>()) {
		int repInterval = localDoc["interval"].as<int>();
		if (repInterval >= 30 && repInterval <= 86400) {
			iSpindelIntervalSec = repInterval;
			reportedInterval = static_cast<uint32_t>(repInterval);
		}
	}

	if (clockValid()) {
		TelemetryISpindelRecord record = {};
		record.t = epochSeconds();
		record.interval = reportedInterval;
		record.temperature = localDoc["temperature"].is<float>()
			? telemetryFixedPoint(localDoc["temperature"].as<float>(), 100) : INT16_MIN;
		record.gravity = localDoc["gravity"].is<float>()
			? telemetryFixedPoint(localDoc["gravity"].as<float>(), 1000) : INT16_MIN;
		record.angle = localDoc["angle"].is<float>()
			? telemetryFixedPoint(localDoc["angle"].as<float>(), 100) : INT16_MIN;
		record.battery = localDoc["battery"].is<float>()
			? telemetryFixedPoint(localDoc["battery"].as<float>(), 100) : INT16_MIN;
		if (localDoc["RSSI"].is<int>()) {
			int rssi = localDoc["RSSI"].as<int>();
			record.rssi = static_cast<int8_t>(constrain(rssi, INT8_MIN + 1, INT8_MAX));
		} else if (localDoc["rssi"].is<int>()) {
			int rssi = localDoc["rssi"].as<int>();
			record.rssi = static_cast<int8_t>(constrain(rssi, INT8_MIN + 1, INT8_MAX));
		} else {
			record.rssi = INT8_MIN;
		}
		record.stableHours = isFermentationComplete() ? -1 : static_cast<int8_t>(min<int>(getPlatoStableHours(), 127));
		telemetryISpindelPush(record);
		sendTelemetryNow = true;
	}
	if (isBootAutoStartPending()) {
		cancelBootAutoStart();
		setCoolerStopped(LOW);
	}
	updateCascadeTarget();
	httpServer.send(200, "text/plain", "OK");

#ifdef WIFI_DEBUG
	String clientMac = getISpindelClientMac();
	String logMsg = F("iSpindel data received: MAC=");
	logMsg += (clientMac.length() > 0 ? clientMac : F("unknown"));
	if (iSpindelRssi != 0) {
		logMsg += F(", RSSI=");
		logMsg += String(iSpindelRssi);
		logMsg += F("dBm");
	}
	Debug.addLine(logMsg);
#else
	Debug.addLine(F("iSpindel data received"));
#endif
}
