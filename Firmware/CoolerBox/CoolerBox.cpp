#include "CoolerBox.h"

DebugHTML Debug;

bool flagExtremeTemp = LOW;
float maxVoltage_backup; // backup original value

float boxTemp = 0.0f;	// current measured temperature inside the box
float HotTemp = 0.0f;	// current measured temperature of hot radiator
float setTemp = 19.0f;	// preset temperature
float dynamicTemp = 19.0f; // The option to adjust setTemp dynamically
#if ENABLE_ISPINDEL_DYNAMIC_TEMP
float iSpindelTemperature = 0.0f;
float dynamicTempDiffCoeff = 2.0f;
#endif
float setVoltage = 0.0f;
float changeVoltageSpeed = 0.4f; 
float minVoltage = 2.0f; 
float maxVoltage = 12.0f; 
float measuredVoltage = 0.0f; 
float measuredCurrent = 0.0f; 
bool stopCooler = LOW;
float setHotPWM = 0.0f; // initially off
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

DoubleResetDetector drd(10, 64);

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

float hotPID_kP=DEFAULT_PID_KP;
float hotPID_kI=DEFAULT_PID_KI;
float hotPID_kD=DEFAULT_PID_KD;

static double pidInput = 0.0;
static double pidOutput = 0.0;
static double pidSetpoint = 0.0;

// Create hot PID with default values
PID hotPID(&pidInput, &pidOutput, &pidSetpoint, hotPID_kP, hotPID_kI, hotPID_kD, DIRECT); // no reverse as it works for heating

static bool currentCoolingDemand = false;

static bool evaluateCoolingDemand() {
	bool isStopped = stopCooler;
	bool isHeating = (boxMode == 3) || (boxMode == 1 && boxSubMode == 3);

	if (isStopped || isHeating) {
		currentCoolingDemand = false;
	} else {
		if (!currentCoolingDemand) {
			if (boxTemp >= (dynamicTemp + DEFAULT_HYSTERESIS)) {
				currentCoolingDemand = true;
			}
		} else {
			if (boxTemp <= (dynamicTemp - DEFAULT_HYSTERESIS)) {
				currentCoolingDemand = false;
			}
		}
	}
	return currentCoolingDemand;
}

void updateCoolingOutput(bool forceSend, bool sendLimits) {
	bool isStopped = stopCooler;
	bool isHeating = (boxMode == 3) || (boxMode == 1 && boxSubMode == 3);

	if (isStopped || isHeating) {
		setVoltage = 0.0f;
	} else {
		bool demand = evaluateCoolingDemand();
		setVoltage = demand ? maxVoltage : minVoltage;
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
	httpServer.on("/start", handleStart);
	httpServer.on("/network", handleNetwork);
	httpServer.on("/debug", handleDebug);
	httpServer.on("/webpage.css", handleCSS);
	//httpServer.serveStatic("webpage.css", LittleFS, "/webpage.css");
	httpServer.on("/network_store", handleNetworkStore);
	httpServer.on("/status", handleStatus);
	httpServer.on("/ajax", handleStatus);
	httpServer.on("/api/settings", HTTP_GET, handleApiSettings);
	httpServer.on("/api/pids", HTTP_GET, handleApiPIDs);
	httpServer.on("/api/network", HTTP_GET, handleApiNetwork);
	httpServer.on("/api/debug", HTTP_GET, handleApiDebug);
  
	handleFSWebServer();

//	httpServer.onNotFound ( handleNotFound );
//	httpServer.on ( "/inline", []() {
//    httpServer.send ( 200, "text/plain", "this works as well" );
//  } );
	
	httpServer.on("/update", HTTP_GET, handleUpdate);
	httpServer.on("/update", HTTP_POST, handleUpdateResult, handleUpdateUpload);
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
	if (iSpindelTemperature > 0.0f) {
		dynamicTemp = setTemp + ((setTemp - iSpindelTemperature) * dynamicTempDiffCoeff);
	}
#endif
	// should we swithc boxSubMode?
	if(boxMode==1){
		if(boxSubMode==2){ //Cooling
			if(boxTemp<(dynamicTemp-0.5f)){
				boxSubMode=3;	// Switch to heating mode
			}
		}else{
			if(boxTemp>(dynamicTemp+0.5f)){
				boxSubMode=2;	// Switch to cooling mode
				setHotPWM = 0.0f;
				pidOutput = 0.0;
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
	bool demand = evaluateCoolingDemand();

	if (isStopped || isHeating) {
		setVoltage = 0.0f;
	} else {
		float effectiveMax = flagExtremeTemp ? thermalVoltageCap : maxVoltage;
		setVoltage = demand ? effectiveMax : minVoltage;
	}
	sendI2Cdata();

	if (!isStopped && ((boxMode == 3 && measuredVoltage < 3.0f) || (boxMode == 1 && boxSubMode == 3))) {
		pidInput = boxTemp;
		pidSetpoint = dynamicTemp;
		hotPID.Compute();
		setHotPWM = (float)pidOutput;
	} else {
		setHotPWM = 0.0f;
		pidOutput = 0.0;
	}
	analogWrite(HEAT_WIRE, (int)round(setHotPWM));

	static int lastLoggedMode = -1;
	static int lastLoggedSubMode = -1;
	static bool lastLoggedStopState = false;
	static bool lastLoggedDemand = false;
	static float lastLoggedSetVoltage = -1.0f;
	static bool lastLoggedHeating = false;

	bool stateChanged = (boxMode != lastLoggedMode || boxSubMode != lastLoggedSubMode ||
	                     stopCooler != lastLoggedStopState || isHeating != lastLoggedHeating);
	bool coolingChanged = (!isHeating && !isStopped && (demand != lastLoggedDemand || setVoltage != lastLoggedSetVoltage));

	if (stateChanged || coolingChanged) {
		String msg;
		if (isStopped) {
			msg = F("Cooler stopped: temp=");
			msg += String(boxTemp, 2);
			msg += F(" target=");
			msg += String(dynamicTemp, 2);
		} else if (isHeating) {
			msg = F("Heating is on: temp=");
			msg += String(boxTemp, 2);
			msg += F(" target=");
			msg += String(dynamicTemp, 2);
		} else {
			msg = F("Cooling: temp=");
			msg += String(boxTemp, 2);
			msg += F(" target=");
			msg += String(dynamicTemp, 2);
			msg += F(" requested=");
			msg += String(setVoltage, 1);
			msg += F("V");
		}
		Debug.addLine(msg);

		lastLoggedMode = boxMode;
		lastLoggedSubMode = boxSubMode;
		lastLoggedStopState = stopCooler;
		lastLoggedDemand = demand;
		lastLoggedSetVoltage = setVoltage;
		lastLoggedHeating = isHeating;
	}

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
		if(HotTemp>=32.0f && digitalRead(HOT_FAN)==LOW){
			digitalWrite(HOT_FAN, HIGH);
		}else if(HotTemp<=24.0f && digitalRead(HOT_FAN)==HIGH){
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
	if (Update.isRunning()) {
		drd.stop();
		httpServer.handleClient();
		delay(1);
		return;
	}

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
	
	if(measuredVoltage<2.0f){
		digitalWrite(LED_WARN, LOW);
	}else{
		digitalWrite(LED_WARN, HIGH);
	}

}
