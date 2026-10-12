#include "CoolerBox.h"

void handleRoot() {
	if (!handleFileRead("/index.htm")) {
		httpServer.send(404, "text/plain", "index.htm not found");
	}
}

void handleSettings() {
	if (!handleFileRead("/settings.htm")) {
		httpServer.send(404, "text/plain", "settings.htm not found");
	}
}

void handlePIDs() {
	if (!handleFileRead("/pids.htm")) {
		httpServer.send(404, "text/plain", "pids.htm not found");
	}
}

void handleNetwork() {
	if (!handleFileRead("/network.htm")) {
		httpServer.send(404, "text/plain", "network.htm not found");
	}
}

void handleDebug() {
	if (!handleFileRead("/debug.htm")) {
		httpServer.send(404, "text/plain", "debug.htm not found");
	}
}

void handleApiSettings() {
	restoreSettingsFromEEPROM();
	INIT_DS18B20();

	JsonDocument doc;
	doc["Caption"] = "Settings";
	doc["changeVoltageSpeed"] = changeVoltageSpeed;
	doc["minVoltage"] = minVoltage;
	doc["maxVoltage"] = maxVoltage;
	doc["fanDynamic_checked"] = fanDynamic_checked;
	doc["boxMode"] = boxMode;
	doc["ColdSensorId"] = ColdSensorId;
	doc["HotSensorId"] = HotSensorId;

	JsonArray sensors = doc["sensors"].to<JsonArray>();
	for (uint8_t x = 0; x < DS18B20_count; x++) {
		JsonObject s = sensors.add<JsonObject>();
		s["id"] = x;
		char hexAddr[17];
		for (uint8_t i = 0; i < 8; i++) {
			snprintf(&hexAddr[i * 2], 3, "%02X", DS18B20_adr[x][i]);
		}
		hexAddr[16] = '\0';
		s["addr"] = hexAddr;
		s["temp"] = DS18B20.getTempC(DS18B20_adr[x]);
	}

	String json;
	serializeJson(doc, json);
	httpServer.send(200, "application/json", json);
}

void handleApiPIDs() {
	JsonDocument doc;
	doc["Caption"] = "PID Settings";
	doc["hotPID_kP"] = hotPID_kP;
	doc["hotPID_kI"] = hotPID_kI;
	doc["hotPID_kD"] = hotPID_kD;

	String json;
	serializeJson(doc, json);
	httpServer.send(200, "application/json", json);
}

void handleApiNetwork() {
	JsonDocument doc;
	doc["Caption"] = emergencyMode ? "Network (Recovery Mode)" : "Network";
	doc["emergencyMode"] = emergencyMode;
	doc["apSSID"] = apSSID;
	doc["apPassword"] = apPassword;
	doc["IntSSID"] = IntSSID;
	doc["IntPASS"] = IntPASS;
	doc["endpointAddress"] = endpointAddress;
	doc["endpointAccessToken"] = endpointAccessToken;
	doc["endpointPath"] = endpointPath;
	doc["sendInterval"] = sendInterval;

	String json;
	serializeJson(doc, json);
	httpServer.send(200, "application/json", json);
}

void handleApiDebug() {
	JsonDocument doc;
	doc["Caption"] = "Debug";
	doc["debug"] = Debug.getHTML();

	String json;
	serializeJson(doc, json);
	httpServer.send(200, "application/json", json);
}

