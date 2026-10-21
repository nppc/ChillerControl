#include "CoolerBox.h"

#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <limits.h>
#include <stdarg.h>

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
unsigned long lastSkipLogMillis[6] = {};
bool skipLogged[6] = {};
char recordBuffer[320];
char heldRecord[320];

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
	}
}

void setUploadBackoff() {
	if (consecutiveUploadFailures < TELEMETRY_BACKOFF_LEVEL_COUNT) {
		++consecutiveUploadFailures;
	}
	nextAllowedSend = millis() +
		static_cast<unsigned long>(telemetryBackoffLevelMinutes()) * 60UL * 1000UL;
	failureBackoffActive = true;
}

int postTelemetryPayload(const String &url, const String &payload) {
	unsigned long t0 = millis();
	Debug.addLine(String(F("POST start: len=")) + payload.length() +
	              F(" hp=") + ESP.getFreeHeap() +
	              F(" blk=") + ESP.getMaxFreeBlockSize() +
	              F(" stk=") + ESP.getFreeContStack());

	BearSSL::WiFiClientSecure client;
	client.setInsecure();
	client.setBufferSizes(1024, 512);
	client.setTimeout(1500);

	HTTPClient https;
	https.setTimeout(1500);
	if (!https.begin(client, url)) {
		Debug.addLine(F("HTTPS endpoint setup failed"));
		return -1;
	}

	https.addHeader("Content-Type", "application/json");
	https.addHeader("Connection", "close");
	int status = https.POST(payload);
	unsigned long duration = millis() - t0;

	Debug.addLine(String(F("POST done: st=")) + status +
	              F(" in ") + duration + F("ms hp=") + ESP.getFreeHeap() +
	              F(" stk=") + ESP.getFreeContStack());

	if (status < 0) {
		Debug.addLine(String(F("HTTPS POST failed: ")) + https.errorToString(status));
	} else if (status < 200 || status >= 300) {
		Debug.addLine(String(F("HTTPS endpoint returned: ")) + status);
	}
	https.end();
	return status;
}

bool appendFormat(char *buffer, size_t capacity, size_t &length, const char *format, ...) {
	if (length >= capacity) {
		return false;
	}
	va_list args;
	va_start(args, format);
	int written = vsnprintf(buffer + length, capacity - length, format, args);
	va_end(args);
	if (written < 0 || static_cast<size_t>(written) >= capacity - length) {
		return false;
	}
	length += static_cast<size_t>(written);
	return true;
}

void formatFixed(char *output, size_t capacity, int16_t value, uint32_t scale, uint8_t digits) {
	int32_t signedValue = value;
	uint32_t magnitude = static_cast<uint32_t>(signedValue < 0 ? -signedValue : signedValue);
	snprintf(output, capacity, "%s%lu.%0*lu", signedValue < 0 ? "-" : "",
	         static_cast<unsigned long>(magnitude / scale), digits,
	         static_cast<unsigned long>(magnitude % scale));
}

bool appendFixedField(char *buffer, size_t capacity, size_t &length, bool &hasField,
	                  const char *key, int16_t value, uint32_t scale, uint8_t digits) {
	if (value == INT16_MIN) {
		return true;
	}
	char fixed[20];
	formatFixed(fixed, sizeof(fixed), value, scale, digits);
	if (!appendFormat(buffer, capacity, length, "%s\"%s\":%s",
	                  hasField ? "," : "", key, fixed)) {
		return false;
	}
	hasField = true;
	return true;
}

