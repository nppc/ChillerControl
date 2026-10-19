#include "CoolerBox.h"

#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

namespace {
const unsigned long TELEMETRY_SKIP_LOG_INTERVAL_MS = 60UL * 1000UL;
const uint8_t TELEMETRY_BACKOFF_LEVELS_MINUTES[] = {1, 2, 5, 10};
const uint8_t TELEMETRY_BACKOFF_LEVEL_COUNT =
	sizeof(TELEMETRY_BACKOFF_LEVELS_MINUTES) / sizeof(TELEMETRY_BACKOFF_LEVELS_MINUTES[0]);

unsigned long nextAllowedSend = 0;
bool failureBackoffActive = false;
uint8_t consecutiveUploadFailures = 0;
unsigned long lastSkipLogMillis[4] = {};
bool skipLogged[4] = {};

void logUploadSkipped(uint8_t reason, const __FlashStringHelper *message) {
	unsigned long now = millis();
	if (!skipLogged[reason] ||
	    static_cast<unsigned long>(now - lastSkipLogMillis[reason]) >= TELEMETRY_SKIP_LOG_INTERVAL_MS) {
		Debug.addLine(message);
		lastSkipLogMillis[reason] = now;
		skipLogged[reason] = true;
	}
}

void setUploadBackoff() {
	if (consecutiveUploadFailures < TELEMETRY_BACKOFF_LEVEL_COUNT) {
		++consecutiveUploadFailures;
	}
	uint8_t backoffMinutes = telemetryBackoffLevelMinutes();
	nextAllowedSend = millis() + static_cast<unsigned long>(backoffMinutes) * 60UL * 1000UL;
	failureBackoffActive = true;
	recordLinkDiagnostic(LinkDiagnosticEvent::UploadFailed, backoffMinutes);
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

bool sendTelemetry() {
	if (failureBackoffActive && static_cast<long>(millis() - nextAllowedSend) < 0) {
		logUploadSkipped(0, F("Telemetry upload skipped: failure backoff"));
		return false;
	}
	if (httpRequestInProgress) {
		logUploadSkipped(1, F("Telemetry upload skipped: HTTP request in progress"));
		return false;
	}
	if (lastISpindelMillis != 0) {
		unsigned long intervalMs = static_cast<unsigned long>(iSpindelIntervalSec) * 1000UL;
		unsigned long sinceLastPacket = millis() - lastISpindelMillis;
		unsigned long wakeWindowStart = intervalMs > 15000UL ? intervalMs - 15000UL : 0UL;
		unsigned long wakeWindowEnd = intervalMs + 10000UL;
		if (sinceLastPacket >= wakeWindowStart && sinceLastPacket <= wakeWindowEnd) {
			logUploadSkipped(3, F("Telemetry upload skipped: iSpindel wake window"));
			return false;
		}
	}

	// Keep periodic scheduling intact for deferred sends, but avoid tight retries for other early exits.
	millisSendDataInterval = millis();
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
		JsonObjectConst obj = lastISpindelDoc.as<JsonObjectConst>();
		for (JsonPairConst kv : obj) {
			String key = kv.key().c_str();
			key.toLowerCase();
			if (!key.startsWith("ispindel")) {
				key = "ispindel_" + key;
			}
			doc[key] = kv.value();
		}
	}

	String payload;
	serializeJson(doc, payload);

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
	} else {
		failureBackoffActive = false;
		consecutiveUploadFailures = 0;
		if (hasNewISpindelData) {
			hasNewISpindelData = false;
		}
		recordLinkDiagnostic(LinkDiagnosticEvent::UploadSucceeded, 0);
	}
	https.end();
	return status >= 200 && status < 300;
}
