#include "CoolerBox.h"

DebugHTML Debug;

unsigned long I2C_error;
bool I2C_PRESENT;
uint8_t i2c_data[I2C_DATALEN];

bool flagExtremeTemp = LOW;
float maxVoltage_backup; // backup original value

double boxTemp;	// current measured temperature inside the box
double HotTemp;	// current measured temperature of hot radiator
double setTemp;	// preset temperature
double dynamicTemp; // The option to adjust setTemp dynamically
#if ENABLE_ISPINDEL_DYNAMIC_TEMP
double iSpindelTemperature;
float dynamicTempDiffCoeff = 2.0;
#endif
double setVoltage;
float changeVoltageSpeed; 
float minVoltage; 
float maxVoltage; 
byte i2c_PWM_value;
float measuredVoltage; 
float measuredCurrent; 
bool stopCooler = LOW;
double setHotPWM=0.0; // initially off
int maxHotPWM = MAXIMUM_PWM_HOT_WIRE; 

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
DeviceAddress  DS18B20_adr[2];
unsigned long millisPIDinterval;
unsigned long millisSendDataInterval;
unsigned long millisReadI2CDataInterval;
unsigned long millisWIFIcheckInterval;

int ColdSensorId = 0;
int HotSensorId = 0;

int boxTemp_SmoothArray[filterSamples];   // array for holding raw sensor values for PT1000 sensor 
int HotTemp_SmoothArray[filterSamples];   // array for holding raw sensor values for PT1000 sensor 


// Default IP in AP mode is 192.168.4.1
const char *ssid = "Cooler";
const char *password = "1234Test";

char IntSSID[20];
char IntPASS[20];
String endpointAddress;
String endpointAccessToken;
String endpointPath;
int sendInterval = 60;  // Interval in seconds for sending data to the endpoint
int fanDynamic_checked = 0;
int boxMode = 0; // 1-Cooling/Heating, 2-Cooling, 3-Heating
int boxSubMode = 3; // 2-Cooling, 3-Heating

// Define a web server at port 80 for HTTP
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

float coldPID_kP=DEFAULT_PID_KP;
float coldPID_kI=DEFAULT_PID_KI;
float coldPID_kD=DEFAULT_PID_KD;
float hotPID_kP=DEFAULT_PID_KP;
float hotPID_kI=DEFAULT_PID_KI;
float hotPID_kD=DEFAULT_PID_KD;

// Create cold PID with default values
PID coldPID(&(boxTemp), &(setVoltage), &(dynamicTemp), coldPID_kP, coldPID_kI, coldPID_kD, REVERSE); // reverse as it works for cooling
// Create hot PID with default values
PID hotPID(&(boxTemp), &(setHotPWM), &(dynamicTemp), hotPID_kP, hotPID_kI, hotPID_kD, DIRECT); // no reverse as it works for heating


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
	JsonDocument doc;
	File f = SPIFFS.open("/settings.json", "r");
	if (!f) {
		Debug.addLine(F("SETTINGS OPEN FAIL"));
		return;
	}
	DeserializationError error = deserializeJson(doc, f);
	f.close();
	if (error) {
		Debug.addLine(String(F("SETTINGS JSON FAIL: ")) + error.c_str());
		return;
	}
	String s = doc["IntSSID"].as<String>();s.toCharArray(IntSSID,20);
	s = doc["IntPASS"].as<String>();s.toCharArray(IntPASS,20);
	endpointAddress = doc["endpointAddress"].as<String>();
	endpointAccessToken = doc["endpointAccessToken"].as<String>();
	endpointPath = doc["endpointPath"].as<String>();
	sendInterval = doc["sendInterval"] | 60;
	if (sendInterval < 10 || sendInterval > 86400) {
		sendInterval = 60;
	}
	setTemp = doc["setTemp"];
	dynamicTemp = setTemp;
	coldPID_kP = doc["coldPID_kP"];
	coldPID_kI = doc["coldPID_kI"];
	coldPID_kD = doc["coldPID_kD"];
	hotPID_kP = doc["hotPID_kP"];
	hotPID_kI = doc["hotPID_kI"];
	hotPID_kD = doc["hotPID_kD"];
	ColdSensorId = doc["ColdSensorId"];
	HotSensorId = doc["HotSensorId"];
	fanDynamic_checked = doc["fanDynamic_checked"];
	boxMode = doc["boxMode"];
	// read settings for i2c DC/DC converter
	minVoltage = doc["i2cDCDC_minVoltage"];
	maxVoltage = doc["i2cDCDC_maxVoltage"];
	changeVoltageSpeed = doc["i2cDCDC_changeVoltage"];
		
}

