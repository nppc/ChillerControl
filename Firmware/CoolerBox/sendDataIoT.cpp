#include "CoolerBox.h"

#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

class StringAppender : public Print {
public:
	explicit StringAppender(String &s) : s_(s) {}
	size_t write(uint8_t c) override { return s_.concat(static_cast<char>(c)) ? 1 : 0; }
	size_t write(const uint8_t *b, size_t n) override {
		return s_.concat(reinterpret_cast<const char *>(b), n) ? n : 0;
	}
private:
	String &s_;
};

namespace {
const unsigned long TELEMETRY_SKIP_LOG_INTERVAL_MS = 60UL * 1000UL;
const uint8_t TELEMETRY_BACKOFF_LEVELS_MINUTES[] = {1, 2, 5, 10};
const uint8_t TELEMETRY_BACKOFF_LEVEL_COUNT =
	sizeof(TELEMETRY_BACKOFF_LEVELS_MINUTES) / sizeof(TELEMETRY_BACKOFF_LEVELS_MINUTES[0]);
const unsigned long ISPINDEL_WAKE_WINDOW_BEFORE_MS = 15000UL;
const unsigned long ISPINDEL_WAKE_WINDOW_AFTER_MS = 10000UL;
const uint8_t ISPINDEL_WAKE_WINDOW_CYCLES = 3;

unsigned long nextAllowedSend = 0;
bool failureBackoffActive = false;
uint8_t consecutiveUploadFailures = 0;
unsigned long lastSkipLogMillis[4] = {};
bool skipLogged[4] = {};

bool isInISpindelWakeWindow(unsigned long since, unsigned long interval) {
	if (interval == 0 || interval <= ISPINDEL_WAKE_WINDOW_BEFORE_MS ||
	    since < interval - ISPINDEL_WAKE_WINDOW_BEFORE_MS ||
	    since > static_cast<unsigned long>(ISPINDEL_WAKE_WINDOW_CYCLES) * interval +
	                ISPINDEL_WAKE_WINDOW_AFTER_MS) {
		return false;
	}

	// Modulo covers accumulated packet-timer drift, limited to the first three expected wakes.
	unsigned long phase = since % interval;
	return phase >= interval - ISPINDEL_WAKE_WINDOW_BEFORE_MS ||
	       phase <= ISPINDEL_WAKE_WINDOW_AFTER_MS;
}

void logUploadSkipped(uint8_t reason, const __FlashStringHelper *message) {
	unsigned long now = millis();
	if (reason < sizeof(skipLogged) / sizeof(skipLogged[0])) {
		if (!skipLogged[reason] ||
		    static_cast<unsigned long>(now - lastSkipLogMillis[reason]) >= TELEMETRY_SKIP_LOG_INTERVAL_MS) {
			Debug.addLine(message);
			lastSkipLogMillis[reason] = now;
			skipLogged[reason] = true;
		}
	}
}

void logUploadAttemptBlock() {
	unsigned long now = millis();
	static unsigned long lastBlockLogMillis = 0;
	if (lastBlockLogMillis == 0 ||
	    static_cast<unsigned long>(now - lastBlockLogMillis) >= TELEMETRY_SKIP_LOG_INTERVAL_MS) {
		lastBlockLogMillis = now;
		Debug.addLine(String(F("Telemetry upload attempt: max free block ")) + ESP.getMaxFreeBlockSize() + F(" B"));
	}
}

void setUploadBackoff() {
	if (consecutiveUploadFailures < TELEMETRY_BACKOFF_LEVEL_COUNT) {
		++consecutiveUploadFailures;
	}
	uint8_t backoffMinutes = telemetryBackoffLevelMinutes();
	nextAllowedSend = millis() + static_cast<unsigned long>(backoffMinutes) * 60UL * 1000UL;
	failureBackoffActive = true;
}

bool postTelemetryPayload(const String &url, const String &payload) {
	BearSSL::WiFiClientSecure client;
	client.setInsecure();
	client.setBufferSizes(1024, 512); // Limit TLS buffer sizes to save RAM and avoid heap exhaustion
	client.setTimeout(1500);          // Bound blocking time on weak links.

	HTTPClient https;
	https.setTimeout(1500);
	if (!https.begin(client, url)) {
		Debug.addLine(F("HTTPS endpoint setup failed"));
		setUploadBackoff();
		return false;
	}
	https.addHeader("Content-Type", "application/json");
	int status = https.POST(payload);
	if (status < 0) {
		Debug.addLine(String(F("HTTPS POST failed: ")) + https.errorToString(status));
		setUploadBackoff();
	} else if (status < 200 || status >= 300) {
		Debug.addLine(String(F("HTTPS endpoint returned: ")) + status);
		setUploadBackoff();
	}
	https.end();
	return status >= 200 && status < 300;
}
}

