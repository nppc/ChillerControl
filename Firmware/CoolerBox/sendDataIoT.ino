const char* ubiServer = "things.ubidots.com";
const char* thingServer = "api.thingspeak.com";

double receiveUbidotsData(String uVar){
  WiFiClient clientSend;
  if(clientSend.connect(ubiServer, 80)){
    String send = F("GET /api/v1.6/devices/");
    send += ubiDevice;
    send += "/" + uVar + F("/values?page_size=1&token=");
    send += ubiToken;
    send += F(" HTTP/1.1\r\nHost: things.ubidots.com\r\nUser-Agent: ESP8266\r\nConnection: close\r\nContent-Type: text/html\r\n");
        
    clientSend.println(send);

    int timeout = 0;
    while (!clientSend.available() && timeout < 5000)
    {
        timeout++;
        delay(1);
    }
  	String strReceive = "";
  	while (clientSend.available()){
  		String dadaLine = clientSend.readStringUntil('\r');
  		strReceive += dadaLine;
    }
	
  	clientSend.stop();
    delay(100);
  
    int bodyPosinit = 9 + strReceive.indexOf("\"value\":");
    int bodyPosend = strReceive.indexOf("}], ");
    Debug.addLine(F("RCVUBIOK"));
	  return strReceive.substring(bodyPosinit,bodyPosend).toFloat();
  }else{
    Debug.addLine(F("RCVUBIFAIL"));
    return 0.0;
  }
}

void send2Ubidots(bool send_setTemp){
  WiFiClient clientSend;
  if(clientSend.connect(ubiServer, 80)){
  	DynamicJsonBuffer jsonBuffer;
  	JsonObject &jobj = jsonBuffer.createObject();
    // send setTemp to Ubidots only when requested
  	if(send_setTemp){
  	  jobj["box_settemp"] = String(setTemp);
  	}else{
    	jobj["box_temperature"] = String(boxTemp);
    	jobj["box_voltage"] = String(setVoltage);
    	jobj["box_hotpwm"] = String(setHotPWM*100.0/MAXIMUM_PWM_HOT_WIRE); // in %
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
    delay(100);
    Debug.addLine(F("SNDUBIOK"));
  }else{
    //connection failed
    Debug.addLine(F("SNDUBIFAIL"));
  }
}


void send2Thingspeak(){
	WiFiClient clientSend;
	if(clientSend.connect(thingServer, 80)){
		// Construct API request body
		String body = F("field1=");
			   body += String(boxTemp);
			   body += F("&field2=");
			   body += String(setTemp);
			   body += F("&field3=");
			   body += String(setVoltage);
			   body += F("&field4=");
			   body += String(setHotPWM);

		clientSend.println(F("POST /update HTTP/1.1"));
		clientSend.println(F("Host: api.thingspeak.com"));
		clientSend.println(F("User-Agent: ESP8266/1.0"));
		clientSend.println(F("Connection: close"));
		clientSend.println("X-THINGSPEAKAPIKEY: " + thingWriteAPIKey);
		clientSend.println(F("Content-Type: application/x-www-form-urlencoded"));
		clientSend.println("Content-Length: " + String(body.length()));
		clientSend.println();
		clientSend.print(body);

    int timeout = 0;
    while (!clientSend.available() && timeout < 5000)
    {
        timeout++;
        delay(1);
    }

  	while (clientSend.available()) {
  		String dadaLine = clientSend.readStringUntil('\r');
    }
  
  	clientSend.stop();
    delay(100);
    Debug.addLine(F("SNDTHNGOK"));
  }else{
    //connection failed
    Debug.addLine(F("SNDTHNGFAIL"));
  }

}
