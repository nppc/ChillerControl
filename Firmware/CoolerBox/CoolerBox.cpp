#include "CoolerBox.h"

DebugHTML Debug;

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
float measuredVoltage; 
float measuredCurrent; 
bool stopCooler = LOW;
double setHotPWM=0.0; // initially off
int maxHotPWM = MAXIMUM_PWM_HOT_WIRE; 

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
DeviceAddress  DS18B20_adr[2];
uint8_t DS18B20_count = 0;
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

namespace {
const size_t SETTINGS_EEPROM_SIZE = 1024;
const int SETTINGS_EEPROM_HEADER_SIZE = sizeof(uint32_t) + sizeof(uint16_t);
const uint32_t SETTINGS_EEPROM_MAGIC = 0x434F4F4C;
bool settingsEepromReady = false;

void applySettings(JsonDocument &doc) {
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
	hotPID_kP = doc["hotPID_kP"];
	hotPID_kI = doc["hotPID_kI"];
	hotPID_kD = doc["hotPID_kD"];
	ColdSensorId = doc["ColdSensorId"];
	HotSensorId = doc["HotSensorId"];
	fanDynamic_checked = doc["fanDynamic_checked"];
	boxMode = doc["boxMode"];
	minVoltage = doc["i2cDCDC_minVoltage"];
	maxVoltage = doc["i2cDCDC_maxVoltage"];
	changeVoltageSpeed = doc["i2cDCDC_changeVoltage"];
}
}

float hotPID_kP=DEFAULT_PID_KP;
float hotPID_kI=DEFAULT_PID_KI;
float hotPID_kD=DEFAULT_PID_KD;

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
  DS18B20_count = 0;
  oneWire.reset_search();

  DeviceAddress address;
  while (DS18B20_count < 2 && oneWire.search(address)) {
    if (OneWire::crc8(address, 7) != address[7] || address[0] != 0x28) {
      continue;
    }

    memcpy(DS18B20_adr[DS18B20_count], address, sizeof(DeviceAddress));
    DS18B20_count++;
  }

  for (uint8_t x = 0; x < DS18B20_count; x++) {
    DS18B20.setResolution(DS18B20_adr[x], 12);
  }

  if (DS18B20_count == 0) {
    Debug.addLine(F("No DS18B20 found by direct OneWire scan"));
  }
}

bool restoreSettingsFromEEPROM(){
	if (!settingsEepromReady) {
		Debug.addLine(F("EEPROM not initialized; settings not restored"));
		return false;
	}

	uint32_t magic;
	uint16_t length;
	EEPROM.get(0, magic);
	EEPROM.get(sizeof(magic), length);
	if (magic != SETTINGS_EEPROM_MAGIC ||
		length == 0 ||
		length > SETTINGS_EEPROM_SIZE - SETTINGS_EEPROM_HEADER_SIZE) {
		Debug.addLine(F("EEPROM settings header invalid"));
		return false;
	}

	String serialized;
	serialized.reserve(length);
	for (uint16_t i = 0; i < length; ++i) {
		serialized += static_cast<char>(EEPROM.read(SETTINGS_EEPROM_HEADER_SIZE + i));
	}

	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, serialized);
	if (error) {
		Debug.addLine(String(F("EEPROM settings JSON FAIL: ")) + error.c_str());
		return false;
	}

	applySettings(doc);
	Debug.addLine(F("Settings restored from EEPROM"));
	return true;
}

void restoreSettings(){
	if (restoreSettingsFromEEPROM()) {
		return;
	}

	JsonDocument doc;

	File f = LittleFS.open("/settings.json", "r");
	if (!f) {
		Debug.addLine(F("SETTINGS OPEN FAIL; using defaults"));
		return;
	}
	DeserializationError error = deserializeJson(doc, f);
	f.close();
	if (error) {
		Debug.addLine(String(F("SETTINGS JSON FAIL: ")) + error.c_str());
		return;
	}
	applySettings(doc);
	Debug.addLine(F("Migrating settings from LittleFS to EEPROM"));
	saveSettings();
}

