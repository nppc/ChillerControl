#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <PID_v1.h>
//#include <EEPROM.h>
#include <ArduinoJson.h>
#include "FS.h"

#include "globals.h"
#include "webpage.htm.h"

#define DEBUG

#define I2C_ADDRESS 0x5E  // Buck converter address
#define I2C_DATALEN 11  // 11 bytes

// pins definition
#define LED_WARN D0   // LED indicates that peltie is under voltage and can't be just powered off
#define ONE_WIRE_BUS D6  // DS18B20 pin
#define HOT_FAN D5		// Cooling fan for hot radiator

#ifdef DEBUG
String debugOut ="";
#endif


bool I2C_PRESENT;
uint8_t i2c_data[I2C_DATALEN-1]; //11 bytes

double ColdTemp;	// current measured temperature inside the box
double HotTemp;	// current measured temperature of hot radiator
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
unsigned long millisDS18B20Interval;
unsigned long millisSendDataInterval;
unsigned long millisReadI2CDataInterval;

int ColdSensorId = 0;
int HotSensorId = 0;

#define filterSamples 5
int ColdTemp_SmoothArray[filterSamples];   // array for holding raw sensor values for PT1000 sensor 
int HotTemp_SmoothArray[filterSamples];   // array for holding raw sensor values for PT1000 sensor 


// Default IP in AP mode is 192.168.4.1
const char *ssid = "Cooler";
const char *password = "1234Test";

char IntSSID[20];
char IntPASS[20];
String thingWriteAPIKey;
String ubiToken;
String ubiDevice;
int sendInterval=600;  // Interval in seconds for sending data to IoT server
int sendUbi_checked=0;
int sendThing_checked=0;


// Define a web server at port 80 for HTTP
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

float pid_kP=DEFAULT_PID_KP;
float pid_kI=DEFAULT_PID_KI;
float pid_kD=DEFAULT_PID_KD;
// Create PID with default values
PID myPID(&(ColdTemp), &(setVoltage), &(setTemp), DEFAULT_PID_KP, DEFAULT_PID_KI, DEFAULT_PID_KD, REVERSE);


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

void restoreSettings(){
	//StaticJsonBuffer<1024> jsonBuffer;
	DynamicJsonBuffer jsonBuffer;
	File f = SPIFFS.open("/settings.json", "r");
	JsonObject &jobj = jsonBuffer.parseObject(f);
	f.close();
	String s = jobj["IntSSID"].as<String>();s.toCharArray(IntSSID,20);
	s = jobj["IntPASS"].as<String>();s.toCharArray(IntPASS,20);
	thingWriteAPIKey = jobj["thingWriteAPIKey"].as<String>();
	ubiToken = jobj["ubiToken"].as<String>();
	ubiDevice = jobj["ubiDevice"].as<String>();
	sendInterval = jobj["sendInterval"];
	sendThing_checked = jobj["sendThing_checked"];
	sendUbi_checked = jobj["sendUbi_checked"];
	setTemp = jobj["setTemp"];
	pid_kP = jobj["pid_kP"];
	pid_kI = jobj["pid_kI"];
	pid_kD = jobj["pid_kD"];
	ColdSensorId = jobj["ColdSensorId"];
	HotSensorId = jobj["HotSensorId"];

//	if(EEdata.pid_kP<=300.0 && EEdata.pid_kP>=0.0){pid_kP = EEdata.pid_kP;}
//	if(EEdata.pid_kI<=300.0 && EEdata.pid_kI>=0.0){pid_kI = EEdata.pid_kI;}
//	if(EEdata.pid_kD<=300.0 && EEdata.pid_kD>=0.0){pid_kD = EEdata.pid_kD;}
//	if(EEdata.setTemp<=30.0 && EEdata.setTemp>=0.0){setTemp = EEdata.setTemp;}
	
	
}

