#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>

#include "webpage.css.h"
#include "webpage.htm.h"

//#define USI
#define I2C_ADDRESS 0x5E  // Buck converter address
#define I2C_DATALEN 11  // 11 bytes

// pins definition
#define LED_WARN D0   // LED indicates that peltie is under voltage and can't be just powered off
#define ONE_WIRE_BUS D6  // DS18B20 pin


bool I2C_PRESENT;
uint8_t i2c_data[I2C_DATALEN-1]; //11 bytes

float curTemp=0;	// current measured temperature
float setTemp=30;	// preset temperature
float setVoltage;
float changeVoltageSpeed; 
float minVoltage; 
float maxVoltage; 
byte PWM_value;
float measuredVoltage; 
float measuredCurrent; 

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
DeviceAddress  DS18B20_adr[2];
unsigned long DS18B20Interval=millis();


// Default IP in AP mode is 192.168.4.1
const char *ssid = "Cooler";
const char *password = "1234Test";

// Define a web server at port 80 for HTTP
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

const int Load1 = D5; // Cooler hot

bool ledState = false;

void INIT_DS18B20(){
  DS18B20.begin();
  int available = DS18B20.getDeviceCount();
  for(byte x = 0; x!= available; x++)
  {
    if(DS18B20.getAddress(DS18B20_adr[x], x))
    {
      DS18B20.setResolution(DS18B20_adr[x], 12);
    }
  }
}

void sendI2Cdata(){
  if(I2C_PRESENT){
    Wire.beginTransmission(I2C_ADDRESS); //Start bit
    Wire.write((uint8_t)(setVoltage * 10.0)); // Set Voltage
    Wire.write((uint8_t)(changeVoltageSpeed * 10.0)); // Speed of voltae change (means 0.4v/s)
    Wire.write((uint8_t)(minVoltage * 10.0)); //Min Voltage
    Wire.write((uint8_t)(maxVoltage * 10.0)); //Max Voltage
    Wire.write(0); //Only filled in Read
    Wire.write(0); //Only filled in Read
    Wire.write(0); //Only filled in Read
    Wire.write(0); //Only filled in Read
    Wire.write(0); //Only filled in Read
    Wire.write(0); //Only filled in Read
    Wire.write(0); //Only filled in Read
    Wire.endTransmission();
  }
}

void readI2Cdata(){
    // Read data from I2CBuck
  if(I2C_PRESENT){
    Wire.requestFrom(I2C_ADDRESS, 11);
    Serial.print("I2C bytes Available ");
    Serial.print(Wire.available());
    Serial.print(": ");
    byte i=0;
    while(Wire.available()!=0) {
      i2c_data[i] = Wire.read();
      Serial.print(i2c_data[i]);
      Serial.print(" ");
      //if(i==5){I2Cvolt = c;}
      i++;
    }
    Serial.println();
  }else{
    // clear array
    for(uint8_t i=0;i<I2C_DATALEN;i++){i2c_data[i]=0;}
  }
  // assign to variables
  setVoltage = (float)i2c_data[0]/10.0;
  changeVoltageSpeed = (float)i2c_data[1]/10.0;
  minVoltage = (float)i2c_data[2]/10.0; 
  maxVoltage = (float)i2c_data[3]/10.0; 
  PWM_value = i2c_data[4];
  measuredVoltage = (float)i2c_data[5]/10.0; 
  measuredCurrent = (float)i2c_data[6]/10.0; 
}

