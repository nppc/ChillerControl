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
	doc["setTemp"] = setTemp;
	doc["targetTemp"] = getHeatingAutoTuneTargetTemp();

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

	uint32_t freeHeap = ESP.getFreeHeap();
	uint8_t frag = ESP.getHeapFragmentation();
	char ramBuf[32];
	snprintf(ramBuf, sizeof(ramBuf), "%u B (%u%% frag)", (unsigned int)freeHeap, frag);
	doc["freeHeap"] = ramBuf;

	if (WiFi.status() == WL_CONNECTED) {
		int rssi = WiFi.RSSI();
		char rssiBuf[48];
		snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm (%s)", rssi, WiFi.SSID().c_str());
		doc["wifiRssi"] = rssiBuf;
	} else if (emergencyMode) {
		doc["wifiRssi"] = "Emergency AP mode";
	} else {
		doc["wifiRssi"] = "Disconnected";
	}

	char dropBuf[48];
	if (emergencyMode) {
		snprintf(dropBuf, sizeof(dropBuf), "Emergency AP mode");
	} else if (!wifiDropOccurred) {
		if (WiFi.status() == WL_CONNECTED) {
			snprintf(dropBuf, sizeof(dropBuf), "None (since boot)");
		} else {
			snprintf(dropBuf, sizeof(dropBuf), "Disconnected (since boot)");
		}
	} else {
		unsigned long dropElapsedSec = (millis() - lastWifiDropMillis) / 1000UL;
		int dropDays = dropElapsedSec / 86400;
		int dropHours = (dropElapsedSec / 3600) % 24;
		int dropMins = (dropElapsedSec / 60) % 60;
		int dropSecs = dropElapsedSec % 60;
		bool isCurrentlyConnected = (WiFi.status() == WL_CONNECTED);

		if (dropDays > 0) {
			snprintf(dropBuf, sizeof(dropBuf), "%dd %02d:%02d:%02d ago%s",
				dropDays, dropHours, dropMins, dropSecs,
				isCurrentlyConnected ? "" : " (offline)");
		} else {
			snprintf(dropBuf, sizeof(dropBuf), "%02d:%02d:%02d ago%s",
				dropHours, dropMins, dropSecs,
				isCurrentlyConnected ? "" : " (offline)");
		}
	}
	doc["lastWifiDrop"] = dropBuf;

	doc["i2cStatus"] = I2C_PRESENT ? "Connected (0x5E)" : "Disconnected";
	doc["resetReason"] = ESP.getResetReason();

	unsigned long val = millis();
	int tdays = val / 86400000;
	unsigned long sec = val / 1000;
	unsigned long min = sec / 60;
	int hr = min / 60;
	char upBuf[32];
	snprintf(upBuf, sizeof(upBuf), "%dd %02d:%02lu:%02lu", tdays, hr % 24, min % 60, sec % 60);
	doc["uptime"] = upBuf;

	String sensStr;
	if (DS18B20_count == 0) {
		sensStr = F("None detected");
	} else if (DS18B20_count == 1) {
		sensStr = "Cold: " + String(boxTemp, 1) + "\xC2\xB0" "C";
	} else {
		sensStr = "Cold: " + String(boxTemp, 1) + "\xC2\xB0" "C / Hot: " + String(HotTemp, 1) + "\xC2\xB0" "C";
	}
	doc["sensorCount"] = sensStr;
	doc["sensorTemps"] = sensStr;

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
	setHeatingTunings(hotPID_kP, hotPID_kI, hotPID_kD);
	saveSettings();

	httpServer.sendHeader("Location", String("/pids"), true);
	httpServer.send(302, "text/plain", "");
}