void saveSettings(){
	const char *filename = "/settings.json";
	
	//if(SPIFFS.exists(filename)) SPIFFS.remove(filename);

	File f = SPIFFS.open(filename, "w");

	DynamicJsonBuffer jsonBuffer;
	JsonObject &jobj = jsonBuffer.createObject();
	jobj["IntSSID"] = String(IntSSID);
	jobj["IntPASS"] = String(IntPASS);
	jobj["thingWriteAPIKey"] = thingWriteAPIKey;
	jobj["ubiToken"] = ubiToken;
	jobj["ubiDevice"] = ubiDevice;
	jobj["sendInterval"] = sendInterval;
	jobj["sendThing_checked"] = sendThing_checked;
	jobj["sendUbi_checked"] = sendUbi_checked;
	jobj["setTemp"] = setTemp;
	jobj["pid_kP"] = pid_kP;
	jobj["pid_kI"] = pid_kI;
	jobj["pid_kD"] = pid_kD;
	jobj["ColdSensorId"] = ColdSensorId;
	jobj["HotSensorId"] = HotSensorId;
	
	jobj.printTo(f);
	f.close(); 
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


void setup() {
	pinMode (HOT_FAN, OUTPUT);
	digitalWrite (HOT_FAN, HIGH);
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
  
	millisReadI2CDataInterval = millis();
	
	restoreSettings();	// restore PID and internet settings

  	
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
      int tmpT = (int)round(DS18B20.getTempCByIndex(ColdSensorId)*100.0);
      ColdTemp = (float)digitalSmooth(tmpT, ColdTemp_SmoothArray) / 100.0;
      tmpT = (int)round(DS18B20.getTempCByIndex(HotSensorId)*100.0);
      HotTemp = (float)digitalSmooth(tmpT, HotTemp_SmoothArray) / 100.0;
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
	httpServer.on("main.js", handleCSS);
	httpServer.on("/setTemp", handleSetTemp);
	httpServer.on("/settings", handleSettings);
	httpServer.on("/settings_store", handleSettingsStore);
	httpServer.on("/pids", handlePIDs);
	httpServer.on("/pids_store", handlePIDsStore);
	httpServer.on("/stop", handleStop);
	httpServer.on("/network", handleNetwork);
	httpServer.on("webpage.css", handleCSS);
	httpServer.on("/network_store", handleNetworkStore);
	httpServer.on("/ajax", handleAJAXrequest);
  
	handleFSWebServer();

//	httpServer.onNotFound ( handleNotFound );
//	httpServer.on ( "/inline", []() {
//    httpServer.send ( 200, "text/plain", "this works as well" );
//  } );
	
	httpUpdater.setup(&httpServer, "/update");
	httpServer.begin();
	Serial.println("HTTP server started");

    millisSendDataInterval=millis();

    millisDS18B20Interval=millis();
	
	myPID.SetOutputLimits(minVoltage, maxVoltage);	// same limits as for Buck converter
	myPID.SetMode(AUTOMATIC);
	myPID.SetTunings(pid_kP, pid_kI, pid_kD);	// Set values restored from EEPROM
}



void loop() {
	httpServer.handleClient();
	delay(1);
	
	// request temperature every minute
	if(millis()-millisDS18B20Interval>10000){
		millisDS18B20Interval=millis();
		DS18B20.requestTemperatures(); 
		delay(1000);
		int tmpT = (int)round(DS18B20.getTempCByIndex(ColdSensorId)*100.0);
		ColdTemp = (float)digitalSmooth(tmpT, ColdTemp_SmoothArray) / 100.0;
		tmpT = (int)round(DS18B20.getTempCByIndex(HotSensorId)*100.0);
		HotTemp = (float)digitalSmooth(tmpT, HotTemp_SmoothArray) / 100.0;
		// also calculate PID if needed. But if radiator is very hot, reduce power.
		if(setTemp!=999.0 && HotTemp<RADIATOR_EXTREME_TEMP){
			myPID.Compute();
		}else{
			setVoltage-=1.0;
			if(setVoltage<0.0){setVoltage=0.0;}
		}
		// we need to send new values to Buck Converter
		sendI2Cdata();
		// Send data to Ubidots
		if(millis()-millisSendDataInterval>(sendInterval * 1000)){
			millisSendDataInterval = millis();
			if(sendUbi_checked==1){send2Ubidots();}
			if(sendThing_checked==1){send2Thingspeak();}
		}
		// control Fan
		if(HotSensorId==ColdSensorId){
			digitalWrite(HOT_FAN, HIGH);	// Fan always ON if only one temperature sensor detected
		}else{
			if(HotTemp>=32.0 && digitalRead(HOT_FAN)==LOW){
				digitalWrite(HOT_FAN, HIGH);
			}else if(HotTemp<=24.0 && digitalRead(HOT_FAN)==HIGH){
				digitalWrite(HOT_FAN, LOW);
			}		
		}
	}
	
	if(millis()-millisReadI2CDataInterval>2000){
		millisReadI2CDataInterval = millis();
		readI2Cdata();
	}
	
	if(measuredVoltage<0.5){
		digitalWrite(LED_WARN, LOW);
	}else{
		digitalWrite(LED_WARN, HIGH);
	}

}