bool formatISpindelRecord(char *buffer, size_t capacity, const TelemetryISpindelRecord &record) {
	size_t length = 0;
	bool hasField = false;
	if (!appendFormat(buffer, capacity, length, "{\"ts\":%lu000,\"values\":{",
	                  static_cast<unsigned long>(record.t)) ||
	    !appendFixedField(buffer, capacity, length, hasField,
	                      "ispindel_temperature", record.temperature, 100, 2) ||
	    !appendFixedField(buffer, capacity, length, hasField,
	                      "ispindel_gravity", record.gravity, 1000, 3) ||
	    !appendFixedField(buffer, capacity, length, hasField,
	                      "ispindel_battery", record.battery, 100, 2) ||
	    !appendFixedField(buffer, capacity, length, hasField,
	                      "ispindel_angle", record.angle, 100, 2)) {
		return false;
	}
	if (record.rssi != INT8_MIN &&
	    !appendFormat(buffer, capacity, length, "%s\"ispindel_rssi\":%d",
	                  hasField ? "," : "", static_cast<int>(record.rssi))) {
		return false;
	}
	if (record.rssi != INT8_MIN) {
		hasField = true;
	}
	if (record.interval != 0 &&
	    !appendFormat(buffer, capacity, length, "%s\"ispindel_interval\":%lu",
	                  hasField ? "," : "", static_cast<unsigned long>(record.interval))) {
		return false;
	}
	return appendFormat(buffer, capacity, length, "}}");
}

bool formatBoxRecord(char *buffer, size_t capacity, const TelemetryBoxRecord &record,
	                 const IPAddress &ipAddress, bool includeIpAddress) {
	size_t length = 0;
	bool hasField = false;
	if (!appendFormat(buffer, capacity, length, "{\"ts\":%lu000,\"values\":{",
	                  static_cast<unsigned long>(record.t)) ||
	    !appendFixedField(buffer, capacity, length, hasField, "box_temperature",
	                      record.box_temperature, 100, 2) ||
	    !appendFixedField(buffer, capacity, length, hasField, "box_target",
	                      record.box_target, 100, 2) ||
	    !appendFixedField(buffer, capacity, length, hasField, "fermenter_temperature",
	                      record.fermenter_temperature, 100, 2) ||
	    !appendFixedField(buffer, capacity, length, hasField, "set_temperature",
	                      record.set_temperature, 100, 2) ||
	    !appendFixedField(buffer, capacity, length, hasField, "hot_temperature",
	                      record.hot_temperature, 100, 2) ||
	    !appendFixedField(buffer, capacity, length, hasField, "voltage",
	                      record.voltage, 100, 2) ||
	    !appendFixedField(buffer, capacity, length, hasField, "current",
	                      record.current, 100, 2) ||
	    !appendFixedField(buffer, capacity, length, hasField, "pwm",
	                      record.pwm, 100, 2)) {
		return false;
	}
	if (!appendFormat(buffer, capacity, length, "%s\"state\":\"%s\"",
	                  hasField ? "," : "",
	                  telemetryStateToString(static_cast<TelemetryState>(record.state)))) {
		return false;
	}
	if (record.rssi != INT8_MIN &&
	    !appendFormat(buffer, capacity, length, ",\"rssi\":%d", static_cast<int>(record.rssi))) {
		return false;
	}
	if (includeIpAddress &&
	    !appendFormat(buffer, capacity, length, ",\"ip_address\":\"%u.%u.%u.%u\"",
	                  ipAddress[0], ipAddress[1], ipAddress[2], ipAddress[3])) {
		return false;
	}
	return appendFormat(buffer, capacity, length, "}}");
}

bool canAppendRecord(const String &payload, const char *record, bool hasPrevious) {
	size_t needed = payload.length() + (hasPrevious ? 1 : 0) + strlen(record) + 1;
	return needed <= TELEMETRY_MAX_PAYLOAD;
}

bool canAppendTwoRecords(const String &payload, const char *first, const char *second) {
	size_t needed = payload.length() + 1 + strlen(first) + 1 + strlen(second) + 1;
	return needed <= TELEMETRY_MAX_PAYLOAD;
}