void saveSettings(){
	if (!settingsEepromReady) {
		Debug.addLine(F("EEPROM not initialized; settings not saved"));
		return;
	}
	JsonDocument doc;
	doc["IntSSID"] = String(IntSSID);
	doc["IntPASS"] = String(IntPASS);
	doc["endpointAddress"] = endpointAddress;
	doc["endpointAccessToken"] = endpointAccessToken;
	doc["endpointPath"] = endpointPath;
	doc["sendInterval"] = sendInterval;
	doc["setTemp"] = setTemp;
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
	
	String serialized;
	serializeJson(doc, serialized);
	if (serialized.isEmpty() ||
		serialized.length() > SETTINGS_EEPROM_SIZE - SETTINGS_EEPROM_HEADER_SIZE) {
		Debug.addLine(F("Settings exceed EEPROM capacity"));
		return;
	}

	uint16_t length = serialized.length();
	for (uint16_t i = 0; i < length; ++i) {
		EEPROM.write(SETTINGS_EEPROM_HEADER_SIZE + i, serialized[i]);
	}
	EEPROM.put(0, SETTINGS_EEPROM_MAGIC);
	EEPROM.put(sizeof(SETTINGS_EEPROM_MAGIC), length);
	if (!EEPROM.commit()) {
		Debug.addLine(F("EEPROM settings commit failed"));
		return;
	}
	Debug.addLine(F("Settings saved to EEPROM"));
}

void setCoolerStopped(bool stopped) {
	if (stopCooler == stopped) {
		return;
	}
	stopCooler = stopped;
	if (stopped) {
		setVoltage = 0.0;
		sendI2Cdata();
	}
	Debug.addLine(stopped ? F("Cooler stopped") : F("Cooler resumed"));
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

	EEPROM.begin(SETTINGS_EEPROM_SIZE);
	settingsEepromReady = true;
  
	if (!LittleFS.begin()) {
		Debug.addLine(F("LittleFS mount failed"));
		Serial.println(F("LittleFS mount failed; filesystem settings and files are unavailable."));
	}

	INIT_DS18B20();

	Serial.begin(115200);
	initI2C();
	
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
  //httpServer.serveStatic("main.js", LittleFS, "/main.js");
	httpServer.on("/setTemp", handleSetTemp);
	httpServer.on("/settings", handleSettings);
	httpServer.on("/settings_store", handleSettingsStore);
	httpServer.on("/pids", handlePIDs);
	httpServer.on("/pids_store", handlePIDsStore);
	httpServer.on("/stop", handleStop);
	httpServer.on("/network", handleNetwork);
	httpServer.on("/debug", handleDebug);
	httpServer.on("/webpage.css", handleCSS);
	//httpServer.serveStatic("webpage.css", LittleFS, "/webpage.css");
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
		
		static bool heatingOnlyLogged = false;
		if(boxMode!=3){
			heatingOnlyLogged = false;
			// Cooler routines
			// If radiator is very hot, reduce power (decrease max voltage allowed).
			if(HotTemp>=RADIATOR_EXTREME_TEMP && !flagExtremeTemp) {flagExtremeTemp = HIGH;}
			if(flagExtremeTemp){
				maxVoltage = maxVoltage + (HotTemp>=RADIATOR_EXTREME_TEMP ? -0.1 : 0.1); // change voltage slowly to avoid spikes
				if(maxVoltage>maxVoltage_backup){maxVoltage=maxVoltage_backup;}
				if(maxVoltage<minVoltage){maxVoltage=minVoltage;}
				if(HotTemp<RADIATOR_EXTREME_TEMP && maxVoltage==maxVoltage_backup){flagExtremeTemp = LOW;}
			}
			bool coolingEnabled = !stopCooler &&
				(boxMode != 1 || boxSubMode == 2) &&
				boxMode != 3;
			bool coolingDemand = coolingEnabled && boxTemp >= dynamicTemp;
			setVoltage = coolingDemand ? maxVoltage : 0.0;
			static int lastLoggedMode = -1;
			static int lastLoggedSubMode = -1;
			static bool lastLoggedStopState = false;
			static bool lastLoggedDemand = false;
			if (boxMode != lastLoggedMode || boxSubMode != lastLoggedSubMode ||
				stopCooler != lastLoggedStopState || coolingDemand != lastLoggedDemand) {
				Debug.addLine(String(F("Cooling decision: mode=")) + boxMode +
					F(" submode=") + boxSubMode +
					F(" stopped=") + (stopCooler ? F("yes") : F("no")) +
					F(" temp=") + String(boxTemp, 1) +
					F(" target=") + String(dynamicTemp, 1) +
					F(" demand=") + (coolingDemand ? F("ON") : F("OFF")) +
					F(" requested=") + String(setVoltage, 1) + F("V"));
				lastLoggedMode = boxMode;
				lastLoggedSubMode = boxSubMode;
				lastLoggedStopState = stopCooler;
				lastLoggedDemand = coolingDemand;
			}
			// we need to send new values to Buck Converter
			sendI2Cdata();
		}else{
		  //make sure cooler is off
		  setVoltage=0;
		  if (!heatingOnlyLogged) {
			Debug.addLine(F("Cooling disabled: heating-only mode"));
			heatingOnlyLogged = true;
		  }
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