void handleApiAutotune() {
	if (httpServer.method() == HTTP_POST) {
		String action = httpServer.arg("action");
		if (action == "start") {
			float target = httpServer.hasArg("targetTemp") ? httpServer.arg("targetTemp").toFloat() : getHeatingAutoTuneTargetTemp();
			float pwm = httpServer.hasArg("testPWM") ? httpServer.arg("testPWM").toFloat() : getHeatingAutoTuneTestPWM();
			float noise = httpServer.hasArg("noiseBand") ? httpServer.arg("noiseBand").toFloat() : 0.25f;
			int cyc = httpServer.hasArg("cycles") ? httpServer.arg("cycles").toInt() : 4;
			startHeatingAutoTune(target, pwm, noise, cyc);
		} else if (action == "stop") {
			stopHeatingAutoTune();
		}
	}

	AutoTuneStatus st = getHeatingAutoTuneStatus();
	JsonDocument doc;
	doc["active"] = isHeatingAutoTuning();
	doc["stopping"] = isCoolerStopped();
	doc["state"] = static_cast<int>(st.state);

	const char *stateStr = "IDLE";
	if (st.state == AutoTuneState::WARMING_UP) stateStr = "WARMING_UP";
	else if (st.state == AutoTuneState::CYCLING) stateStr = "CYCLING";
	else if (st.state == AutoTuneState::COMPLETED) stateStr = "COMPLETED";
	else if (st.state == AutoTuneState::FAILED) stateStr = "FAILED";

	doc["stateStr"] = stateStr;
	doc["cycle"] = st.currentCycle;
	doc["targetCycles"] = st.targetCycles;
	doc["targetTemp"] = st.targetTemp;
	doc["testPWM"] = st.testPWM;
	doc["setTemp"] = setTemp;
	doc["currentTemp"] = st.currentTemp;
	doc["peakTemp"] = st.peakTemp;
	doc["valleyTemp"] = st.valleyTemp;
	doc["calculatedKp"] = st.calculatedKp;
	doc["calculatedKi"] = st.calculatedKi;
	doc["calculatedKd"] = st.calculatedKd;
	doc["message"] = st.message;

	String json;
	serializeJson(doc, json);
	httpServer.send(200, "application/json", json);
}

void handleAutotune() {
	if (httpServer.hasArg("action")) {
		String action = httpServer.arg("action");
		if (action == "start") {
			float target = httpServer.hasArg("targetTemp") ? httpServer.arg("targetTemp").toFloat() : setTemp;
			float pwm = httpServer.hasArg("testPWM") ? httpServer.arg("testPWM").toFloat() : 300.0f;
			startHeatingAutoTune(target, pwm);
		} else if (action == "stop") {
			stopHeatingAutoTune();
		}
	}
	httpServer.sendHeader("Location", String("/pids"), true);
	httpServer.send(302, "text/plain", "");
}

void handleSetTemp() {
	if (httpServer.hasArg("Temp")) {
		float tmpTemp = httpServer.arg("Temp").toFloat();
		if (tmpTemp >= 0.0f && tmpTemp <= 45.0f) {
			setTemp = tmpTemp;
			dynamicTemp = tmpTemp;
			if (boxMode == 1) {
				if (boxTemp < dynamicTemp) {
					boxSubMode = 3; // Immediately switch to Heating
				} else if (boxTemp > dynamicTemp) {
					boxSubMode = 2; // Immediately switch to Cooling
				}
			}
			saveSettings();
		}
	}
	setCoolerStopped(LOW);
	delay(50);
	updateCoolingOutput();

	JsonDocument doc;
	doc["status"] = "ok";
	doc["setTemp"] = setTemp;
	String json;
	serializeJson(doc, json);
	httpServer.send(200, "application/json", json);
}

void handleStop() {
	setCoolerStopped(HIGH);
	httpServer.send(200, "application/json", "{\"status\":\"stopped\"}");
}

void handleStart() {
	setCoolerStopped(LOW);
	httpServer.send(200, "application/json", "{\"status\":\"started\"}");
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
	if (!handleFileRead("/webpage.css")) {
		httpServer.send(404, "text/plain", "webpage.css not found");
	}
}

void handleScript() {
	if (!handleFileRead("/main.js")) {
		httpServer.send(404, "text/plain", "main.js not found");
	}
}

void handleUpdate() {
	if (!handleFileRead("/update.htm")) {
		httpServer.send(200, "text/html",
			F("<!DOCTYPE html><html><head><title>Update</title><link href='webpage.css' rel='stylesheet'></head><body>"
			  "<div class='container'><h2>Firmware Update</h2>"
			  "<form method='POST' action='/update' enctype='multipart/form-data'>"
			  "<p>Select firmware.bin (or .bin.gz):<br><input type='file' name='update' accept='.bin,.gz,.bin.gz' required></p>"
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
	delay(500);
	ESP.restart();
}