void saveSettings(){
	const char *filename = "/settings.json";
	
	//if(SPIFFS.exists(filename)) SPIFFS.remove(filename);

	File f = SPIFFS.open(filename, "w");

	JsonDocument doc;
	doc["IntSSID"] = String(IntSSID);
	doc["IntPASS"] = String(IntPASS);
	doc["endpointAddress"] = endpointAddress;
	doc["endpointAccessToken"] = endpointAccessToken;
	doc["endpointPath"] = endpointPath;
	doc["sendInterval"] = sendInterval;
	doc["setTemp"] = setTemp;
	doc["coldPID_kP"] = coldPID_kP;
	doc["coldPID_kI"] = coldPID_kI;
	doc["coldPID_kD"] = coldPID_kD;
	doc["hotPID_kP"] = hotPID_kP;
	doc["hotPID_kI"] = hotPID_kI;
	doc["hotPID_kD"] = hotPID_kD;
	doc["ColdSensorId"] = ColdSensorId;
	doc["HotSensorId"] = HotSensorId;
	doc["fanDynamic_checked"] = fanDynamic_checked;
	doc["boxMode"] = boxMode;
	// store settings for i2c DC/DC converter
	doc["i2cDCDC_minVoltage"] = minVoltage;
	doc["i2cDCDC_maxVoltage"] = maxVoltage;
	doc["i2cDCDC_changeVoltage"] = changeVoltageSpeed;
	
	serializeJson(doc, f);
	f.close(); 
}


void sendI2Cdata(){
	if(I2C_PRESENT){
		Wire.beginTransmission(I2C_ADDRESS); //Start bit
		Wire.write((uint8_t)(setVoltage * 10.0)); // Set Voltage
		Wire.write((uint8_t)(changeVoltageSpeed * 10.0)); // Speed of voltage change (means 0.4v/s)
		Wire.write((uint8_t)(minVoltage * 10.0)); //Min Voltage
		Wire.write((uint8_t)(maxVoltage * 10.0)); //Max Voltage
		Wire.write(i2c_PWM_value); //No affect on Buck Converter
		Wire.write((uint8_t)(measuredVoltage * 10.0)); //No affect on Buck Converter
		Wire.write((uint8_t)(measuredCurrent * 10.0)); //No affect on Buck Converter
		Wire.write(0); //Only filled in Read
		Wire.write(0); //Only filled in Read
		Wire.write(0); //Only filled in Read
		Wire.write(0); //Only filled in Read
		byte error = Wire.endTransmission();
		if (error==0){
			I2C_PRESENT=HIGH;
			Serial.println(F("I2C Buck converter fund at address 0x5E"));
		}else{
			I2C_PRESENT=LOW;
			Serial.println(F("I2C Buck converter is not connected."));
			I2C_error++;
		}
	} else {
		// check i2c again
		Wire.beginTransmission(I2C_ADDRESS);
		byte error = Wire.endTransmission();
		if (error==0){
			I2C_PRESENT=HIGH;
			Serial.println(F("I2C Buck converter fund at address 0x5E"));
		}else{
			I2C_PRESENT=LOW;
			Serial.println(F("I2C Buck converter is not connected."));
		}
	}
}

void readI2Cdata(){
    // Read data from I2CBuck
  if(I2C_PRESENT){
    Serial.print(F("I2C bytes Available "));
    Wire.requestFrom(I2C_ADDRESS, I2C_DATALEN);
    uint8_t b_avail = Wire.available();
	Serial.print(b_avail);
    Serial.print(": ");
    uint8_t i=0;
    while(Wire.available()!=0) {
      if(b_avail==I2C_DATALEN) i2c_data[i] = Wire.read();
      Serial.print(i2c_data[i]);
      Serial.print(" ");
      i++;
    }
	if(b_avail!=I2C_DATALEN) I2C_error++;
    Serial.println();
  }else{
    // clear array
    for(uint8_t i=0;i<I2C_DATALEN;i++){i2c_data[i]=0;}
  }
  // assign to variables
  setVoltage = (float)i2c_data[0]/10.0;
  //changeVoltageSpeed = (float)i2c_data[1]/10.0;
  //minVoltage = (float)i2c_data[2]/10.0; 
  //maxVoltage = (float)i2c_data[3]/10.0; 
  i2c_PWM_value = i2c_data[4];
  measuredVoltage = (float)i2c_data[5]/10.0; 
  measuredCurrent = (float)i2c_data[6]/10.0; 
  measuredCurrent = (measuredCurrent>13.0 ? 0 : measuredCurrent); // hack. To lazy to fix buck converter code. :)
}


