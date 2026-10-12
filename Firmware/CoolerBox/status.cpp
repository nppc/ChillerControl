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
	doc["FW"] = FIRMWAREVERSION;
	doc["CurTime"] = buf;
	doc["I2CStatus"] = I2C_PRESENT ? "I2C connected" : "I2C connection failed";
	doc["HotTemp"] = String(HotTemp, 1);
	doc["boxTemp"] = String(boxTemp, 1);
	doc["setHotPWM"] = String((int)round(setHotPWM * 100.0f / (float)maxHotPWM)) + "%";
	doc["stopping"] = stopCooler;
	doc["setTemp"] = stopCooler ? "Stopping..." : String(setTemp, 1);
	doc["rawSetTemp"] = String(setTemp, 1);
	doc["Voltage"] = String(measuredVoltage, 1) + "V";
	doc["Current"] = String(measuredCurrent, 1) + "A";
	doc["Fan"] = digitalRead(HOT_FAN) ? "#ffd699" : "Transparent";

	String json;
	serializeJson(doc, json);
	httpServer.send(200, "application/json", json);
}
