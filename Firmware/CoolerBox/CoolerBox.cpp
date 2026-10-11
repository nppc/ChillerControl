#include "CoolerBox.h"

DebugHTML Debug;

bool flagExtremeTemp = LOW;
float maxVoltage_backup; // backup original value

double boxTemp = 0.0;	// current measured temperature inside the box
double HotTemp = 0.0;	// current measured temperature of hot radiator
double setTemp = 19.0;	// preset temperature
double dynamicTemp = 19.0; // The option to adjust setTemp dynamically
#if ENABLE_ISPINDEL_DYNAMIC_TEMP
double iSpindelTemperature = 0.0;
float dynamicTempDiffCoeff = 2.0;
#endif
double setVoltage = 0.0;
float changeVoltageSpeed = 0.4f; 
float minVoltage = 2.0f; 
float maxVoltage = 12.0f; 
float measuredVoltage = 0.0f; 
float measuredCurrent = 0.0f; 
bool stopCooler = LOW;
double setHotPWM=0.0; // initially off
int maxHotPWM = MAXIMUM_PWM_HOT_WIRE; 

unsigned long millisPIDinterval;
unsigned long millisSendDataInterval;
unsigned long millisReadI2CDataInterval;
unsigned long millisWIFIcheckInterval;

int ColdSensorId = 0;
int HotSensorId = 0; 


// Default IP in AP mode is 192.168.4.1
char apSSID[33] = "Cooler";
char apPassword[65] = "";
bool emergencyMode = false;
const char *ssid = apSSID;
const char *password = apPassword;

DoubleResetDetector drd(10, 0);

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

float hotPID_kP=DEFAULT_PID_KP;
float hotPID_kI=DEFAULT_PID_KI;
float hotPID_kD=DEFAULT_PID_KD;

// Create hot PID with default values
PID hotPID(&(boxTemp), &(setHotPWM), &(dynamicTemp), hotPID_kP, hotPID_kI, hotPID_kD, DIRECT); // no reverse as it works for heating

void updateCoolingOutput(bool forceSend, bool sendLimits) {
	bool isStopped = stopCooler;
	bool isHeating = (boxMode == 3) || (boxMode == 1 && boxSubMode == 3);

	if (isStopped || isHeating) {
		setVoltage = 0.0;
	} else {
		bool coolingDemand = (boxTemp >= dynamicTemp);
		setVoltage = coolingDemand ? maxVoltage : minVoltage;
	}
	sendI2Cdata(forceSend, sendLimits);
}

void setCoolerStopped(bool stopped) {
	if (stopCooler == stopped) {
		return;
	}
	stopCooler = stopped;
	updateCoolingOutput();
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

	drd.detectDoubleReset();
	emergencyMode = drd.isDoubleResetDetected();
  
	boxMode=1;	// set some mode in case this setting not yet present
	
	millisWIFIcheckInterval = millis();

	initEEPROM();
  
	if (!LittleFS.begin()) {
		Debug.addLine(F("LittleFS mount failed"));
		Serial.println(F("LittleFS mount failed; filesystem settings and files are unavailable."));
	}

	INIT_DS18B20();

	Serial.begin(115200);
	initI2C();
	
	restoreSettings();	// restore PID and internet settings

	maxVoltage_backup = maxVoltage; // update backup
	updateCoolingOutput(true, true); // send initial limits and voltage to I2C buck converter
  	
	Serial.println();
	if (emergencyMode) {
		Serial.println("Double reset detected! Starting unsecured AP 'Cooler'...");
		Debug.addLine(F("Double reset detected: started unsecured AP 'Cooler'"));

		WiFi.persistent(false);
		WiFi.mode(WIFI_AP);
		WiFi.softAP("Cooler");

		IPAddress myIP = WiFi.softAPIP();
		Serial.print("AP IP address: ");
		Serial.println(myIP);

		primeDS18B20();
	} else {
		Serial.println("Configuring access point...");

		WiFi.persistent(false); // don't write wifi settings to flash
		WiFi.mode(WIFI_AP_STA);

		if (apPassword[0] != '\0') {
			Debug.addLine(String(F("Normal boot: starting secured AP '")) + apSSID + F("'"));
			WiFi.softAP(apSSID, apPassword);
		} else {
			Debug.addLine(String(F("Normal boot: starting unsecured AP '")) + apSSID + F("'"));
			WiFi.softAP(apSSID);
		}

		IPAddress myIP = WiFi.softAPIP();
		Serial.print("AP IP address: ");
		Serial.println(myIP);

		Serial.println();
		Serial.println("Connecting to Internet...");

		WiFi.setAutoConnect(false);
		WiFi.setAutoReconnect(true);
		WiFi.begin(IntSSID, IntPASS);

		primeDS18B20();

		int i = 20;
		while (WiFi.status() != WL_CONNECTED && i > 0) {
			delay(500);
			Serial.print(i);
			i--;
		}

		if (WiFi.status() == WL_CONNECTED) {
			Serial.print(F("Internet IP address: "));
			Serial.println(WiFi.localIP());
			Debug.addLine(String(F("Internet connected: ")) + WiFi.localIP().toString());
		} else {
			Serial.print(F("Internet connection failed."));
			Serial.print(F(" Error is: "));
			Serial.println(WiFi.status());
			Debug.addLine(String(F("Internet connection failed, status: ")) + WiFi.status());
		}
	}


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
    resetTempConversionState();
	
  hotPID.SetOutputLimits(0, maxHotPWM);  // same limits for hot wire PWM
  hotPID.SetMode(AUTOMATIC);
  hotPID.SetTunings(hotPID_kP, hotPID_kI, hotPID_kD);

  drd.notifySetupComplete();
}