void handleRoot() {
  digitalWrite (LED_BUILTIN, LOW); //turn the built in LED on pin DO of NodeMCU on

  char I2CText[80];
  if (I2C_PRESENT) {
    strcpy(I2CText, "I2C Buck converter is connected at address 0x5E.");
  }

  else {
    strcpy(I2CText, "I2C Buck converter is not connected.");
  }

  unsigned long sec = millis() / 1000;
  unsigned long min = sec / 60;
  int hr = min / 60;
  String CurTime=String(hr)+":"+String(min % 60)+":"+String(sec % 60);
  
  readI2Cdata();

// Build an HTML page to display on the web-server root address
	String html_head = FPSTR(HTTP_HEAD_START);
	html_head += FPSTR(HTTP_STYLE);
	String html = FPSTR(HTTP_HEAD_END);    
	html.replace("{Caption}", "Info");
  html = html_head + html;
	html += FPSTR(HTTP_MAIN_DATA);
	html.replace("{curTemp}", String(curTemp));
	html.replace("{setTemp}", String(setTemp));
	html.replace("{curTime}", CurTime);
	html.replace("{setVoltage}", String(setVoltage));
//        html.replace("{changeVoltageSpeed}", String(changeVoltageSpeed));
//        html.replace("{minVoltage}", String(minVoltage));
//        html.replace("{maxVoltage}", String(maxVoltage));
	html.replace("{PWM}", String(PWM_value));
	html.replace("{Voltage}", String(measuredVoltage));
	html.replace("{Current}", String(measuredCurrent));
	html.replace("{I2CText}", String(I2CText));
	html += FPSTR(HTTP_END);

	httpServer.send ( 200, "text/html", html );
	digitalWrite ( LED_BUILTIN,HIGH);
}


void handleSetTemp(){
  Serial.println(httpServer.args());
  if (httpServer.args() > 0 ) {
    for ( uint8_t i = 0; i < httpServer.args(); i++ ) {
       Serial.print("Received field (");
       Serial.print(httpServer.argName(i));
       Serial.print("): ");
       Serial.println(httpServer.arg(i));
      if (httpServer.argName(i) == "Temp") {
         //convert voltage to valid range
         Serial.print("Variable set to: ");
         Serial.println(httpServer.arg(i));
         float tmpVolt=httpServer.arg(i).toFloat();
         if(tmpVolt>25.5){tmpVolt=25.5;}
         if(tmpVolt<0){tmpVolt=0.0;}
         setVoltage = tmpVolt;
         sendI2Cdata();
      }
   }
  }
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

void setup() {
	pinMode (Load1, OUTPUT);
	digitalWrite (Load1, HIGH);
	pinMode (LED_WARN, OUTPUT);
	digitalWrite (LED_WARN, LOW);

	INIT_DS18B20();

	Wire.begin(D2,D1);  // D2-sda, D1-scl
	delay(100); // this delay is very essential for proper working of I2C line
	Wire.setClock(50000); // higher rate is unstable
	Wire.setClockStretchLimit(600); // this should be checked

	Serial.begin(115200);

	 // Detect I2C device
  Wire.beginTransmission(I2C_ADDRESS);
  byte error = Wire.endTransmission();
  if (error==0){
    I2C_PRESENT=HIGH;
    Serial.println("I2C Buck converter fund at address 0x5E");
  }else{
    I2C_PRESENT=LOW;
    Serial.println("I2C Buck converter is not connected.");
  }
   
  delay(500);

  readI2Cdata();

	Serial.println();
	Serial.println("Configuring access point...");

	WiFi.mode(WIFI_AP_STA);
	
	//You can remove the password parameter if you want the AP to be open.
	WiFi.softAP(ssid, password);

	IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);
 
	httpServer.on ( "/", handleRoot );
	httpServer.on("/setTemp", handleSetTemp);
	httpServer.on ( "/inline", []() {
    httpServer.send ( 200, "text/plain", "this works as well" );
  } );
	httpServer.onNotFound ( handleNotFound );
	
	httpUpdater.setup(&httpServer, "/update");
	httpServer.begin();
	Serial.println("HTTP server started");
}

void loop() {
	httpServer.handleClient();
	
	if(millis()-DS18B20Interval>60000){
		// request temperature every minute
		DS18B20Interval=millis();
		DS18B20.requestTemperatures(); 
		curTemp = DS18B20.getTempCByIndex(0);
	}
	
	
	if(measuredVoltage==0){
		digitalWrite (LED_WARN, LOW);
	}else{
		digitalWrite (LED_WARN, HIGH);
	}

}