void setup() {
	pinMode (HOT_FAN, OUTPUT);
  pinMode (HEAT_WIRE, OUTPUT);

  analogWriteFreq(100); // 100hz for heating
  analogWrite(HEAT_WIRE, 0);
  
	digitalWrite (HOT_FAN, HIGH);
	pinMode (LED_WARN, OUTPUT);
	digitalWrite (LED_WARN, LOW);

  Debug.init();
  
	boxMode=1;	// set some mode in case this setting not yet present
	
	millisWIFIcheckInterval = millis();
  
	SPIFFS.begin();

	INIT_DS18B20();

	Wire.begin(D2,D1);  // D2-sda, D1-scl
	delay(100); // this delay is very essential for proper working of I2C line
	Wire.setClock(50000); // higher rate is unstable
	Wire.setClockStretchLimit(600); // this should be checked

	Serial.begin(115200);

	 // Detect I2C device
	I2C_error=0;
	Wire.beginTransmission(I2C_ADDRESS);
	byte error = Wire.endTransmission();
	if (error==0){
		I2C_PRESENT=HIGH;
		Serial.println(F("I2C Buck converter fund at address 0x5E"));
	}else{
		I2C_PRESENT=LOW;
		Serial.println(F("I2C Buck converter is not connected."));
	}
   
	delay(500);

	//readI2Cdata();
  
	millisReadI2CDataInterval = millis();
	
	restoreSettings();	// restore PID and internet settings

	maxVoltage_backup = maxVoltage; // update backup
  	
	Serial.println();
	Serial.println("Configuring access point...");
	
	//You can remove the password parameter if you want the AP to be open.
	WiFi.softAP(ssid, password);

	WiFi.persistent(false); // don't write wifi settings to flash
	WiFi.mode(WIFI_AP_STA);

	IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);

	Serial.println();
	Serial.println("Connecting to Internet...");

	WiFi.setAutoConnect(false);
	WiFi.setAutoReconnect(true);
	WiFi.begin(IntSSID, IntPASS);

    for(byte i1=0;i1<filterSamples*2;i1++){
      DS18B20.requestTemperatures(); 
      delay(1000);
      int tmpT = (int)round(DS18B20.getTempCByIndex(ColdSensorId)*100.0);
      boxTemp = (float)digitalSmooth(tmpT, boxTemp_SmoothArray) / 100.0;
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
    Serial.print(F("Internet IP address: "));
    Serial.println(WiFi.localIP());
  }else{
    Serial.print(F("Internet connection failed."));
    Serial.print(F(" Error is: "));
    Serial.println(WiFi.status());
    //WiFi.disconnect();
  }
//  WIFIlocIP = WiFi.localIP();


	httpServer.on ( "/", handleRoot );
	httpServer.on("/main.js", handleScript);
  //httpServer.serveStatic("main.js", SPIFFS, "/main.js");
	httpServer.on("/setTemp", handleSetTemp);
	httpServer.on("/settings", handleSettings);
	httpServer.on("/settings_store", handleSettingsStore);
	httpServer.on("/pids", handlePIDs);
	httpServer.on("/pids_store", handlePIDsStore);
	httpServer.on("/stop", handleStop);
	httpServer.on("/network", handleNetwork);
	httpServer.on("/debug", handleDebug);
	httpServer.on("/webpage.css", handleCSS);
	//httpServer.serveStatic("webpage.css", SPIFFS, "/webpage.css");
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

    millisPIDinterval=millis();
	
	coldPID.SetOutputLimits(minVoltage, maxVoltage);
	coldPID.SetMode(AUTOMATIC);
	coldPID.SetTunings(coldPID_kP, coldPID_kI, coldPID_kD);
  hotPID.SetOutputLimits(0, maxHotPWM);  // same limits for hot wire PWM
  hotPID.SetMode(AUTOMATIC);
  hotPID.SetTunings(hotPID_kP, hotPID_kI, hotPID_kD);
}



