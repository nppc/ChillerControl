#include "CoolerBox.h"

DebugHTML Debug;

float boxTemp = 0.0f;	// current measured temperature inside the box
float HotTemp = 0.0f;	// current measured temperature of hot radiator
float setTemp = 19.0f;	// preset temperature
float dynamicTemp = 19.0f; // The option to adjust setTemp dynamically
#if ENABLE_ISPINDEL_DYNAMIC_TEMP
float iSpindelTemperature = 0.0f;
float dynamicTempDiffCoeff = 2.0f;
#endif
float measuredVoltage = 0.0f; 
float measuredCurrent = 0.0f; 

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

void setup() {
	pinMode(HOT_FAN, OUTPUT);
	digitalWrite(HOT_FAN, HIGH);
	pinMode(LED_WARN, OUTPUT);
	digitalWrite(LED_WARN, LOW);

	initHeating();
	initCooling();

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

	restoreSettings();	// restore PID, boxMode, and voltage limits first
	maxVoltage_backup = maxVoltage; // update backup

	initI2C();	// Detect I2C slave, sync initial voltage limits, and read initial telemetry
  	
	Serial.println();
	if (emergencyMode) {
		Serial.println("Double reset detected! Starting unsecured AP 'Cooler'...");
		Debug.addLine(F("Double reset detected: started unsecured AP 'Cooler'"));

		WiFi.persistent(false);
		WiFi.mode(WIFI_AP);
		WiFi.setSleepMode(WIFI_NONE_SLEEP);
		WiFi.softAP("Cooler");

		IPAddress myIP = WiFi.softAPIP();
		Serial.print("AP IP address: ");
		Serial.println(myIP);

		primeDS18B20();
	} else {
		Serial.println("Configuring access point...");

		WiFi.persistent(false); // don't write wifi settings to flash
		WiFi.mode(WIFI_AP_STA);
		WiFi.setSleepMode(WIFI_NONE_SLEEP); // Disable Wi-Fi modem sleep to improve weak signal (-85 dBm) stability

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
	httpServer.on("/autotune", HTTP_GET, handleAutotune);
	httpServer.on("/autotune", HTTP_POST, handleAutotune);
	httpServer.on("/api/autotune", HTTP_GET, handleApiAutotune);
	httpServer.on("/api/autotune", HTTP_POST, handleApiAutotune);
  
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

  drd.notifySetupComplete();
}

void processControl() {
#if ENABLE_ISPINDEL_DYNAMIC_TEMP
	if (iSpindelTemperature > 0.0f) {
		dynamicTemp = setTemp + ((setTemp - iSpindelTemperature) * dynamicTempDiffCoeff);
	}
#endif

	// Autotune mode override
	if (isHeatingAutoTuning()) {
		processCooling(boxTemp, HotTemp, dynamicTemp, false); // cooling disabled
		processHeating(boxTemp, dynamicTemp, true);
		digitalWrite(HOT_FAN, LOW); // Fan is always OFF during heating autotune
		return;
	}

	// Automatic switchover between cooling and heating in Dual Mode (boxMode == 1)
	if (boxMode == 1) {
		if (boxSubMode == 2) { // Currently Cooling
			if (boxTemp < (dynamicTemp - 0.5f)) {
				boxSubMode = 3;	// Switch to heating mode
			}
		} else { // Currently Heating
			if (boxTemp > (dynamicTemp + 0.5f)) {
				boxSubMode = 2;	// Switch to cooling mode
			}
		}
	}

	bool isStopped = isCoolerStopped();
	bool isHeating = (boxMode == 3) || (boxMode == 1 && boxSubMode == 3);
	bool isCooling = !isHeating && !isStopped;

	// Reset PID memory whenever transitioning into Heating mode to eliminate stale memory and prevent windups
	static bool wasHeating = false;
	static bool wasStopped = true;
	static int lastSubMode = -1;

	bool enteredHeating = (!wasHeating && isHeating && !isStopped) ||
	                      (wasStopped && !isStopped && isHeating) ||
	                      (boxSubMode == 3 && lastSubMode != 3 && !isStopped);
	if (enteredHeating) {
		resetHeatingPID();
		Debug.addLine(F("Switched to Heating: PID memory fully reset"));
	}
	wasHeating = isHeating;
	wasStopped = isStopped;
	lastSubMode = boxSubMode;

	// Execute separated cooling logic
	processCooling(boxTemp, HotTemp, dynamicTemp, isCooling);

	// Execute separated heating logic
	// In heating mode or dual heating submode, wait for buck converter voltage to be safely below minVoltage + margin
	float safePeltierThreshold = max(2.5f, minVoltage + 0.5f);
	bool heatingAllowed = !isStopped && (measuredVoltage < safePeltierThreshold) && ((boxMode == 3) || (boxMode == 1 && boxSubMode == 3));
	processHeating(boxTemp, dynamicTemp, isHeating && heatingAllowed);

	// Log control state changes
	static int lastLoggedMode = -1;
	static int lastLoggedSubMode = -1;
	static bool lastLoggedStopState = false;
	static bool lastLoggedDemand = false;
	static float lastLoggedSetVoltage = -1.0f;
	static bool lastLoggedHeating = false;

	bool demand = isCoolingDemanded();
	bool stateChanged = (boxMode != lastLoggedMode || boxSubMode != lastLoggedSubMode ||
	                     isStopped != lastLoggedStopState || isHeating != lastLoggedHeating);
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
		lastLoggedStopState = isStopped;
		lastLoggedDemand = demand;
		lastLoggedSetVoltage = setVoltage;
		lastLoggedHeating = isHeating;
	}

	// Send data to IoT
	if (!emergencyMode && (millis() - millisSendDataInterval > static_cast<unsigned long>(sendInterval) * 1000UL)) {
		millisSendDataInterval = millis();
		sendTelemetry();
	}

	// Control Fan
	if (isHeating) {
		digitalWrite(HOT_FAN, LOW); // Fan is always OFF when heating
	} else if (HotSensorId == ColdSensorId || fanDynamic_checked == 0) {
		digitalWrite(HOT_FAN, HIGH);
	} else {
		if (HotTemp >= 32.0f && digitalRead(HOT_FAN) == LOW) {
			digitalWrite(HOT_FAN, HIGH);
		} else if (HotTemp <= 24.0f && digitalRead(HOT_FAN) == HIGH) {
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

	// Monitor Wi-Fi connection state and reconnect if dropped
	static bool wifiWasConnected = false;
	if (!emergencyMode && (millis() - millisWIFIcheckInterval > (30 * 1000UL))) {
		millisWIFIcheckInterval = millis();
		bool isConnected = (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0));
		if (!isConnected) {
			if (wifiWasConnected) {
				wifiWasConnected = false;
				Debug.addLine(F("Wi-Fi connection lost"));
			}
			static unsigned long lastReconnectAttempt = 0;
			if (millis() - lastReconnectAttempt > (60 * 1000UL)) {
				lastReconnectAttempt = millis();
				Debug.addLine(F("Wi-Fi disconnected: attempting reconnect"));
				WiFi.reconnect();
			}
		} else {
			if (!wifiWasConnected) {
				wifiWasConnected = true;
				Debug.addLine(String(F("Wi-Fi connected: ")) + WiFi.localIP().toString());
			}
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