uint8_t telemetryBackoffLevelMinutes() {
	if (consecutiveUploadFailures == 0) {
		return 0;
	}
	uint8_t index = consecutiveUploadFailures - 1;
	if (index >= TELEMETRY_BACKOFF_LEVEL_COUNT) {
		index = TELEMETRY_BACKOFF_LEVEL_COUNT - 1;
	}
	return TELEMETRY_BACKOFF_LEVELS_MINUTES[index];
}

bool isTelemetryUploadDue() {
	if (!clockValid() || telemetryQueuePending() == 0) {
		return false;
	}
	if (static_cast<long>(millis() - nextAllowedSend) < 0) {
		return false;
	}
	if (httpServer.client().connected()) {
		return false;
	}
	return true;
}

bool sendTelemetry() {
	if (static_cast<long>(millis() - nextAllowedSend) < 0) {
		if (failureBackoffActive) {
			logUploadSkipped(0, F("Telemetry upload skipped: failure backoff"));
		}
		return false;
	}
	if (httpServer.client().connected()) {
		return false;
	}
	if (lastISpindelMillis != 0) {
		unsigned long intervalMs = static_cast<unsigned long>(iSpindelIntervalSec) * 1000UL;
		unsigned long sinceLastPacket = millis() - lastISpindelMillis;
		if (isInISpindelWakeWindow(sinceLastPacket, intervalMs)) {
			logUploadSkipped(3, F("Telemetry upload skipped: iSpindel wake window"));
			return false;
		}
	}

	if (!clockValid()) {
		millisSendDataInterval = millis();
	} else if (telemetryQueuePending() == 0) {
		return false;
	}

	if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
		return false;
	}
	if (endpointAddress.isEmpty() || endpointAccessToken.isEmpty() || endpointPath.isEmpty()) {
		Debug.addLine(F("HTTPS endpoint settings missing"));
		return false;
	}
	if (!endpointAddress.startsWith("https://")) {
		Debug.addLine(F("Endpoint address must use https://"));
		return false;
	}

	String url = endpointAddress;
	while (url.endsWith("/")) {
		url.remove(url.length() - 1);
	}
	url += "/";
	url += endpointAccessToken;
	if (!endpointPath.startsWith("/")) {
		url += "/";
	}
	url += endpointPath;

	if (clockValid()) {
		String payload;
		uint8_t recordsToSend = telemetryQueuePending();
		if (recordsToSend > TELEMETRY_CHUNK_MAX) {
			recordsToSend = TELEMETRY_CHUNK_MAX;
		}
		bool chunkIncludedISpindel = false;

		while (recordsToSend > 0) {
			payload = "";
			payload.reserve(TELEMETRY_MAX_PAYLOAD);
			payload = '[';
			chunkIncludedISpindel = false;

			for (uint8_t i = 0; i < recordsToSend; ++i) {
				if (i > 0) {
					payload += ',';
				}
				const TelemetrySnapshot &snap = telemetryQueuePeek(i);
				char tsBuf[16];
				snprintf(tsBuf, sizeof(tsBuf), "%lu000", static_cast<unsigned long>(snap.t));

				JsonDocument recDoc;
				recDoc["ts"] = serialized(tsBuf);
				JsonObject values = recDoc["values"].to<JsonObject>();

				if (!isnan(snap.box_temperature)) values["box_temperature"] = snap.box_temperature;
				if (!isnan(snap.box_target)) values["box_target"] = snap.box_target;
				if (!isnan(snap.fermenter_temperature)) values["fermenter_temperature"] = snap.fermenter_temperature;
				values["state"] = telemetryStateToString(static_cast<TelemetryState>(snap.state));
				if (!isnan(snap.set_temperature)) values["set_temperature"] = snap.set_temperature;
				if (!isnan(snap.hot_temperature)) values["hot_temperature"] = snap.hot_temperature;
				if (!isnan(snap.voltage)) values["voltage"] = snap.voltage;
				if (!isnan(snap.current)) values["current"] = snap.current;
				if (!isnan(snap.hot_pwm)) values["hot_pwm"] = snap.hot_pwm;
				if (!isnan(snap.pwm)) values["pwm"] = snap.pwm;
				if (i == recordsToSend - 1) {
					values["ip_address"] = WiFi.localIP().toString();
				}
				if (!isnan(snap.rssi)) values["rssi"] = static_cast<int>(snap.rssi);

				if (!isnan(snap.ispindel_temperature)) {
					values["ispindel_temperature"] = snap.ispindel_temperature;
					chunkIncludedISpindel = true;
				}
				if (!isnan(snap.ispindel_gravity)) {
					values["ispindel_gravity"] = snap.ispindel_gravity;
					chunkIncludedISpindel = true;
				}
				if (!isnan(snap.ispindel_battery)) {
					values["ispindel_battery"] = snap.ispindel_battery;
					chunkIncludedISpindel = true;
				}
				if (!isnan(snap.ispindel_rssi)) {
					values["ispindel_rssi"] = static_cast<int>(snap.ispindel_rssi);
					chunkIncludedISpindel = true;
				}

				StringAppender out(payload);
				serializeJson(recDoc, out);
			}
			payload += ']';

			if (payload.length() <= TELEMETRY_MAX_PAYLOAD || recordsToSend == 1) {
				break;
			}
			--recordsToSend;
		}

		logUploadAttemptBlock();
		if (ESP.getMaxFreeBlockSize() < TELEMETRY_MIN_FREE_BLOCK) {
			logUploadSkipped(2, F("Telemetry upload skipped: heap block too small"));
			nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
			return false;
		}

		if (postTelemetryPayload(url, payload)) {
			failureBackoffActive = false;
			consecutiveUploadFailures = 0;
			telemetryQueuePop(recordsToSend);
			telemetryQueueIncrementChunksSent();
			if (chunkIncludedISpindel) {
				hasNewISpindelData = false;
			}
			if (telemetryQueuePending() > 0) {
				nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
			}
			return true;
		}
		return false;
	} else {
		float targetVal = isHeatingAutoTuning() ? getHeatingAutoTuneTargetTemp() : setTemp;
		float pwmPct = (maxHotPWM > 0) ? (setHotPWM * 100.0f / static_cast<float>(maxHotPWM)) : 0.0f;
		const char *currentState = setHotPWM > 0.0f ? "heating" :
			(!isCoolerStopped() && isCoolingDemanded() ? "cooling" : "nothing");

		JsonDocument doc;
		doc["box_temperature"] = boxTemp;
		doc["box_target"] = effectiveBoxTarget;
		doc["fermenter_temperature"] = getFermenterTemp();
		doc["state"] = currentState;
		doc["set_temperature"] = targetVal;
		doc["hot_temperature"] = HotTemp;
		doc["voltage"] = measuredVoltage;
		doc["current"] = measuredCurrent;
		doc["hot_pwm"] = pwmPct;
		doc["pwm"] = pwmPct;
		doc["ip_address"] = WiFi.localIP().toString();
		doc["rssi"] = WiFi.RSSI();

		if (hasNewISpindelData) {
			// Forward all iSpindel fields from typed globals — no live JsonDocument needed
			doc["ispindel_temperature"] = iSpindelTemperature;
			doc["ispindel_gravity"]     = iSpindelGravity;
			doc["ispindel_battery"]     = iSpindelBattery;
			doc["ispindel_rssi"]        = iSpindelRssi;
			// Forward any extra fields captured verbatim at receive time
			for (uint8_t i = 0; i < iSpindelExtraCount; ++i) {
				// Values were serialised as JSON; deserialise them back so they retain
				// their original type (number, boolean, string) in the outgoing payload.
				JsonDocument valDoc;
				if (deserializeJson(valDoc, iSpindelExtraValues[i]) == DeserializationError::Ok) {
					doc[iSpindelExtraKeys[i]] = valDoc.as<JsonVariant>();
				}
			}
		}

		String payload;
		serializeJson(doc, payload);

		logUploadAttemptBlock();
		if (ESP.getMaxFreeBlockSize() < TELEMETRY_MIN_FREE_BLOCK) {
			logUploadSkipped(2, F("Telemetry upload skipped: heap block too small"));
			return false;
		}

		if (postTelemetryPayload(url, payload)) {
			failureBackoffActive = false;
			consecutiveUploadFailures = 0;
			if (hasNewISpindelData) {
				hasNewISpindelData = false;
			}
			return true;
		}
		return false;
	}
}
