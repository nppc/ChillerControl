#include "CoolerBox.h"

void handleStatus() {
	unsigned long val = millis();
	int tdays = val / 86400000;
	unsigned long sec = val / 1000;
	unsigned long min = sec / 60;
	int hr = min / 60;

	char buf[24];
	snprintf(buf, sizeof(buf), "%d days, %02d:%02lu:%02lu", tdays, hr % 24, min % 60, sec % 60);

	JsonDocument doc;
	doc["Caption"] = emergencyMode ? "Main Info (Emergency AP)" : "Main Info";
	doc["CurTime"] = buf;
	const bool wifiConnected = WiFi.status() == WL_CONNECTED;
	doc["wifiConnected"] = wifiConnected;
	if (wifiConnected) {
		doc["rssi"] = WiFi.RSSI();
	}
	const bool stopped = isCoolerStopped();
	const bool autoTune = isHeatingAutoTuning();
	const bool heatingMode = autoTune || boxMode == 3 || (boxMode == 1 && boxSubMode == 3);
	const bool heatingWaiting = heatingMode && !stopped && !autoTune && I2C_PRESENT &&
		measuredVoltage >= max(2.5f, minVoltage + 0.5f);
	doc["stopping"] = stopped;
	doc["autoTuning"] = autoTune;
	doc["controlMode"] = boxMode == 1 ? "Automatic" : boxMode == 2 ? "Cooling only" : "Heating only";
	doc["activeProcess"] = autoTune ? "heating" : stopped ? "stopped" : heatingMode ? "heating" : "cooling";
	doc["processDetail"] = autoTune ? getHeatingAutoTuneStatus().message :
		stopped ? "Stopped" : heatingWaiting ? "Waiting for safe voltage" :
		heatingMode ? (setHotPWM > 0.0f ? "Heating" : "Holding target") :
		(isCoolingDemanded() ? "Cooling" : "Holding target");
	doc["I2CStatus"] = I2C_PRESENT ? "I2C connected" : "I2C connection failed";
	doc["HotTemp"] = String(HotTemp, 1);
	doc["boxTemp"] = String(boxTemp, 1);
	doc["setHotPWM"] = String((int)round(setHotPWM * 100.0f / (float)maxHotPWM)) + "%";
	doc["setTemp"] = String(setTemp, 1);
	doc["rawSetTemp"] = String(setTemp, 1);
	doc["Voltage"] = String(measuredVoltage, 1) + "V";
	doc["Current"] = String(measuredCurrent, 1) + "A";
	doc["Fan"] = digitalRead(HOT_FAN) ? "On" : "Off";

	String json;
	serializeJson(doc, json);
	httpServer.send(200, "application/json", json);
}
