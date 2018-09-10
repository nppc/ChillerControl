#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <PID_v1.h>
#include <EEPROM.h>
#include "FS.h"

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
unsigned long SendDataInterval;

#define filterSamples 5
int Temp_SmoothArray[filterSamples];   // array for holding raw sensor values for PT1000 sensor 


// Default IP in AP mode is 192.168.4.1
const char *ssid = "Cooler";
const char *password = "1234Test";

char IntSSID[20];
char IntPASS[20];
String thingWriteAPIKey;
char ubiToken[40];
char ubiDevice[20];
//const char* VARIABLE_LABEL = "box_temperature"; // Your variable label
//const char* USER_AGENT = "ESP8266";
//const char* VERSION = "1.0";
const char* ubiServer = "industrial.api.ubidots.com";
const char* thingServer = "api.thingspeak.com";

// Define a web server at port 80 for HTTP
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


const int Load1 = D5; // Cooler hot

float pid_kP=DEFAULT_PID_KP;
float pid_kI=DEFAULT_PID_KI;
float pid_kD=DEFAULT_PID_KD;
// Create PID with default values
PID myPID(&(curTemp), &(setVoltage), &(setTemp), DEFAULT_PID_KP, DEFAULT_PID_KI, DEFAULT_PID_KD, REVERSE);


// smooth algorytm for ADC reading
int digitalSmooth(int rawIn, int *sensSmoothArray){     // "int *sensSmoothArray" passes an array to the function - the asterisk indicates the array name is a pointer
  int j, k, temp, top, bottom;
  long total;
  static int i;
  static int sorted[filterSamples];
  boolean done;

  i = (i + 1) % filterSamples;    // increment counter and roll over if necc. -  % (modulo operator) rolls over variable
  sensSmoothArray[i] = rawIn;                 // input new data into the oldest slot

  for (j=0; j<filterSamples; j++){     // transfer data array into anther array for sorting and averaging
    sorted[j] = sensSmoothArray[j];
  }

  done = 0;                // flag to know when we're done sorting              
  while(done != 1){        // simple swap sort, sorts numbers from lowest to highest
    done = 1;
    for (j = 0; j < (filterSamples - 1); j++){
      if (sorted[j] > sorted[j + 1]){     // numbers are out of order - swap
        temp = sorted[j + 1];
        sorted [j+1] =  sorted[j] ;
        sorted [j] = temp;
        done = 0;
      }
    }
  }

  // throw out top and bottom 15% of samples - limit to throw out at least one from top and bottom
  bottom = max(((filterSamples * 15)  / 100), 1); 
  top = min((((filterSamples * 85) / 100) + 1  ), (filterSamples - 1));   // the + 1 is to make up for asymmetry caused by integer rounding
  k = 0;
  total = 0;
  for ( j = bottom; j< top; j++){
    total += sorted[j];  // total remaining indices
    k++; 
  }

  return total / k;    // divide by number of samples
}


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
    float pid_kP;
    float pid_kI;
    float pid_kD;
	  float setTemp;
  } EEdata;
	EEPROM.begin(40);
	EEPROM.get(0,EEdata);
  EEPROM.end();
	// assign data to variables and check for validity and if not valid leave default
	if(EEdata.pid_kP<=300.0 && EEdata.pid_kP>=0.0){pid_kP = EEdata.pid_kP;}
	if(EEdata.pid_kI<=300.0 && EEdata.pid_kI>=0.0){pid_kI = EEdata.pid_kI;}
	if(EEdata.pid_kD<=300.0 && EEdata.pid_kD>=0.0){pid_kD = EEdata.pid_kD;}
	if(EEdata.setTemp<=30.0 && EEdata.setTemp>=0.0){setTemp = EEdata.setTemp;}

  File f = SPIFFS.open("/settings.txt", "r");
  String s = f.readStringUntil('\n');
  s.trim();
  s.toCharArray(IntSSID,20);
  s = f.readStringUntil('\n');
  s.trim();
  s.toCharArray(IntPASS,20);
  thingWriteAPIKey = f.readStringUntil('\n');
  f.close();  
}