void handleSettingsStore() {
	if (httpServer.args() > 0) {
		fanDynamic_checked = 0;
		for (uint8_t i = 0; i < httpServer.args(); i++) {
			if (httpServer.argName(i) == "VoltChange") {
				float tmpVolt = httpServer.arg(i).toFloat();
				if (tmpVolt > 25.5f) { tmpVolt = changeVoltageSpeed; }
				if (tmpVolt < 0.4f) { tmpVolt = 0.4f; }
				changeVoltageSpeed = tmpVolt;
			}
			if (httpServer.argName(i) == "VoltMin") {
				float tmpVolt = httpServer.arg(i).toFloat();
				if (tmpVolt > 12.0f) { tmpVolt = minVoltage; }
				if (tmpVolt < 1.0f) { tmpVolt = 1.0f; }
				minVoltage = tmpVolt;
			}
			if (httpServer.argName(i) == "VoltMax") {
				float tmpVolt = httpServer.arg(i).toFloat();
				if (tmpVolt > 12.0f) { tmpVolt = maxVoltage; }
				if (tmpVolt < 1.0f) { tmpVolt = 1.0f; }
				maxVoltage = tmpVolt;
				maxVoltage_backup = maxVoltage;
			}
			if (httpServer.argName(i) == "SelColdSensor") {
				ColdSensorId = httpServer.arg(i).toInt();
			}
			if (httpServer.argName(i) == "SelHotSensor") {
				HotSensorId = httpServer.arg(i).toInt();
			}
			if (httpServer.argName(i) == "fanDynamic") {
				fanDynamic_checked = 1;
			}
			if (httpServer.argName(i) == "boxMode") {
				boxMode = httpServer.arg(i).toInt();
			}
		}
	}
	hotPID.SetOutputLimits(0, maxHotPWM);
	delay(50);
	updateCoolingOutput(true, true);
	delay(50);
	saveSettings();
	httpServer.sendHeader("Location", String("/settings"), true);
	httpServer.send(302, "text/plain", "");
}

void handlePIDsStore() {
	if (httpServer.args() > 0) {
		for (uint8_t i = 0; i < httpServer.args(); i++) {
			if (httpServer.argName(i) == "hotPID_kP") {
				hotPID_kP = httpServer.arg(i).toFloat();
			}
			if (httpServer.argName(i) == "hotPID_kI") {
				hotPID_kI = httpServer.arg(i).toFloat();
			}
			if (httpServer.argName(i) == "hotPID_kD") {
				hotPID_kD = httpServer.arg(i).toFloat();
			}
		}
	}
	hotPID.SetTunings(hotPID_kP, hotPID_kI, hotPID_kD);
	saveSettings();

	httpServer.sendHeader("Location", String("/pids"), true);
	httpServer.send(302, "text/plain", "");
}

void handleSetTemp() {
	if (httpServer.args() > 0) {
		for (uint8_t i = 0; i < httpServer.args(); i++) {
			if (httpServer.argName(i) == "Temp") {
				float tmpTemp = httpServer.arg(i).toFloat();
				if (tmpTemp > 45.0f) { tmpTemp = setTemp; }
				if (tmpTemp < 0.0f) { tmpTemp = 0.0f; }
				setTemp = tmpTemp;
				dynamicTemp = tmpTemp;
				saveSettings();
			}
		}
	}
	setCoolerStopped(LOW);
	delay(50);
	updateCoolingOutput();
	delay(50);
	httpServer.sendHeader("Location", String("/"), true);
	httpServer.send(302, "text/plain", "");
	sendTelemetry();
}

void handleStop() {
	setCoolerStopped(HIGH);
	httpServer.sendHeader("Location", String("/"), true);
	httpServer.send(302, "text/plain", "");
}

void handleNotFound() {
	digitalWrite(LED_BUILTIN, LOW);
	String message = F("File Not Found\n\n");
	message += "URI: ";
	message += httpServer.uri();
	message += "\nMethod: ";
	message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += httpServer.args();
	message += "\n";

	for (uint8_t i = 0; i < httpServer.args(); i++) {
		message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
	}

	httpServer.send(404, "text/plain", message);
	digitalWrite(LED_BUILTIN, HIGH);
}

