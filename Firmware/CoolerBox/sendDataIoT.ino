const char* ubiServer = "things.ubidots.com";
const char* thingServer = "api.thingspeak.com";

void send2Ubidots(){
  WiFiClient clientSend;
  int cn=clientSend.connect(ubiServer, 80);
  if (cn) {
	  
	StaticJsonBuffer<100> jsonBuffer;
	JsonObject &jobj = jsonBuffer.createObject();
	jobj["box_temperature"] = String(ColdTemp);
	jobj["box_voltage"] = String(setVoltage);
	jobj["box_settemp"] = String(setTemp);

	
	String send = F("POST /api/v1.6/devices/");
	send += ubiDevice;
	send += "?token=";
	send += ubiToken;
	send += F(" HTTP/1.1\r\nHost: things.ubidots.com\r\nUser-Agent: ESP8266\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: ");
	send += jobj.measureLength();
	send += "\r\n";
	clientSend.println(send);
	jobj.printTo(clientSend);
	clientSend.println();

    int timeout = 0;
    while (!clientSend.available() && timeout < 2000)
    {
        timeout++;
        delay(1);
    }
	while (clientSend.available())
    {
        clientSend.readStringUntil('\r');
    }
	
	clientSend.stop();
  }
}


void send2Thingspeak(){
	WiFiClient clientSend;
	int cn=clientSend.connect(thingServer, 80);
	if (cn) {
		// Construct API request body
		String body = "field1=";
			   body += String(ColdTemp);
			   body += "&field2=";
			   body += String(setTemp);
			   body += "&field3=";
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
    while (!clientSend.available() && timeout < 2000)
    {
        timeout++;
        delay(1);
    }
	while (clientSend.available())
    {
        clientSend.readStringUntil('\r');
    }

	clientSend.stop();

}
