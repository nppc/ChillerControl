const char* ubiServer = "industrial.api.ubidots.com";
const char* thingServer = "api.thingspeak.com";

void send2Ubidots(){
  WiFiClient clientSend;
  int cn=clientSend.connect(ubiServer, 80);
  if (cn) {
	String body = "\"box_temperature\":";
		   body += String(curTemp);

	clientSend.println("POST /api/v1.6/devices/" + ubiDevice + " HTTP/1.1");
	clientSend.println("Host: things.ubidots.com");
	clientSend.println("User-Agent: ESP8266/1.0");
	clientSend.println("X-Auth-Token: " + ubiToken);
	clientSend.println("Connection: close");
	clientSend.println("Content-Type: Content-Type: application/json");
	clientSend.println("Content-Length: " + String(body.length()));
	clientSend.println("");
	clientSend.print(body);

	clientSend.stop();
  }
	/*
char* body = (char *) malloc(sizeof(char) * 150);
  char* data = (char *) malloc(sizeof(char) * 300);
  // Space to store values to send 
  char str_val[10];

  dtostrf(curTemp, 4, 2, str_val);

  sprintf(body, "{\"%s\":%s}", VARIABLE_LABEL, str_val);
  
  sprintf(data, "POST /api/v1.6/devices/%s", DEVICE_LABEL);
  sprintf(data, "%s HTTP/1.1\r\n", data);
  sprintf(data, "%sHost: things.ubidots.com\r\n", data);
  sprintf(data, "%sUser-Agent: %s/%s\r\n", data, USER_AGENT, VERSION);
  sprintf(data, "%sX-Auth-Token: %s\r\n", data, UBI_TOKEN);
  sprintf(data, "%sConnection: close\r\n", data);
  sprintf(data, "%sContent-Type: application/json\r\n", data);
  sprintf(data, "%sContent-Length: %d\r\n\r\n", data, dataLen(body));
  sprintf(data, "%s%s\r\n\r\n", data, body);

  clientUbi.connect(HTTPSERVER, HTTPPORT);

  if (clientUbi.connect(HTTPSERVER, HTTPPORT)) {
        Serial.println(F("Posting your variables: "));
        Serial.println(data);
        clientSend.print(data);
  }

  free(data);
  free(body);
  clientUbi.stop();
}
*/
}


void send2Thingspeak(){
  WiFiClient clientSend;
  int cn=clientSend.connect(thingServer, 80);
  if (cn) {
    // Construct API request body
    String body = "field1=";
           body += String(curTemp);
           body += "&field2=";
           body += String(setTemp);
           body += "&field3=";
           body += String(setVoltage);
    
    clientSend.println("POST /update HTTP/1.1");
    clientSend.println("Host: api.thingspeak.com");
    clientSend.println("User-Agent: ESP8266/1.0");
    clientSend.println("Connection: close");
    clientSend.println("X-THINGSPEAKAPIKEY: " + thingWriteAPIKey);
    clientSend.println("Content-Type: application/x-www-form-urlencoded");
    clientSend.println("Content-Length: " + String(body.length()));
    clientSend.println("");
    clientSend.print(body);

  }
  clientSend.stop();

}
