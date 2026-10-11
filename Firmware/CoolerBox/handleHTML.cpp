  #include "CoolerBox.h"
  #include <functional>

static bool streamTemplate(const char *path, const std::function<bool(const String &key, String &val)> &resolver) {
	File f = LittleFS.open(path, "r");
	if (!f) {
		Debug.addLine(String(F("Template missing: ")) + path);
		httpServer.send(500, "text/plain", String("Template missing: ") + path);
		return false;
	}

	if (!httpServer.chunkedResponseModeStart(200, "text/html")) {
		httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
		httpServer.send(200, "text/html", "");
	}

	String chunk;
	chunk.reserve(512);

	while (f.available()) {
		String line;
		line.reserve(96);
		while (f.available()) {
			char c = (char)f.read();
			if (c == '\r') continue;
			if (c == '\n') break;
			line += c;
		}

		int openIdx = line.indexOf('{');
		while (openIdx >= 0) {
			int closeIdx = line.indexOf('}', openIdx + 1);
			if (closeIdx > openIdx) {
				String key = line.substring(openIdx + 1, closeIdx);
				String val;
				if (resolver(key, val)) {
					line = line.substring(0, openIdx) + val + line.substring(closeIdx + 1);
					openIdx = line.indexOf('{', openIdx + val.length());
				} else {
					openIdx = line.indexOf('{', closeIdx + 1);
				}
			} else {
				break;
			}
		}

		chunk += line;
		chunk += '\n';

		if (chunk.length() >= 400) {
			httpServer.sendContent(chunk);
			chunk = "";
		}
	}
	f.close();

	if (chunk.length() > 0) {
		httpServer.sendContent(chunk);
	}
	httpServer.chunkedResponseFinalize();
	return true;
}

void handleRoot() {
	streamTemplate("/index.htm", [](const String &key, String &val) -> bool {
		if (key == "Caption") { val = emergencyMode ? "Main Info (Emergency AP)" : "Main Info"; return true; }
		if (key == "FW") { val = FIRMWAREVERSION; return true; }
		if (key == "boxTemp") { val = String(boxTemp, 1); return true; }
		if (key == "setTemp") {
			val = stopCooler ? "<div style='color:RED;display:inline-block'>Stopping...</div>" : String(setTemp, 1);
			return true;
		}
		if (key == "setHotPWM") {
			val = String((int)round(setHotPWM * 100.0 / (float)maxHotPWM)) + "%";
			return true;
		}
		if (key == "HotTemp") { val = String(HotTemp, 1); return true; }
		if (key == "Voltage") { val = String(measuredVoltage, 1) + "V"; return true; }
		if (key == "Current") { val = String(measuredCurrent, 1) + "A"; return true; }
		if (key == "I2CText") {
			val = I2C_PRESENT ? "I2C connected" : "I2C connection failed";
			return true;
		}
		return false;
	});
}

String ConstructDS18B20list(int idx){
	uint8_t available = DS18B20_count;
	String AdrTbl = "";
	if (available == 0) {
		return F("<option value='' disabled selected>No sensors detected</option>");
	}
	for(uint8_t x=0; x < available; x++){
		AdrTbl += F("<option value='");
		AdrTbl += String(x) + "' " + (idx==x ? "selected" : "") + ">";
		for (uint8_t i = 0; i < 8; i++) {
			if (DS18B20_adr[x][i] < 16) AdrTbl += "0";
			AdrTbl += String(DS18B20_adr[x][i], HEX);
		}
		AdrTbl += " = " + String(DS18B20.getTempC(DS18B20_adr[x]),2);
		AdrTbl += F("&deg; C</option>");
	}
	return AdrTbl;
}	

void handleSettings() {
	restoreSettingsFromEEPROM();
	INIT_DS18B20();
	// Refresh addresses before rendering the sensor selectors.
	String ColdDSlist = ConstructDS18B20list(ColdSensorId);
	String HotDSlist = ConstructDS18B20list(HotSensorId);
	streamTemplate("/settings.htm", [&](const String &key, String &val) -> bool {
		if (key == "Caption") { val = "Settings"; return true; }
		if (key == "changeVoltageSpeed") { val = String(changeVoltageSpeed, 1); return true; }
		if (key == "minVoltage") { val = String(minVoltage, 1); return true; }
		if (key == "maxVoltage") { val = String(maxVoltage, 1); return true; }
		if (key == "ColdDSlist") { val = ColdDSlist; return true; }
		if (key == "HotDSlist") { val = HotDSlist; return true; }
		if (key == "fanDynamic_checked") { val = (fanDynamic_checked == 0 ? "" : "checked"); return true; }
		if (key == "boxMode_1") { val = (boxMode == 1 ? "checked" : ""); return true; }
		if (key == "boxMode_2") { val = (boxMode == 2 ? "checked" : ""); return true; }
		if (key == "boxMode_3") { val = (boxMode == 3 ? "checked" : ""); return true; }
		return false;
	});
}

