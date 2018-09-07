#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <PID_v1.h>
#include <EEPROM.h>

#include "globals.h"
#include "webpage.css.h"
#include "webpage.htm.h"

#define I2C_ADDRESS 0x5E  // Buck converter address
#define I2C_DATALEN 11  // 11 bytes

// pins definition
#define LED_WARN D0   // LED indicates that peltie is under voltage and can't be just powered off
#define ONE_WIRE_BUS D6  // DS18B20 pin


bool I2C_PRESENT;
uint8_t i2c_data[I2C_DATALEN-1]; //11 bytes

double curTemp;	// current measured temperature
double setTemp=18;	// preset temperature
double setVoltage;
float changeVoltageSpeed; 
float minVoltage; 
float maxVoltage; 
byte PWM_value;
float measuredVoltage; 
float measuredCurrent; 

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
DeviceAddress  DS18B20_adr[2];
unsigned long DS18B20Interval;


// Default IP in AP mode is 192.168.4.1
const char *ssid = "Cooler";
const char *password = "1234Test";

// Define a web server at port 80 for HTTP
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

const int Load1 = D5; // Cooler hot

float pid_kP=DEFAULT_PID_KP;
float pid_kI=DEFAULT_PID_KI;
float pid_kD=DEFAULT_PID_KD;
// Create PID with default values
PID myPID(&(curTemp), &(setVoltage), &(setTemp), DEFAULT_PID_KP, DEFAULT_PID_KI, DEFAULT_PID_KD, REVERSE);


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

void restoreEEPROMdata(){
 struct {
    float pid_kP=DEFAULT_PID_KP;
    float pid_kI=DEFAULT_PID_KI;
    float pid_kD=DEFAULT_PID_KD;
	float SetTemp=18;
  } EEdata;
  
	EEPROM.begin(40);
	EEPROM.get(0,EEdata);
	// assign data to variables and check for validity and if not valid leave default
	if(EEdata.pid_kP<=300.0 && EEdata.pid_kP>=0.0){pid_kP = EEdata.pid_kP;}
	if(EEdata.pid_kI<=300.0 && EEdata.pid_kI>=0.0){pid_kI = EEdata.pid_kI;}
	if(EEdata.pid_kD<=300.0 && EEdata.pid_kD>=0.0){pid_kD = EEdata.pid_kD;}
	if(EEdata.SetTemp<=30.0 && EEdata.SetTemp>=0.0){SetTemp = EEdata.SetTemp;}
}

void saveEEPROMdata(){
 struct {
    float pid_kP;
    float pid_kI;
    float pid_kD;
	float SetTemp;
  } EEdata;
  
	EEPROM.begin(40);
	EEPROM.get(0,EEdata);	// somebody explained that this is still needed....
	// fill struct with correct data
	EEdata.pid_kP=pid_kP;
	EEdata.pid_kI=pid_kI;
	EEdata.pid_kD=pid_kD;
	EEdata.SetTemp=SetTemp;
	//update EEPROM
	EEPROM.put(0,data);
	EEPROM.commit();
}

void sendI2Cdata(){
  if(I2C_PRESENT){
    Wire.beginTransmission(I2C_ADDRESS); //Start bit
    Wire.write((uint8_t)(setVoltage * 10.0)); // Set Voltage
    Wire.write((uint8_t)(changeVoltageSpeed * 10.0)); // Speed of voltae change (means 0.4v/s)
    Wire.write((uint8_t)(minVoltage * 10.0)); //Min Voltage
    Wire.write((uint8_t)(maxVoltage * 10.0)); //Max Voltage
    Wire.write(PWM_value); //No affect on Buck Converter
    Wire.write((uint8_t)(measuredVoltage * 10.0)); //No affect on Buck Converter
    Wire.write((uint8_t)(measuredCurrent * 10.0)); //No affect on Buck Converter
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

  char I2CText[80];
  if (I2C_PRESENT) {
    strcpy(I2CText, "I2C Buck converter is connected at address 0x5E.");
  }

  else {
    strcpy(I2CText, "I2C Buck converter is not connected.");
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
	String html_head = FPSTR(HTTP_HEAD_START);
	html_head += FPSTR(HTTP_STYLE);
	String html = FPSTR(HTTP_HEAD_END);    
	html.replace("{Caption}", "Main Info");
	html = html_head + html;
	html += FPSTR(HTTP_MAIN_DATA);
	html.replace("{FW}", FIRMWAREVERSION);
	html.replace("{curTemp}", String(curTemp,1));
	html.replace("{setTemp}", String(setTemp,1));
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
	String html_head = FPSTR(HTTP_HEAD_START);
	html_head += FPSTR(HTTP_STYLE);
	String html = FPSTR(HTTP_HEAD_END);    
	html.replace("{Caption}", "Settings");
	html = html_head + html;
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
	String html_head = FPSTR(HTTP_HEAD_START);
	html_head += FPSTR(HTTP_STYLE);
	String html = FPSTR(HTTP_HEAD_END);    
	html.replace("{Caption}", "PID Settings");
	html = html_head + html;
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

	restoreEEPROMdata();	// restore PID settings
	
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
	httpServer.on("/settings", handleSettings);
	httpServer.on("/settings_store", handleSettingsStore);
	httpServer.on("/pids", handlePIDs);
	httpServer.on("/pids_store", handlePIDsStore);

	httpServer.onNotFound ( handleNotFound );
//	httpServer.on ( "/inline", []() {
//    httpServer.send ( 200, "text/plain", "this works as well" );
//  } );
	
	httpUpdater.setup(&httpServer, "/update");
	httpServer.begin();
	Serial.println("HTTP server started");

    DS18B20Interval=millis();
    DS18B20.requestTemperatures(); 
    curTemp = DS18B20.getTempCByIndex(0);
	
	myPID.SetOutputLimits(minVoltage, maxVoltage);	// same limits as for Buck converter
	myPID.SetMode(AUTOMATIC);
	myPID.SetTunings(pid_kP, pid_kI, pid_kD);	// Set values restored from EEPROM
}

void loop() {
	httpServer.handleClient();
	
	// request temperature every minute
	if(millis()-DS18B20Interval>60000){
		DS18B20Interval=millis();
		DS18B20.requestTemperatures(); 
		curTemp = DS18B20.getTempCByIndex(0);
		// also calculate PID
		myPID.Compute();
		// we need to send new values to Buck Converter
		sendI2Cdata();
	}
	
	
	if(measuredVoltage<0.5){
		digitalWrite (LED_WARN, LOW);
	}else{
		digitalWrite (LED_WARN, HIGH);
	}

}