void loop() {
	//check connection every 5 minutes
	if(millis()-millisWIFIcheckInterval>(300 * 1000)){
		if(WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0,0,0,0)) {
			WiFi.reconnect();
			delay(1000);
		}
	}

	httpServer.handleClient();
	delay(1);
	
	if(millis()-millisPIDinterval>(PIDCOMPUTE_INTERVAL * 1000)){
		millisPIDinterval=millis();
		// before computing PID we want latest temperatures
		DS18B20.requestTemperatures(); 
		delay(1000);
		int tmpT = (int)round(DS18B20.getTempCByIndex(ColdSensorId)*100.0);
		boxTemp = (float)digitalSmooth(tmpT, boxTemp_SmoothArray) / 100.0;
		tmpT = (int)round(DS18B20.getTempCByIndex(HotSensorId)*100.0);
		HotTemp = (float)digitalSmooth(tmpT, HotTemp_SmoothArray) / 100.0;
#if ENABLE_ISPINDEL_DYNAMIC_TEMP
		if (iSpindelTemperature > 0) {
			dynamicTemp = setTemp + ((setTemp - iSpindelTemperature) * dynamicTempDiffCoeff);
		}
#endif
		// should we swithc boxSubMode?
		if(boxMode==1){
			if(boxSubMode==2){ //Cooling
				if(boxTemp<(dynamicTemp-0.5)){
					boxSubMode=3;	// Switch to heating mode
				}
			}else{
				if(boxTemp>(dynamicTemp+0.5)){
					boxSubMode=2;	// Switch to cooling mode
          setHotPWM = 0.0;
				}
			}
		}
		
		if(boxMode!=3){
			// Cooler routines
			// If radiator is very hot, reduce power (decrease max voltage allowed).
			if(HotTemp>=RADIATOR_EXTREME_TEMP && !flagExtremeTemp) {flagExtremeTemp = HIGH;}
			if(flagExtremeTemp){
				maxVoltage = maxVoltage + (HotTemp>=RADIATOR_EXTREME_TEMP ? -0.1 : 0.1); // change voltage slowly to avoid spikes
				if(maxVoltage>maxVoltage_backup){maxVoltage=maxVoltage_backup;}
				if(maxVoltage<minVoltage){maxVoltage=minVoltage;}
				coldPID.SetOutputLimits(minVoltage, maxVoltage);  // set new max limit to the PID
				if(HotTemp<RADIATOR_EXTREME_TEMP && maxVoltage==maxVoltage_backup){flagExtremeTemp = LOW;}
			}
			// if we in stopping process then just reduce the voltage every PID loop for 1v
			// also calculate PID if not stopping
			if((!stopCooler && boxMode!=1) || (!stopCooler && boxMode==1 && boxSubMode==2) ){
				coldPID.Compute();
			}else{
				setVoltage-=1; // reduce current voltage
				if(setVoltage<0.0){setVoltage=0.0;}
			}
			// we need to send new values to Buck Converter
			sendI2Cdata();
		}else{
		  //make sure cooler is off
		  setVoltage=0;
		  sendI2Cdata();
		}
		if((boxMode==3 && measuredVoltage<3.0) || (boxMode==1 && boxSubMode==3)){ // make sure that peltie is not taking much current
/*
		  // Prevent I windup at edge values
		  if((setHotPWM==maxHotPWM || setHotPWM==0.0) && hotPID.GetKi()!=0.0){
        hotPID.SetTunings(hotPID_kP, 0.0, hotPID_kD);
		  }else if((setHotPWM!=maxHotPWM && setHotPWM!=0.0) && hotPID.GetKi()==0.0){
        hotPID.SetTunings(hotPID_kP, hotPID_kI, hotPID_kD);
		  }
*/
      // now compute PID
		  hotPID.Compute();
		}else{
		  //make sure heater is off
		  setHotPWM = 0.0;
		}      
		analogWrite(HEAT_WIRE, setHotPWM);
   
		// Send data to IoT
		if(millis()-millisSendDataInterval>static_cast<unsigned long>(sendInterval) * 1000UL){
			millisSendDataInterval = millis();
			sendTelemetry();
		}		
		// control Fan
		if(HotSensorId==ColdSensorId || fanDynamic_checked==0){
			// Fan always ON if only one temperature sensor detected or Dynamic control is disabled
			// Also check if heating is on, then Fan should be off
			digitalWrite(HOT_FAN, (boxMode==3 || (boxMode==1 && boxSubMode==3) ? LOW : HIGH));	// Fan is OFF when heating.
		}else{
			if(HotTemp>=32.0 && digitalRead(HOT_FAN)==LOW){
				digitalWrite(HOT_FAN, HIGH);
			}else if(HotTemp<=24.0 && digitalRead(HOT_FAN)==HIGH){
				digitalWrite(HOT_FAN, LOW);
			}		
		}
	}
	
	if(millis()-millisReadI2CDataInterval>(I2C_INTERVAL * 1000)){
		millisReadI2CDataInterval = millis();
		readI2Cdata();
	}
	
	if(measuredVoltage<2.0){
		digitalWrite(LED_WARN, LOW);
	}else{
		digitalWrite(LED_WARN, HIGH);
	}

}
