  //char I2CText[80];

void handleRoot() {
  
  char I2CText[80];
  if (I2C_PRESENT) {
    strcpy(I2CText, "I2C connected with address 0x5E.");
    //sprintf(I2CText, "NET: %s, %s", IntSSID, IntPASS);
    //char buf[16];
    //sprintf(I2CText, "IP:%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
  }

  else {
    strcpy(I2CText, "I2C is not connected.");
  }

	unsigned long val = millis();
	int tdays = val / 86400000;
	unsigned long sec = val / 1000;
	unsigned long min = sec / 60;
	int hr = min / 60;

	char buf[20];
	sprintf(buf,"%d days, %02d:%02d:%02d",tdays,hr,min % 60,sec % 60);
	String CurTime=String(buf);

	delay(50);	// make sure, data is not sent too often
	readI2Cdata();

// Build an HTML page to display on the web-server root address
	String html = FPSTR(HTTP_HEAD);
	html.replace("{Caption}", "Main Info");
	html += FPSTR(HTTP_MAIN_DATA);
	html.replace("{FW}", FIRMWAREVERSION);
	html.replace("{curTemp}", String(curTemp,1));
	html.replace("{setTemp}", (setTemp==999.0 ? "<div style='color:RED;display:inline-block'>Stopping...</div>":String(setTemp,1)));
	html.replace("{curTime}", CurTime);
	html.replace("{setVoltage}", String(setVoltage,1));
	html.replace("{PWM}", String(PWM_value));
	html.replace("{Voltage}", String(measuredVoltage,1));
	html.replace("{Current}", String(measuredCurrent,1));
	html.replace("{I2CText}", String(I2CText));
	html += FPSTR(HTTP_END);

	httpServer.send ( 200, "text/html", html );
}

void handleSettings() {
	// get temperature sensors addresses
	//int available = DS18B20.getDeviceCount();
	//for(byte x = 0; x!= available; x++){
	//		DS18B20_adr[x];
	//	}
	//}
	// Build an HTML page to display on the web-server 
	String html = FPSTR(HTTP_HEAD);
	html.replace("{Caption}", "Settings");
	html += FPSTR(HTTP_SETTINGS_DATA);
	html.replace("{changeVoltageSpeed}", String(changeVoltageSpeed,1));
	html.replace("{minVoltage}", String(minVoltage,1));
	html.replace("{maxVoltage}", String(maxVoltage,1));
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
      }
   }
  }
  myPID.SetOutputLimits(minVoltage, maxVoltage);  // same limits as for Buck converter
  delay(50);	// make sure, data is not sent too often
  sendI2Cdata();
  delay(50);	// make sure, data is not sent too often
  httpServer.sendHeader("Location", String("/"), true);
  httpServer.send ( 302, "text/plain", "");
}

void handlePIDs() {
	// Build an HTML page to display on the web-server 
	String html = FPSTR(HTTP_HEAD);
	html.replace("{Caption}", "PID Settings");
	html += FPSTR(HTTP_PIDS_DATA);
	html.replace("{pid_kP}", String(pid_kP,2));
	html.replace("{pid_kI}", String(pid_kI,2));
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
	saveEEPROMdata();	// Store new PID values

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
				saveEEPROMdata();	// Store new PID value
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

void handleCSS() {
	File file = SPIFFS.open("/webpage.css", "r");
	httpServer.streamFile(file, "text/css");
	file.close();
}