void processControl() {
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
	
	static float thermalVoltageCap = 12.0f;
	if (boxMode != 3) {
		// Cooler routines
		// If radiator is very hot, reduce operating voltage without modifying configuration limits.
		if (HotTemp >= RADIATOR_EXTREME_TEMP && !flagExtremeTemp) {
			flagExtremeTemp = HIGH;
			thermalVoltageCap = maxVoltage;
		}
		if (flagExtremeTemp) {
			thermalVoltageCap += (HotTemp >= RADIATOR_EXTREME_TEMP ? -0.1f : 0.1f); // change voltage slowly to avoid spikes
			if (thermalVoltageCap > maxVoltage) { thermalVoltageCap = maxVoltage; }
			if (thermalVoltageCap < minVoltage) { thermalVoltageCap = minVoltage; }
			if (HotTemp < RADIATOR_EXTREME_TEMP && thermalVoltageCap >= maxVoltage) {
				flagExtremeTemp = LOW;
				thermalVoltageCap = maxVoltage;
			}
		}
	} else {
		flagExtremeTemp = LOW;
		thermalVoltageCap = maxVoltage;
	}

	bool isStopped = stopCooler;
	bool isHeating = (boxMode == 3) || (boxMode == 1 && boxSubMode == 3);
	bool coolingDemand = (!isStopped && !isHeating) && (boxTemp >= dynamicTemp);

	if (isStopped || isHeating) {
		setVoltage = 0.0;
	} else {
		float effectiveMax = flagExtremeTemp ? thermalVoltageCap : maxVoltage;
		setVoltage = coolingDemand ? effectiveMax : minVoltage;
	}

	static int lastLoggedMode = -1;
	static int lastLoggedSubMode = -1;
	static bool lastLoggedStopState = false;
	static bool lastLoggedDemand = false;
	static double lastLoggedSetVoltage = -1.0;
	if (boxMode != lastLoggedMode || boxSubMode != lastLoggedSubMode ||
		stopCooler != lastLoggedStopState || coolingDemand != lastLoggedDemand ||
		setVoltage != lastLoggedSetVoltage) {
		Debug.addLine(String(F("Cooling decision: mode=")) + boxMode +
			F(" submode=") + boxSubMode +
			F(" stopped=") + (isStopped ? F("yes") : F("no")) +
			F(" heating=") + (isHeating ? F("yes") : F("no")) +
			F(" temp=") + String(boxTemp, 1) +
			F(" target=") + String(dynamicTemp, 1) +
			F(" demand=") + (coolingDemand ? F("ON") : F("OFF")) +
			F(" requested=") + String(setVoltage, 1) + F("V"));
		lastLoggedMode = boxMode;
		lastLoggedSubMode = boxSubMode;
		lastLoggedStopState = stopCooler;
		lastLoggedDemand = coolingDemand;
		lastLoggedSetVoltage = setVoltage;
	}
	// we need to send new values to Buck Converter
	sendI2Cdata();
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
	if(!emergencyMode && (millis()-millisSendDataInterval>static_cast<unsigned long>(sendInterval) * 1000UL)){
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

void handleTemperatureControl() {
	if (updateDS18B20()) {
		processControl();
	}
}

void loop() {
	drd.loop();

	//check connection every 5 minutes
	if(!emergencyMode && (millis()-millisWIFIcheckInterval>(300 * 1000UL))){
		millisWIFIcheckInterval = millis();
		if(WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0,0,0,0)) {
			WiFi.reconnect();
		}
	}

	httpServer.handleClient();
	delay(1);
	
	handleTemperatureControl();
	
	if(millis()-millisReadI2CDataInterval>(I2C_INTERVAL * 1000UL)){
		millisReadI2CDataInterval = millis();
		readI2Cdata();
	}
	
	if(measuredVoltage<2.0){
		digitalWrite(LED_WARN, LOW);
	}else{
		digitalWrite(LED_WARN, HIGH);
	}

}
