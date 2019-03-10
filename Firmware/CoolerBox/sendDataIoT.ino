const char* ubiServer = "things.ubidots.com";
const char* thingServer = "api.thingspeak.com";

void receiveUbidotsData(){
  WiFiClient clientSend;
  int cn=clientSend.connect(ubiServer, 80);
  if (cn) {
    
    String send = F("GET /api/v1.6/devices/");
    send += ubiDevice;
    send += F("/box_settemp/values?page_size=1&token=");
    send += ubiToken;
    send += F(" HTTP/1.1\r\nHost: things.ubidots.com\r\nUser-Agent: ESP8266\r\nConnection: close\r\nContent-Type: text/html\r\n");
    
    //thingDebugData= send ;
    
    clientSend.println(send);

    int timeout = 0;
    while (!clientSend.available() && timeout < 5000)
    {
        timeout++;
        delay(1);
    }
  	//ubiDebugData = ""; // clear previous data
  	String strReceive = "";
	while (clientSend.available())
    {
  		String dadaLine = clientSend.readStringUntil('\r');
  		strReceive += dadaLine;
    }
	
  	clientSend.stop();
  
    int bodyPosinit = 9 + strReceive.indexOf("\"value\":");
    int bodyPosend = strReceive.indexOf("}], ");
    // for debugging
	ubiDebugData += strReceive.substring(bodyPosinit,bodyPosend);
	ubiDebugData += " ";
	double tmp_setTemp = strReceive.substring(bodyPosinit,bodyPosend).toFloat();
    // store setTemp if changed and in range
    if(setTemp!=tmp_setTemp && tmp_setTemp>=5.0 && tmp_setTemp<=25.0){
      setTemp = tmp_setTemp;
      saveSettings();
    }
  }
}

void send2Ubidots(bool send_setTemp){
  WiFiClient clientSend;
  int cn=clientSend.connect(ubiServer, 80);
  if (cn) {
	  
	DynamicJsonBuffer jsonBuffer;
	JsonObject &jobj = jsonBuffer.createObject();
  // send setTemp to Ubidots only when requested
	if(send_setTemp){
	  jobj["box_settemp"] = String(setTemp);
	}else{
  	jobj["box_temperature"] = String(boxTemp);
  	jobj["box_voltage"] = String(setVoltage);
  	jobj["box_radiator"] = String(HotTemp);
	}
	
	String send = F("POST /api/v1.6/devices/");
	send += ubiDevice;
	send += F("?token=");
	send += ubiToken;
	send += F(" HTTP/1.1\r\nHost: things.ubidots.com\r\nUser-Agent: ESP8266\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: ");
	send += jobj.measureLength();
	send += "\r\n";
	clientSend.println(send);
	jobj.printTo(clientSend);
	clientSend.println();

    int timeout = 0;
    while (!clientSend.available() && timeout < 5000)
    {
        timeout++;
        delay(1);
    }
	//ubiDebugData = ""; // clear previous data
	while (clientSend.available())
    {
		String dadaLine = clientSend.readStringUntil('\r');
		//ubiDebugData += dadaLine;
    }
	
	clientSend.stop();
  }
}


void send2Thingspeak(){
	WiFiClient clientSend;
	int cn=clientSend.connect(thingServer, 80);
	if (cn) {
		// Construct API request body
		String body = F("field1=");
			   body += String(boxTemp);
			   body += F("&field2=");
			   body += String(setTemp);
			   body += F("&field3=");
			   body += String(setVoltage);

		clientSend.println(F("POST /update HTTP/1.1"));
		clientSend.println(F("Host: api.thingspeak.com"));
		clientSend.println(F("User-Agent: ESP8266/1.0"));
		clientSend.println(F("Connection: close"));
		clientSend.println("X-THINGSPEAKAPIKEY: " + thingWriteAPIKey);
		clientSend.println(F("Content-Type: application/x-www-form-urlencoded"));
		clientSend.println("Content-Length: " + String(body.length()));
		clientSend.println();
		clientSend.print(body);
	}

    int timeout = 0;
    while (!clientSend.available() && timeout < 5000)
    {
        timeout++;
        delay(1);
    }

	thingDebugData = ""; // clear previous data
	while (clientSend.available())
    {
		String dadaLine = clientSend.readStringUntil('\r');
		thingDebugData += dadaLine;
    }

	clientSend.stop();

}