void handleSettingsStore(){
  if (httpServer.args() > 0 ) {
	fanDynamic_checked = 0;
	for ( uint8_t i = 0; i < httpServer.args(); i++ ) {
      if (httpServer.argName(i) == "VoltChange") {
         //convert voltage to valid range
         float tmpVolt=httpServer.arg(i).toFloat();
         if(tmpVolt>25.5){tmpVolt=changeVoltageSpeed;}	// don't change just in case.
         if(tmpVolt<0.4){tmpVolt=0.4;}
         changeVoltageSpeed = tmpVolt;
      }
      if (httpServer.argName(i) == "VoltMin") {
         //convert voltage to valid range
         float tmpVolt=httpServer.arg(i).toFloat();
         if(tmpVolt>12.0){tmpVolt=minVoltage;}	// don't change just in case.
         if(tmpVolt<1.0){tmpVolt=1.0;}
         minVoltage = tmpVolt;
      }
      if (httpServer.argName(i) == "VoltMax") {
         //convert voltage to valid range
         float tmpVolt=httpServer.arg(i).toFloat();
         if(tmpVolt>12.0){tmpVolt=maxVoltage;}	// don't change just in case.
         if(tmpVolt<1.0){tmpVolt=1.0;}
         maxVoltage = tmpVolt;
         maxVoltage_backup = maxVoltage; // update backup

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
  hotPID.SetOutputLimits(0, maxHotPWM);  // same limits as for Buck converter
  delay(50);	// make sure, data is not sent too often
  updateCoolingOutput(true, true);
  delay(50);	// make sure, data is not sent too often
  saveSettings();	// Store settings
  httpServer.sendHeader("Location", String("/"), true);
  httpServer.send ( 302, "text/plain", "");
}

void handlePIDs() {
	streamTemplate("/pids.htm", [](const String &key, String &val) -> bool {
		if (key == "Caption") { val = "PID Settings"; return true; }
		if (key == "hotPID_kP") { val = String(hotPID_kP, 2); return true; }
		if (key == "hotPID_kI") { val = String(hotPID_kI, 4); return true; }
		if (key == "hotPID_kD") { val = String(hotPID_kD, 2); return true; }
		return false;
	});
}

void handlePIDsStore(){
	if (httpServer.args() > 0 ) {
		for(uint8_t i=0;i<httpServer.args();i++) {
			if (httpServer.argName(i) == "hotPID_kP") {
				hotPID_kP=httpServer.arg(i).toFloat();
			}
			if (httpServer.argName(i) == "hotPID_kI") {
				hotPID_kI=httpServer.arg(i).toFloat();
			}
			if (httpServer.argName(i) == "hotPID_kD") {
				hotPID_kD=httpServer.arg(i).toFloat();
			}
		}
	}
	hotPID.SetTunings(hotPID_kP, hotPID_kI, hotPID_kD);	// Set new values 
	saveSettings();	// Store new PID values

	httpServer.sendHeader("Location", String("/"), true);
	httpServer.send ( 302, "text/plain", "");
}

void handleSetTemp(){
	if (httpServer.args() > 0 ) {
		for ( uint8_t i = 0; i < httpServer.args(); i++ ) {
			if (httpServer.argName(i) == "Temp") {
				//convert temperature to valid range
				float tmpTemp=httpServer.arg(i).toFloat();
				if(tmpTemp>45.0){tmpTemp=setTemp;}	// don't change just in case.
				if(tmpTemp<0){tmpTemp=0.0;}
				setTemp = tmpTemp;
        dynamicTemp = tmpTemp;
				saveSettings();	// Store new PID value
			}
		}
	}
	setCoolerStopped(LOW); // resume Cooler if temperature changed
	delay(50);	// make sure, data is not sent too often
	updateCoolingOutput();
	delay(50);	// make sure, data is not sent too often
	httpServer.sendHeader("Location", String("/"), true);
	httpServer.send ( 302, "text/plain", "");
	sendTelemetry();
}

void handleStop(){
  setCoolerStopped(HIGH);
  httpServer.sendHeader("Location", String("/"), true);
  httpServer.send ( 302, "text/plain", "");
}

void handleNotFound() {
  digitalWrite ( LED_BUILTIN, LOW );
  String message = F("File Not Found\n\n");
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += ( httpServer.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";

  for ( uint8_t i = 0; i < httpServer.args(); i++ ) {
    message += " " + httpServer.argName ( i ) + ": " + httpServer.arg ( i ) + "\n";
  }

  httpServer.send ( 404, "text/plain", message );
  digitalWrite ( LED_BUILTIN, HIGH ); //turn the built in LED on pin DO of NodeMCU off
}

void handleNetwork() {
	streamTemplate("/network.htm", [](const String &key, String &val) -> bool {
		if (key == "Caption") { val = emergencyMode ? "Network (Recovery Mode)" : "Network"; return true; }
		if (key == "apSSID") { val = apSSID; return true; }
		if (key == "apPassword") { val = apPassword; return true; }
		if (key == "IntSSID") { val = IntSSID; return true; }
		if (key == "IntPASS") { val = IntPASS; return true; }
		if (key == "endpointAddress") { val = endpointAddress; return true; }
		if (key == "endpointAccessToken") { val = endpointAccessToken; return true; }
		if (key == "endpointPath") { val = endpointPath; return true; }
		if (key == "sendInterval") { val = String(sendInterval); return true; }
		return false;
	});
}

void handleNetworkStore(){
	if (httpServer.args() > 0 ) {
		for(uint8_t i=0;i<httpServer.args();i++) {
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
				s.toCharArray(IntSSID,20);
			}
			if (httpServer.argName(i) == "IntPASS") {
				s.toCharArray(IntPASS,20);
			}
			if (httpServer.argName(i) == "endpointAddress") {
				endpointAddress=s;
			}
			if (httpServer.argName(i) == "endpointAccessToken") {
				endpointAccessToken=s;
			}
			if (httpServer.argName(i) == "endpointPath") {
				endpointPath=s;
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
	saveSettings();	// Store new settings
	drd.stop();		// Ensure next boot is normal

	if (!handleFileRead("/reboot.htm")) {
		httpServer.send(200, "text/plain", "Settings saved. Rebooting...");
	}
	delay(1000);
	ESP.restart();
}

void handleDebug() {
	streamTemplate("/debug.htm", [](const String &key, String &val) -> bool {
		if (key == "Caption") { val = "Debug"; return true; }
		if (key == "Debug") { val = Debug.getHTML(); return true; }
		return false;
	});
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
