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
unsigned long lastWifiDropMillis = 0;
bool wifiDropOccurred = false;
bool wifiWasConnected = false;

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
	Debug.addLine(String(F("Boot reset: ")) + ESP.getResetInfo());

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
		WiFi.setOutputPower(20.5f); // Always use maximum transmit power (+20.5 dBm)

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
			wifiWasConnected = true;
			Serial.print(F("Internet IP address: "));
			Serial.println(WiFi.localIP());
			Debug.addLine(String(F("Internet connected: ")) + WiFi.localIP().toString());
		} else {
			wifiWasConnected = false;
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
    millisWIFIcheckInterval=millis();

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
		digitalWrite(HOT_FAN, LOW); // Fan is always OFF during heating autotune

		float safePeltierThreshold = max(2.5f, minVoltage + 0.5f);
		bool peltierSafe = !I2C_PRESENT || (measuredVoltage < safePeltierThreshold);
		processHeating(boxTemp, dynamicTemp, peltierSafe);
	} else {
		/*
		 * Tendency-Based Dual Mode Switchover (5-Minute Window)
		 * ======================================================
		 * Rationale:
		 * Rather than using hard, immediate temperature thresholds (e.g., ±0.5°C) that
		 * trigger jarring mode flips and fight thermal inertia:
		 *
		 * 1. Heating -> Cooling:
		 *    When heating reaches or overshoots target, the heating PID automatically
		 *    drops PWM to 0%. Residual thermal mass in the heater wire can cause a minor
		 *    overshoot. If ambient is cooler than the box, heat naturally dissipates.
		 *    The system observes the temperature trend for a 5-minute window (MODE_SWITCH_WINDOW_SEC).
		 *    If the temperature is actively dropping from its peak towards target, the
		 *    window is renewed so passive cooling can do its work without active Peltier interference.
		 *    Only if the heater has stayed at 0% for 5 minutes and the temperature is STILL
		 *    above target and failing to cool down (flat or continuing to rise) do we conclude
		 *    that ambient or fermentation heat is driving the temperature up, and switch to Cooling mode.
		 *
		 * 2. Cooling -> Heating (Two-Stage Handover):
		 *    When cooling demand satisfies the setpoint (temp <= dynamicTemp - DEFAULT_HYSTERESIS),
		 *    active cooling demand turns off. If the temperature continues to fall below target:
		 *    - Stage 1: Cut the Peltier voltage from minVoltage (typically 2.0V) all the way to 0.0V.
		 *      This completely stops residual cooling current.
		 *    - Stage 2: Observe the temperature trend at 0.0V for 5 minutes (MODE_SWITCH_WINDOW_SEC).
		 *      If the temperature starts recovering (rising from its trough back toward target),
		 *      the window is renewed to let passive ambient warmth restore equilibrium.
		 *      Only if the Peltier has been at 0.0V for 5 minutes and the temperature is STILL
		 *      below target and failing to rise (flat or continuing to drop) do we conclude that
		 *      ambient is cold and active heating is necessary, and switch to Heating mode.
		 */
		static unsigned long heatingIdleStartTime = 0;
		static float heatingPeakTemp = -999.0f;

		static unsigned long coolingZeroStartTime = 0;
		static float coolingTroughTemp = 999.0f;

		static float lastTargetTemp = -999.0f;
		if (lastTargetTemp < -900.0f) {
			lastTargetTemp = dynamicTemp;
		}

		// If target temperature changed, immediately decide sub-mode based on current temperature
		if (dynamicTemp != lastTargetTemp) {
			if (boxMode == 1) {
				if (boxTemp < dynamicTemp) {
					boxSubMode = 3; // Immediately switch to Heating
					Debug.addLine(String(F("Target changed to ")) + String(dynamicTemp, 1) +
					              F("°C: immediate switch to Heating (boxTemp=") + String(boxTemp, 2) + F("°C)"));
				} else if (boxTemp > dynamicTemp) {
					boxSubMode = 2; // Immediately switch to Cooling
					Debug.addLine(String(F("Target changed to ")) + String(dynamicTemp, 1) +
					              F("°C: immediate switch to Cooling (boxTemp=") + String(boxTemp, 2) + F("°C)"));
				}
			}
			heatingIdleStartTime = 0;
			heatingPeakTemp = -999.0f;
			coolingZeroStartTime = 0;
			coolingTroughTemp = 999.0f;
			coolingZeroVoltage = false;
			lastTargetTemp = dynamicTemp;
		}

		bool isStopped = isCoolerStopped();

		if (boxMode == 1 && !isStopped) {
			if (boxSubMode == 2) { // Currently in Cooling sub-mode
				heatingIdleStartTime = 0;
				heatingPeakTemp = -999.0f;

				// In cooling mode, demand turns off at targetTemp - DEFAULT_HYSTERESIS (e.g. target - 0.05°C)
				if (boxTemp >= (dynamicTemp - DEFAULT_HYSTERESIS)) {
					// Temperature is at or above the cooling cutoff; normal cooling or idle at minVoltage
					coolingZeroVoltage = false;
					coolingZeroStartTime = 0;
					coolingTroughTemp = 999.0f;
				} else {
					// Temperature is continuing to drop below target:
					// Stage 1: Cut Peltier voltage to 0.0V to eliminate idle bias
					coolingZeroVoltage = true;

					if (coolingZeroStartTime == 0) {
						coolingZeroStartTime = millis();
						coolingTroughTemp = boxTemp;
						Debug.addLine(String(F("Cooling idle: temp below target (")) + String(boxTemp, 2) +
						              F("°C). Cutting Peltier to 0.0V. Observing tendency (5m)..."));
					} else {
						// Track the minimum temperature reached during this undershoot
						if (boxTemp < coolingTroughTemp) {
							coolingTroughTemp = boxTemp;
						}

						// Stage 2: Check tendency over time
						// If temperature has visibly started recovering (+0.05°C above trough), renew window
						if (boxTemp >= (coolingTroughTemp + 0.05f)) {
							coolingTroughTemp = boxTemp;
							coolingZeroStartTime = millis();
						} else if (millis() - coolingZeroStartTime >= (MODE_SWITCH_WINDOW_SEC * 1000UL)) {
							// 5 minutes at 0.0V expired and temperature did not recover
							if (boxTemp < dynamicTemp) {
								boxSubMode = 3; // Switch to Heating mode
								coolingZeroVoltage = false;
								coolingZeroStartTime = 0;
								coolingTroughTemp = 999.0f;
								Debug.addLine(String(F("Switching Cooling -> Heating: 5m at 0.0V, temp still dropping/below target (")) +
								              String(boxTemp, 2) + F("°C)"));
							}
						}
					}
				}
			} else { // Currently in Heating sub-mode (boxSubMode == 3)
				coolingZeroVoltage = false;
				coolingZeroStartTime = 0;
				coolingTroughTemp = 999.0f;

				// Heater is idle when PID output is 0 and temperature is above target
				bool heatingIdle = (setHotPWM <= 0.0f) && (boxTemp > dynamicTemp);

				if (!heatingIdle) {
					// Heater is active or temperature is below target
					heatingIdleStartTime = 0;
					heatingPeakTemp = -999.0f;
				} else {
					if (heatingIdleStartTime == 0) {
						heatingIdleStartTime = millis();
						heatingPeakTemp = boxTemp;
						Debug.addLine(String(F("Heating idle: heater at 0%, temp above target (")) + String(boxTemp, 2) +
						              F("°C). Observing tendency (5m)..."));
					} else {
						// Track the maximum temperature reached during this overshoot
						if (boxTemp > heatingPeakTemp) {
							heatingPeakTemp = boxTemp;
						}

						// Check tendency over time
						// If temperature has visibly started cooling down (-0.05°C below peak), renew window
						if (boxTemp <= (heatingPeakTemp - 0.05f)) {
							heatingPeakTemp = boxTemp;
							heatingIdleStartTime = millis();
						} else if (millis() - heatingIdleStartTime >= (MODE_SWITCH_WINDOW_SEC * 1000UL)) {
							// 5 minutes at 0% PWM expired and temperature did not drop
							if (boxTemp > dynamicTemp) {
								boxSubMode = 2; // Switch to Cooling mode
								heatingIdleStartTime = 0;
								heatingPeakTemp = -999.0f;
								Debug.addLine(String(F("Switching Heating -> Cooling: 5m at 0% PWM, temp still rising/above target (")) +
								              String(boxTemp, 2) + F("°C)"));
							}
						}
					}
				}
			}
		} else {
			// Not in Dual Mode or cooler stopped
			heatingIdleStartTime = 0;
			heatingPeakTemp = -999.0f;
			coolingZeroStartTime = 0;
			coolingTroughTemp = 999.0f;
			if (boxMode == 2 && !isStopped) {
				// In cooling-only mode, also cut Peltier to 0.0V if below cutoff to avoid overcooling
				coolingZeroVoltage = (boxTemp < (dynamicTemp - DEFAULT_HYSTERESIS));
			} else {
				coolingZeroVoltage = false;
			}
		}

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

	// Send data to IoT
	if (!emergencyMode && (millis() - millisSendDataInterval > static_cast<unsigned long>(sendInterval) * 1000UL)) {
		millisSendDataInterval = millis();
		sendTelemetry();
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
	if (!emergencyMode && (millis() - millisWIFIcheckInterval >= 3000UL)) {
		millisWIFIcheckInterval = millis();
		bool isConnected = (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0));
		if (!isConnected) {
			if (wifiWasConnected) {
				wifiWasConnected = false;
				wifiDropOccurred = true;
				lastWifiDropMillis = millis();
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

			// Ensure TX power remains at maximum (+20.5 dBm)
			static bool txPowerInitialized = false;
			if (!txPowerInitialized) {
				txPowerInitialized = true;
				WiFi.setOutputPower(20.5f);
				Debug.addLine(String(F("Wi-Fi TX power set to maximum (20.5 dBm), RSSI: ")) + WiFi.RSSI() + F(" dBm"));
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
