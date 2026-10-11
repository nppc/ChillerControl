  #include "CoolerBox.h"

void handleRoot() {

// Build an HTML page to display on the web-server root address
	String html = FPSTR(HTTP_HEADweb);
	html.replace("{Caption}", emergencyMode ? "Main Info (Emergency AP)" : "Main Info");
	html += FPSTR(HTTP_MAIN_DATA);
	html.replace("{FW}", FIRMWAREVERSION);
	html.replace("{boxTemp}", String(boxTemp,1));
	html.replace("{setTemp}", (stopCooler ? "<div style='color:RED;display:inline-block'>Stopping...</div>":String(setTemp,1)));
	html.replace("{setHotPWM}", String((int)round(setHotPWM*100.0/(float)maxHotPWM)) + "%");
	html.replace("{HotTemp}", String(HotTemp,1));
	html.replace("{Voltage}", String(measuredVoltage,1) + "V");
	html.replace("{Current}", String(measuredCurrent,1) + "A");
	html.replace("{I2CText}", I2C_PRESENT ? "I2C connected" : "I2C connection failed");
	html += FPSTR(HTTP_END);
	httpServer.send ( 200, "text/html", html );
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
	// Build an HTML page to display on the web-server 
	String html = FPSTR(HTTP_HEADweb);
	html.replace("{Caption}", "Settings");
	html += FPSTR(HTTP_SETTINGS_DATA);
	html.replace("{changeVoltageSpeed}", String(changeVoltageSpeed,1));
	html.replace("{minVoltage}", String(minVoltage,1));
	html.replace("{maxVoltage}", String(maxVoltage,1));
	html.replace("{ColdDSlist}", ColdDSlist);
	html.replace("{HotDSlist}", HotDSlist);
	html.replace("{fanDynamic_checked}", String(fanDynamic_checked==0 ? "":"checked"));
	// set correct box mode on the page
	html.replace("{boxMode_1}", String(boxMode==1 ? "checked":""));
	html.replace("{boxMode_2}", String(boxMode==2 ? "checked":""));
	html.replace("{boxMode_3}", String(boxMode==3 ? "checked":""));
	#ifdef DEBUG
	html += "<pre>" + debugOut + "</pre>";
	#endif

	html += FPSTR(HTTP_END);
	httpServer.send ( 200, "text/html", html );
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
  updateCoolingOutput();
  delay(50);	// make sure, data is not sent too often
  saveSettings();	// Store settings
  httpServer.sendHeader("Location", String("/"), true);
  httpServer.send ( 302, "text/plain", "");
}

void handlePIDs() {
	// Build an HTML page to display on the web-server 
	String html = FPSTR(HTTP_HEADweb);
	html.replace("{Caption}", "PID Settings");
	html += FPSTR(HTTP_PIDS_DATA);
	html.replace("{hotPID_kP}", String(hotPID_kP,2));
	html.replace("{hotPID_kI}", String(hotPID_kI,4));
	html.replace("{hotPID_kD}", String(hotPID_kD,2));
	html += FPSTR(HTTP_END);

	httpServer.send ( 200, "text/html", html );
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
	// Build an HTML page to display on the web-server 
	String html = FPSTR(HTTP_HEADweb);
	html.replace("{Caption}", emergencyMode ? "Network (Recovery Mode)" : "Network");
	html += FPSTR(HTTP_NETWORK_DATA);
	html.replace("{apSSID}", String(apSSID));
	html.replace("{apPassword}", String(apPassword));
	html.replace("{IntSSID}", String(IntSSID));
	html.replace("{IntPASS}", String(IntPASS));
	html.replace("{endpointAddress}", endpointAddress);
	html.replace("{endpointAccessToken}", endpointAccessToken);
	html.replace("{endpointPath}", endpointPath);
	html.replace("{sendInterval}", String(sendInterval));
	html += FPSTR(HTTP_END);
	
	httpServer.send ( 200, "text/html", html );
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

	String html = FPSTR(HTTP_HEADweb);
	html.replace("{Caption}", "Rebooting");
	html.replace("</head>", "<meta http-equiv=\"refresh\" content=\"6;url=/\"></head>");
	html += F("<BR><TABLE class=\"values_table\"><TR class=\"values_header\"><TD>Network Settings</TD></TR>");
	html += F("<TR><TD style=\"padding:20px;\">Settings saved successfully.<BR><BR>Rebooting device, please wait...</TD></TR></TABLE>");
	html += FPSTR(HTTP_END);
	httpServer.send(200, "text/html", html);
	delay(1000);
	ESP.restart();
}

void handleDebug() {
	// Build an HTML page to display on the web-server 
	String html = FPSTR(HTTP_HEADweb);
	html.replace("{Caption}", "Debug");
	html += FPSTR(HTTP_DEBUG_DATA);
	html.replace("{Debug}", Debug.getHTML());
	html += FPSTR(HTTP_END);
	
	httpServer.send ( 200, "text/html", html );
}


void handleCSS() {
  String html = FPSTR(HTTP_CSS);
  httpServer.send ( 200, "text/css", html );
}

void handleScript() {
  String html = FPSTR(MAIN_JS);
  httpServer.send ( 200, "text/javascript", html );
}