bool appendPayloadRecord(String &payload, const char *record, bool hasPrevious) {
	return (!hasPrevious || payload.concat(',')) && payload.concat(record);
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
			logUploadSkipped(0, F("Skip: failure backoff"));
		}
		return false;
	}
	if (httpServer.client().connected()) {
		logUploadSkipped(1, F("Skip: HTTP client connected"));
		return false;
	}
	if (lastISpindelMillis != 0) {
		unsigned long intervalMs = static_cast<unsigned long>(iSpindelIntervalSec) * 1000UL;
		unsigned long sinceLastPacket = millis() - lastISpindelMillis;
		if (isInISpindelWakeWindow(sinceLastPacket, intervalMs)) {
			logUploadSkipped(3, F("Skip: iSpindel wake window"));
			return false;
		}
	}
	if (!clockValid() || telemetryQueuePending() == 0) {
		return false;
	}

	if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
		logUploadSkipped(4, F("Skip: WiFi disconnected or no IP"));
		return false;
	}
	if (endpointAddress.isEmpty() || endpointAccessToken.isEmpty() || endpointPath.isEmpty()) {
		logUploadSkipped(5, F("Skip: Endpoint settings missing"));
		return false;
	}
	if (!endpointAddress.startsWith("https://")) {
		Debug.addLine(F("Endpoint address must use https://"));
		return false;
	}

	Debug.addLine(String(F("TX prep: box=")) + telemetryBoxPending() +
	              F(" isp=") + telemetryISpindelPending() +
	              F(" hp=") + ESP.getFreeHeap() +
	              F(" pcb=") + getTcpActivePcbCount() + F("/") + getTcpTwPcbCount() +
	              F(" stk=") + ESP.getFreeContStack());

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

	String payload;
	if (!payload.reserve(TELEMETRY_MAX_PAYLOAD)) {
		Debug.addLine(F("Telemetry payload allocation failed"));
		nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
		return false;
	}
	if (!payload.concat('[')) {
		Debug.addLine(F("Telemetry payload allocation failed"));
		nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
		return false;
	}

	uint8_t iSpindelCount = 0;
	uint8_t boxCount = 0;
	bool stoppedOnPayloadLimit = false;

	for (uint8_t i = 0; i < telemetryISpindelPending(); ++i) {
		if (!formatISpindelRecord(recordBuffer, sizeof(recordBuffer), telemetryISpindelPeek(i))) {
			Debug.addLine(F("Telemetry record formatting failed"));
			nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
			return false;
		}
		if (!canAppendRecord(payload, recordBuffer, iSpindelCount > 0) ||
		    !appendPayloadRecord(payload, recordBuffer, iSpindelCount > 0)) {
			stoppedOnPayloadLimit = true;
			break;
		}
		++iSpindelCount;
	}

	IPAddress ipAddress = WiFi.localIP();
	if (!stoppedOnPayloadLimit) {
		uint8_t boxLimit = telemetryBoxPending();
		if (boxLimit > TELEMETRY_CHUNK_MAX) {
			boxLimit = TELEMETRY_CHUNK_MAX;
		}

		bool hasHeldRecord = false;
		for (uint8_t i = 0; i < boxLimit; ++i) {
			const TelemetryBoxRecord &boxRecord = telemetryBoxPeek(i);
			if (!hasHeldRecord) {
				if (!formatBoxRecord(recordBuffer, sizeof(recordBuffer), boxRecord, ipAddress, true) ||
				    !canAppendRecord(payload, recordBuffer, iSpindelCount > 0)) {
					stoppedOnPayloadLimit = true;
					break;
				}
				if (!formatBoxRecord(heldRecord, sizeof(heldRecord), boxRecord, ipAddress, false)) {
					Debug.addLine(F("Telemetry record formatting failed"));
					nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
					return false;
				}
				hasHeldRecord = true;
				continue;
			}

			if (!formatBoxRecord(recordBuffer, sizeof(recordBuffer), boxRecord, ipAddress, true)) {
				Debug.addLine(F("Telemetry record formatting failed"));
				nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
				return false;
			}
			if (canAppendTwoRecords(payload, heldRecord, recordBuffer)) {
				if (!appendPayloadRecord(payload, heldRecord, iSpindelCount + boxCount > 0)) {
					Debug.addLine(F("Telemetry payload append failed"));
					nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
					return false;
				}
				++boxCount;
				if (!formatBoxRecord(heldRecord, sizeof(heldRecord), boxRecord, ipAddress, false)) {
					Debug.addLine(F("Telemetry record formatting failed"));
					nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
					return false;
				}
				continue;
			}

			if (!formatBoxRecord(recordBuffer, sizeof(recordBuffer),
			                     telemetryBoxPeek(boxCount), ipAddress, true) ||
			    !canAppendRecord(payload, recordBuffer, iSpindelCount + boxCount > 0) ||
			    !appendPayloadRecord(payload, recordBuffer, iSpindelCount + boxCount > 0)) {
				Debug.addLine(F("Telemetry payload append failed"));
				nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
				return false;
			}
			++boxCount;
			hasHeldRecord = false;
			stoppedOnPayloadLimit = true;
			break;
		}

		if (hasHeldRecord) {
			if (!formatBoxRecord(recordBuffer, sizeof(recordBuffer),
			                     telemetryBoxPeek(boxCount), ipAddress, true) ||
			    !canAppendRecord(payload, recordBuffer, iSpindelCount + boxCount > 0) ||
			    !appendPayloadRecord(payload, recordBuffer, iSpindelCount + boxCount > 0)) {
				Debug.addLine(F("Telemetry payload append failed"));
				nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
				return false;
			}
			++boxCount;
		}
	}

	if (iSpindelCount == 0 && boxCount == 0) {
		nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
		return false;
	}
	if (!payload.concat(']')) {
		Debug.addLine(F("Telemetry payload append failed"));
		nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
		return false;
	}

	logUploadAttemptBlock();
	uint32_t currentMaxBlock = ESP.getMaxFreeBlockSize();
	if (currentMaxBlock < TELEMETRY_MIN_FREE_BLOCK) {
		logUploadSkipped(2, F("Skip: low heap block"));
		Debug.addLine(String(F("Low blk=")) + currentMaxBlock + F(" < ") + TELEMETRY_MIN_FREE_BLOCK +
		              F(" hp=") + ESP.getFreeHeap() + F(" stk=") + ESP.getFreeContStack());
		nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
		return false;
	}

	int status = postTelemetryPayload(url, payload);
	if (status >= 200 && status < 300) {
		failureBackoffActive = false;
		consecutiveUploadFailures = 0;
		telemetryISpindelPop(iSpindelCount);
		telemetryBoxPop(boxCount);
		telemetryQueueReset();
		Debug.addLine(String(F("TX OK: sent box=")) + boxCount + F(" isp=") + iSpindelCount +
		              F(" rem box=") + telemetryBoxPending() + F(" isp=") + telemetryISpindelPending() +
		              F(" pcb=") + getTcpActivePcbCount() + F("/") + getTcpTwPcbCount());
		if (telemetryQueuePending() > 0) {
			nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
		}
		return true;
	}

	if (status == 400 || status == 413 || status == 422) {
		Debug.addLine(String(F("dropped chunk, HTTP ")) + status);
		telemetryISpindelPop(iSpindelCount);
		telemetryBoxPop(boxCount);
		if (telemetryQueuePending() > 0) {
			nextAllowedSend = millis() + TELEMETRY_CHUNK_GAP_MS;
		}
		return false;
	}

	setUploadBackoff();
	Debug.addLine(String(F("TX fail: backoff=")) + telemetryBackoffLevelMinutes() +
	              F("m rem box=") + telemetryBoxPending() + F(" isp=") + telemetryISpindelPending());
	return false;
}