void handleNetworkStore() {
	if (httpServer.args() > 0) {
		for (uint8_t i = 0; i < httpServer.args(); i++) {
			String s = httpServer.arg(i);
			if (httpServer.argName(i) == "apSSID") {
				s.trim();
				if (s.isEmpty()) {
					strncpy(apSSID, "Cooler", sizeof(apSSID));
					apSSID[sizeof(apSSID) - 1] = '\0';
				} else {
					s.toCharArray(apSSID, sizeof(apSSID));
				}
			}
			if (httpServer.argName(i) == "apPassword" || httpServer.argName(i) == "apPASS") {
				s.trim();
				if (s.isEmpty() || s.length() >= 8) {
					s.toCharArray(apPassword, sizeof(apPassword));
				} else {
					Debug.addLine(F("AP password must be at least 8 characters or empty"));
				}
			}
			if (httpServer.argName(i) == "IntSSID") {
				s.toCharArray(IntSSID, 20);
			}
			if (httpServer.argName(i) == "IntPASS") {
				s.toCharArray(IntPASS, 20);
			}
			if (httpServer.argName(i) == "endpointAddress") {
				endpointAddress = s;
			}
			if (httpServer.argName(i) == "endpointAccessToken") {
				endpointAccessToken = s;
			}
			if (httpServer.argName(i) == "endpointPath") {
				endpointPath = s;
			}
			if (httpServer.argName(i) == "sendInterval") {
				int requestedInterval = s.toInt();
				if (requestedInterval >= 10 && requestedInterval <= 86400) {
					sendInterval = requestedInterval;
				} else {
					Debug.addLine(F("Invalid telemetry interval"));
				}
			}
		}
	}
	saveSettings();
	drd.stop();

	if (!handleFileRead("/reboot.htm")) {
		httpServer.send(200, "text/plain", "Settings saved. Rebooting...");
	}
	delay(1000);
	ESP.restart();
}

void handleCSS() {
	if (!handleFileRead("webpage.css")) {
		httpServer.send(404, "text/plain", "webpage.css not found");
	}
}

void handleScript() {
	if (!handleFileRead("main.js")) {
		httpServer.send(404, "text/plain", "main.js not found");
	}
}

void handleUpdate() {
	if (!handleFileRead("/update.htm")) {
		httpServer.send(200, "text/html",
			F("<!DOCTYPE html><html><head><title>Update</title><link href='webpage.css' rel='stylesheet'></head><body>"
			  "<div class='container'><h2>Firmware Update</h2>"
			  "<form method='POST' action='/update' enctype='multipart/form-data'>"
			  "<p>Select firmware.bin:<br><input type='file' name='update' accept='.bin' required></p>"
			  "<button type='submit' class='btn btn_round'>Upload</button>"
			  "</form></div></body></html>"));
	}
}

static bool updateFailed = false;
static bool updateReceived = false;

void handleUpdateUpload() {
	HTTPUpload &upload = httpServer.upload();

	if (upload.status == UPLOAD_FILE_START) {
		updateFailed = false;
		updateReceived = false;
		drd.stop();

		const bool filesystem = (httpServer.arg("type") == "filesystem" ||
		                         upload.name == "filesystem" ||
		                         upload.filename.indexOf("littlefs") >= 0 ||
		                         upload.filename.indexOf("filesystem") >= 0);
		const int command = filesystem ? U_FS : U_FLASH;

		size_t maxUpdateSize = 0;
		if (filesystem) {
			LittleFS.end();
			maxUpdateSize = ((size_t)FS_end - (size_t)FS_start);
		} else {
			maxUpdateSize = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
		}

		WiFiUDP::stopAll();
		Serial.printf("OTA upload started: %s (%s, max size: %u)\n",
		              upload.filename.c_str(), filesystem ? "LittleFS" : "firmware", (unsigned int)maxUpdateSize);

		if (!Update.begin(maxUpdateSize, command)) {
			Update.printError(Serial);
			updateFailed = true;
		}
	} else if (upload.status == UPLOAD_FILE_WRITE) {
		if (!updateFailed && Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
			Update.printError(Serial);
			updateFailed = true;
		}
	} else if (upload.status == UPLOAD_FILE_END) {
		if (!updateFailed && !Update.end(true)) {
			Update.printError(Serial);
			updateFailed = true;
		}
		updateReceived = true;
		Serial.printf("OTA upload finished: %u bytes\n", (unsigned int)upload.totalSize);
	}
}

void handleUpdateResult() {
	if (!updateReceived || updateFailed || Update.hasError()) {
		httpServer.send(500, "text/plain", "Update failed: flash write error");
		return;
	}

	httpServer.send(200, "text/plain", "Update successful. Restarting...");
	delay(100);
	ESP.restart();
}
