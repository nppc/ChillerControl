#include "CoolerBox.h"

#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

void sendTelemetry() {
	if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
		return;
	}
	if (endpointAddress.isEmpty() || endpointAccessToken.isEmpty() || endpointPath.isEmpty()) {
		Debug.addLine(F("HTTPS endpoint settings missing"));
		return;
	}
	if (!endpointAddress.startsWith("https://")) {
		Debug.addLine(F("Endpoint address must use https://"));
		return;
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
		hasNewISpindelData = false;
	}

	String payload;
	serializeJson(doc, payload);

	BearSSL::WiFiClientSecure client;
	client.setInsecure();
	client.setBufferSizes(1024, 512); // Limit TLS buffer sizes to save RAM and avoid heap exhaustion
	client.setTimeout(2500);          // 2.5s timeout on TLS socket operations

	HTTPClient https;
	https.setTimeout(2500);           // 2.5s HTTP timeout to prevent blocking main loop on weak signal
	if (!https.begin(client, url)) {
		Debug.addLine(F("HTTPS endpoint setup failed"));
		return;
	}
	https.addHeader("Content-Type", "application/json");
	int status = https.POST(payload);
	if (status < 0) {
		Debug.addLine(String(F("HTTPS POST failed: ")) + https.errorToString(status));
	} else if (status < 200 || status >= 300) {
		Debug.addLine(String(F("HTTPS endpoint returned: ")) + status);
	}
	https.end();
}
