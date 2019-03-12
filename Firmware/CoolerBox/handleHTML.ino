  //char I2CText[80];

void handleRoot() {
  
	String I2CText;
	if (I2C_PRESENT) {
		I2CText = "I2C connected";
	} else {
		I2CText = "I2C is not connected";
	}


	//delay(50);	// make sure, data is not sent too often
	//readI2Cdata();

// Build an HTML page to display on the web-server root address
	String html = FPSTR(HTTP_HEAD);
	html.replace("{Caption}", "Main Info");
	html += FPSTR(HTTP_MAIN_DATA);
	html.replace("{FW}", FIRMWAREVERSION);
	html.replace("{boxTemp}", String(boxTemp,1));
	html.replace("{setTemp}", (stopCooler ? "<div style='color:RED;display:inline-block'>Stopping...</div>":String(setTemp,1)));
	//html.replace("{setVoltage}", String(setVoltage,1) + "V");
	html.replace("{setHotPWM}", String((int)round(setHotPWM*100.0/(float)maxHotPWM)) + "%");
	html.replace("{HotTemp}", String(HotTemp,1));
	html.replace("{Voltage}", String(measuredVoltage,1) + "V");
	html.replace("{Current}", String(measuredCurrent,1) + "A");
	html.replace("{I2CText}", I2CText);
	html += FPSTR(HTTP_END);
	httpServer.send ( 200, "text/html", html );
}

String ConstructDS18B20list(int idx){
	int available = DS18B20.getDeviceCount();
	String AdrTbl = "";
	for(byte x=0; x!= available; x++){
		AdrTbl += F("<option value='");
		AdrTbl += String(x) + "' " + (idx==x ? "selected" : "") + ">";
		for( byte i=0; i<8; i++) {
			if (DS18B20_adr[x][i] < 16) AdrTbl += "0";
			AdrTbl += String(DS18B20_adr[x][i], HEX);
		}
		AdrTbl += " = " + String(DS18B20.getTempCByIndex(x),2);
		AdrTbl += F("&deg; C</option>");
	}
	return AdrTbl;
}	

void handleSettings() {
	// get temperature sensors addresses
	String ColdDSlist = ConstructDS18B20list(ColdSensorId);
	String HotDSlist = ConstructDS18B20list(HotSensorId);
	// Build an HTML page to display on the web-server 
	String html = FPSTR(HTTP_HEAD);
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
  coldPID.SetOutputLimits(minVoltage, maxVoltage);  // reverse
  hotPID.SetOutputLimits(0, maxHotPWM);  // same limits as for Buck converter
  delay(50);	// make sure, data is not sent too often
  sendI2Cdata();
  delay(50);	// make sure, data is not sent too often
  saveSettings();	// Store settings
  httpServer.sendHeader("Location", String("/"), true);
  httpServer.send ( 302, "text/plain", "");
}

void handlePIDs() {
	// Build an HTML page to display on the web-server 
	String html = FPSTR(HTTP_HEAD);
	html.replace("{Caption}", "PID Settings");
	html += FPSTR(HTTP_PIDS_DATA);
	html.replace("{coldPID_kP}", String(coldPID_kP,2));
	html.replace("{coldPID_kI}", String(coldPID_kI,4));
	html.replace("{coldPID_kD}", String(coldPID_kD,2));
	html.replace("{hotPID_kP}", String(hotPID_kP,2));
	html.replace("{hotPID_kI}", String(hotPID_kI,4));
	html.replace("{hotPID_kD}", String(hotPID_kD,2));
	html += FPSTR(HTTP_END);

	httpServer.send ( 200, "text/html", html );
}

void handlePIDsStore(){
	if (httpServer.args() > 0 ) {
		for(uint8_t i=0;i<httpServer.args();i++) {
			if (httpServer.argName(i) == "coldPID_kP") {
				coldPID_kP=httpServer.arg(i).toFloat();
			}
			if (httpServer.argName(i) == "coldPID_kI") {
				coldPID_kI=httpServer.arg(i).toFloat();
			}
			if (httpServer.argName(i) == "coldPID_kD") {
				coldPID_kD=httpServer.arg(i).toFloat();
			}
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
	coldPID.SetTunings(coldPID_kP, coldPID_kI, coldPID_kD);	// Set new values 
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
				saveSettings();	// Store new PID value
			}
		}
	}
	delay(50);	// make sure, data is not sent too often
	sendI2Cdata();
	delay(50);	// make sure, data is not sent too often
	httpServer.sendHeader("Location", String("/"), true);
	httpServer.send ( 302, "text/plain", "");
  if(sendUbi_checked==1){send2Ubidots(HIGH);} // send only setTemp
  stopCooler = LOW; // resume Cooler if temperature changed
}

void handleStop(){
  stopCooler=HIGH;
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
	String html = FPSTR(HTTP_HEAD);
	html.replace("{Caption}", "Network");
	html += FPSTR(HTTP_NETWORK_DATA);
	html.replace("{IntSSID}", String(IntSSID));
	html.replace("{IntPASS}", String(IntPASS));
	html.replace("{thingWriteAPIKey}",thingWriteAPIKey);
	html.replace("{ubiToken}", String(ubiToken));
	html.replace("{ubiDevice}", String(ubiDevice));
	html.replace("{sendInterval}", String(sendInterval));
	html.replace("{sendThing_checked}", String(sendThing_checked==0 ? "":"checked"));
	html.replace("{sendUbi_checked}", String(sendUbi_checked==0 ? "":"checked"));
	html += FPSTR(HTTP_END);
	
	httpServer.send ( 200, "text/html", html );
}

void handleNetworkStore(){
	if (httpServer.args() > 0 ) {
		sendThing_checked = 0;
		sendUbi_checked = 0;
		for(uint8_t i=0;i<httpServer.args();i++) {
			String s = httpServer.arg(i);
			if (httpServer.argName(i) == "IntSSID") {
				s.toCharArray(IntSSID,20);
			}
			if (httpServer.argName(i) == "IntPASS") {
				s.toCharArray(IntPASS,20);
			}
			if (httpServer.argName(i) == "thingWriteAPIKey") {
				thingWriteAPIKey=s;
			}
			if (httpServer.argName(i) == "ubiToken") {
				ubiToken=s;
			}
			if (httpServer.argName(i) == "ubiDevice") {
				ubiDevice=s;
			}
			if (httpServer.argName(i) == "sendInterval") {
				sendInterval = s.toInt();
			}
			if (httpServer.argName(i) == "sendThing") {
				sendThing_checked = 1;
			}
			if (httpServer.argName(i) == "sendUbi") {
				sendUbi_checked = 1;
			}
		}
	}
	saveSettings();	// Store new PID values

	httpServer.sendHeader("Location", String("/"), true);
	httpServer.send ( 302, "text/plain", "");
}

void handleDebug() {
	// Build an HTML page to display on the web-server 
	String html = FPSTR(HTTP_HEAD);
	html.replace("{Caption}", "Debug");
	html += FPSTR(HTTP_DEBUG_DATA);
	html.replace("{ubiDebug}", ubiDebugData);
	html.replace("{thingDebug}", thingDebugData);
	html += FPSTR(HTTP_END);
	
	httpServer.send ( 200, "text/html", html );
}


void handleCSS() {
	File file = SPIFFS.open("/webpage.css", "r");
	httpServer.streamFile(file, "text/css");
	file.close();
}
