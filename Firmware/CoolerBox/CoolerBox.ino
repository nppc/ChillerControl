#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>

//IPAddress    apIP(42, 42, 42, 42);  // Defining a static IP address: local & gateway
                                    // Default IP in AP mode is 192.168.4.1

const char *ssid = "Cooler";
const char *password = "1234Test";

// Define a web server at port 80 for HTTP
ESP8266WebServer server(80);

const int Load1 = D5; // an LED

bool ledState = false;
int I2Cvolt = 0;

void handleRoot() {
  digitalWrite (LED_BUILTIN, 0); //turn the built in LED on pin DO of NodeMCU on
  digitalWrite (Load1, server.arg("led").toInt());
  ledState = digitalRead(Load1);

 /* Dynamically generate the LED toggle link, based on its current state (on or off)*/
  char ledText[80];
  
  if (ledState) {
    strcpy(ledText, "LED is on. <a href=\"/?led=0\">Turn it OFF!</a>");
  }

  else {
    strcpy(ledText, "LED is OFF. <a href=\"/?led=1\">Turn it ON!</a>");
  }
 
  ledState = digitalRead(Load1);

  char html[1000];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

// Read voltage from I2CBuck
  Wire.requestFrom(0x5E, 11);
  Serial.print("I2C bytes Available ");
  while(Wire.available()==0);
  Serial.print(Wire.available());
  Serial.print(": ");
	byte i=0;
  while(Wire.available()!=0) {
    if(i==5){I2Cvolt = Wire.read();}else{Wire.read();}
	i++;
  }
  Serial.println();
  
// Build an HTML page to display on the web-server root address
  snprintf ( html, 1000,

"<html>\
  <head>\
    <meta http-equiv='refresh' content='10'/>\
    <title>ESP8266 WiFi Network</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; font-size: 1.5em; Color: #000000; }\
      h1 { Color: #AA0000; }\
    </style>\
  </head>\
  <body>\
    <h1>ESP8266 Wi-Fi Access Point and Web Server Demo</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>Voltage: %d%</p>\
    <p>%s<p>\
    <p>This page refreshes every 10 seconds. Click <a href=\"javascript:window.location.reload();\">here</a> to refresh the page now.</p>\
  </body>\
</html>",

    hr, min % 60, sec % 60,
    I2Cvolt,
    ledText
  );
  server.send ( 200, "text/html", html );
  digitalWrite ( LED_BUILTIN, 1 );
}

void handleNotFound() {
  digitalWrite ( LED_BUILTIN, 0 );
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  digitalWrite ( LED_BUILTIN, 1 ); //turn the built in LED on pin DO of NodeMCU off
}

void setup() {
	pinMode (Load1, OUTPUT);
	digitalWrite (Load1, LOW);

	Wire.begin(D1,D2);
	Wire.setClock(100000);
	//Wire.setClockStretchLimit(2 * 230);

	Serial.begin(115200);
	delay(1000);

	Serial.println("Configuring I2C Buck...");
	Wire.beginTransmission(0x5E); //Start bit
	Wire.write(0); // Set Voltage
	Wire.write(4); // Speed of voltae change (means 0.4v/s)
	Wire.write(7); //Min Voltage
	Wire.write(120); //Max Voltage
	Wire.write(0); //Only filled in Read
	Wire.write(0); //Only filled in Read
	Wire.write(0); //Only filled in Read
	Wire.write(0); //Only filled in Read
	Wire.write(0); //Only filled in Read
	Wire.write(0); //Only filled in Read
	Wire.write(0); //Only filled in Read
	Wire.endTransmission();

	Serial.println();
	Serial.println("Configuring access point...");

	//set-up the custom IP address
	WiFi.mode(WIFI_AP_STA);
	//WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   // subnet FF FF FF 00  
	
	/* You can remove the password parameter if you want the AP to be open. */
	WiFi.softAP(ssid, password);

	IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);
 
  server.on ( "/", handleRoot );
  server.on ( "/led=1", handleRoot);
  server.on ( "/led=0", handleRoot);
  server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );
  server.onNotFound ( handleNotFound );
	
	server.begin();
	Serial.println("HTTP server started");
}

void loop() {
	server.handleClient();
}
