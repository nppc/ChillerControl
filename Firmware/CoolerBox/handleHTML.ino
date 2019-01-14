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
	html.replace("{ColdTemp}", String(ColdTemp,1));
	html.replace("{setTemp}", (setTemp==999.0 ? "<div style='color:RED;display:inline-block'>Stopping...</div>":String(setTemp,1)));
	//html.replace("{CurTime}", CurTime);
	html.replace("{setVoltage}", String(setVoltage,1) + "V");
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
	#ifdef DEBUG
	html += "<pre>" + debugOut + "</pre>";
	#endif

	html += FPSTR(HTTP_END);
	httpServer.send ( 200, "text/html", html );
}

void handleSettingsStore(){
  if (httpServer.args() > 0 ) {
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
   }
  }
  myPID.SetOutputLimits(minVoltage, maxVoltage);  // same limits as for Buck converter
  delay(50);	// make sure, data is not sent too often
  sendI2Cdata();
  delay(50);	// make sure, data is not sent too often
  saveSettings();	// Store new temperature sensors mapping
  httpServer.sendHeader("Location", String("/"), true);
  httpServer.send ( 302, "text/plain", "");
}

void handlePIDs() {
	// Build an HTML page to display on the web-server 
	String html = FPSTR(HTTP_HEAD);
	html.replace("{Caption}", "PID Settings");
	html += FPSTR(HTTP_PIDS_DATA);
	html.replace("{pid_kP}", String(pid_kP,2));
	html.replace("{pid_kI}", String(pid_kI,4));
	html.replace("{pid_kD}", String(pid_kD,2));
	html += FPSTR(HTTP_END);

	httpServer.send ( 200, "text/html", html );
}

void handlePIDsStore(){
	if (httpServer.args() > 0 ) {
		for(uint8_t i=0;i<httpServer.args();i++) {
			if (httpServer.argName(i) == "pid_kP") {
				pid_kP=httpServer.arg(i).toFloat();
			}
			if (httpServer.argName(i) == "pid_kI") {
				pid_kI=httpServer.arg(i).toFloat();
			}
			if (httpServer.argName(i) == "pid_kD") {
				pid_kD=httpServer.arg(i).toFloat();
			}
		}
	}
	myPID.SetTunings(pid_kP, pid_kI, pid_kD);	// Set new values 
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
				if(tmpTemp>25.0){tmpTemp=setTemp;}	// don't change just in case.
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
}

void handleStop(){
  setTemp=999;
  httpServer.sendHeader("Location", String("/"), true);
  httpServer.send ( 302, "text/plain", "");
}

void handleNotFound() {
  digitalWrite ( LED_BUILTIN, LOW );
  String message = "File Not Found\n\n";
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


void handleCSS() {
	File file = SPIFFS.open("/webpage.css", "r");
	httpServer.streamFile(file, "text/css");
	file.close();
}