void saveEEPROMdata(){
 struct {
    float pid_kP;
    float pid_kI;
    float pid_kD;
	  float setTemp;
  } EEdata;
  
	//EEPROM.get(0,EEdata);	// somebody explained that this is still needed....
	// fill struct with correct data
	EEdata.pid_kP=pid_kP;
	EEdata.pid_kI=pid_kI;
	EEdata.pid_kD=pid_kD;
	EEdata.setTemp=setTemp;
  EEPROM.begin(40);
	EEPROM.put(0,EEdata);
  EEPROM.end();
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
	String html_head = FPSTR(HTTP_HEAD_START);
	html_head += FPSTR(HTTP_STYLE);
	String html = FPSTR(HTTP_HEAD_END);    
	html.replace("{Caption}", "Main Info");
	html = html_head + html;
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

void setup() {
	pinMode (Load1, OUTPUT);
	digitalWrite (Load1, HIGH);
	pinMode (LED_WARN, OUTPUT);
	digitalWrite (LED_WARN, LOW);
  
  SPIFFS.begin();

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
  
	restoreEEPROMdata();	// restore PID and internet settings

  	
	Serial.println();
	Serial.println("Configuring access point...");
	
	//You can remove the password parameter if you want the AP to be open.
	WiFi.softAP(ssid, password);

  WiFi.mode(WIFI_AP_STA);

	IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);

  Serial.println();
  Serial.println("Connecting to Internet...");

  WiFi.begin(IntSSID, IntPASS);

    for(byte i1=0;i1<filterSamples*2;i1++){
      DS18B20.requestTemperatures(); 
      delay(1000);
      int tmpT = (int)round(DS18B20.getTempCByIndex(0)*100.0);
      curTemp = (float)digitalSmooth(tmpT, Temp_SmoothArray) / 100.0;
    }

  int i = 20;
  while (WiFi.status() != WL_CONNECTED && i>0) {
    delay(500);
    Serial.print(i);
    i--;
    }

  if(WiFi.status() == WL_CONNECTED){
    Serial.print("Internet IP address: ");
    Serial.println(WiFi.localIP());
    WiFi.setAutoReconnect(true);
  }else{
    Serial.print("Internet connection failed.");
    Serial.print(" Error is: ");
    Serial.println(WiFi.status());
    WiFi.setAutoReconnect(false);
    WiFi.disconnect();
  }
//  WIFIlocIP = WiFi.localIP();


	httpServer.on ( "/", handleRoot );
	httpServer.on("/setTemp", handleSetTemp);
	httpServer.on("/settings", handleSettings);
	httpServer.on("/settings_store", handleSettingsStore);
	httpServer.on("/pids", handlePIDs);
	httpServer.on("/pids_store", handlePIDsStore);
  httpServer.on("/stop", handleStop);

	httpServer.onNotFound ( handleNotFound );
//	httpServer.on ( "/inline", []() {
//    httpServer.send ( 200, "text/plain", "this works as well" );
//  } );
	
	httpUpdater.setup(&httpServer, "/update");
	httpServer.begin();
	Serial.println("HTTP server started");

    SendDataInterval=millis();

    DS18B20Interval=millis();
	
	myPID.SetOutputLimits(minVoltage, maxVoltage);	// same limits as for Buck converter
	myPID.SetMode(AUTOMATIC);
	myPID.SetTunings(pid_kP, pid_kI, pid_kD);	// Set values restored from EEPROM
}

/*
void send2Ubidots(){
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
}
*/

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
    clientSend.println("X-THINGSPEAKAPIKEY: " + String(thingWriteAPIKey));
    clientSend.println("Content-Type: application/x-www-form-urlencoded");
    clientSend.println("Content-Length: " + String(body.length()));
    clientSend.println("");
    clientSend.print(body);

  }
  clientSend.stop();

}


void loop() {
	httpServer.handleClient();
  delay(1);
	
	// request temperature every minute
	if(millis()-DS18B20Interval>10000){
		DS18B20Interval=millis();
		DS18B20.requestTemperatures(); 
    delay(1000);
    int tmpT = (int)round(DS18B20.getTempCByIndex(0)*100.0);
    curTemp = (float)digitalSmooth(tmpT, Temp_SmoothArray) / 100.0;
		// also calculate PID if needed
		if(setTemp!=999.0){
		  myPID.Compute();
		}else{
      setVoltage-=1.0;
		  if(setVoltage<0.0){setVoltage=0.0;}
		}
		// we need to send new values to Buck Converter
		sendI2Cdata();
	  // Send data to Ubidots
	  if(millis()-SendDataInterval>60000){
  	  SendDataInterval = millis();
  	  //send2Ubidots();
      send2Thingspeak();
	  }
	}
	
	
	if(measuredVoltage<0.5){
		digitalWrite (LED_WARN, LOW);
	}else{
		digitalWrite (LED_WARN, HIGH);
	}

}

