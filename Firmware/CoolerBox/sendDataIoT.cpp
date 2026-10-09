#include "CoolerBox.h"

#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

void sendTelemetry() {
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

	JsonDocument doc;
	doc["temperature"] = boxTemp;
	doc["set_temperature"] = setTemp;
	doc["hot_temperature"] = HotTemp;
	doc["voltage"] = measuredVoltage;
	doc["current"] = measuredCurrent;
	doc["hot_pwm"] = setHotPWM * 100.0 / maxHotPWM;
	doc["ip_address"] = WiFi.localIP().toString();

	String payload;
	serializeJson(doc, payload);

	BearSSL::WiFiClientSecure client;
	client.setInsecure();
	HTTPClient https;
	if (!https.begin(client, url)) {
		Debug.addLine(F("HTTPS endpoint setup failed"));
		return;
	}
	https.addHeader("Content-Type", "application/json");
	int status = https.POST(payload);
	if (status >= 200 && status < 300) {
		Debug.addLine(String(F("HTTPS telemetry sent: ")) + status);
	} else if (status < 0) {
		Debug.addLine(String(F("HTTPS POST failed: ")) + https.errorToString(status));
	} else {
		Debug.addLine(String(F("HTTPS endpoint returned: ")) + status);
	}
	https.end();
}
