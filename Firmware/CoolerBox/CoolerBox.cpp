#include "CoolerBox.h"
#include <lwip/err.h>
#include <lwip/napt.h>

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

// Recovery AP defaults to 192.168.4.1; normal operation selects a safe subnet.
char apSSID[33] = "Cooler";
char apPassword[65] = "";
bool apHidden = false;
bool allowApWeb = false;
String iotDeviceMac;
char suggestedIotMac[18] = "";
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

namespace {
const uint16_t NAT_TABLE_SIZE = 8;
const unsigned long NAT_DISCONNECT_TIMEOUT_MS = 10000UL;
const IPAddress AP_SUBNET_MASK(255, 255, 255, 0);
volatile uint8_t pendingApEvent = 0;
volatile uint8_t pendingApMac[6] = {};
volatile bool pendingUpstreamIp = false;
WiFiEventHandler apConnectHandler;
WiFiEventHandler apDisconnectHandler;
WiFiEventHandler staGotIpHandler;
bool natReady = false;
bool natEnabled = false;
bool apStarted = false;
bool hasActiveNatClient = false;
char activeNatMac[18] = "";
IPAddress configuredApIP(0, 0, 0, 0);
IPAddress configuredApDns(0, 0, 0, 0);
unsigned long apNoClientSince = 0;

bool parseMac(const char *text, uint8_t mac[6]) {
	if (text == nullptr || strlen(text) != 17) {
		return false;
	}

	for (uint8_t i = 0; i < 6; ++i) {
		const uint8_t offset = i * 3;
		auto hexValue = [](char value) -> int {
			if (value >= '0' && value <= '9') return value - '0';
			if (value >= 'A' && value <= 'F') return value - 'A' + 10;
			if (value >= 'a' && value <= 'f') return value - 'a' + 10;
			return -1;
		};
		const int high = hexValue(text[offset]);
		const int low = hexValue(text[offset + 1]);
		if (i < 5 && text[offset + 2] != ':' && text[offset + 2] != '-') {
			return false;
		}
		if (high < 0 || low < 0) {
			return false;
		}
		mac[i] = static_cast<uint8_t>((high << 4) | low);
	}
	return true;
}

void formatMac(const uint8_t mac[6], char text[18]) {
	snprintf(text, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool macMatches(const char *text, const uint8_t mac[6]) {
	uint8_t configured[6];
	return parseMac(text, configured) && memcmp(configured, mac, sizeof(configured)) == 0;
}

bool overlapsUpstream(IPAddress candidate, IPAddress upstream, IPAddress upstreamMask) {
	for (uint8_t i = 0; i < 4; ++i) {
		const uint8_t commonMask = AP_SUBNET_MASK[i] & upstreamMask[i];
		if ((candidate[i] & commonMask) != (upstream[i] & commonMask)) {
			return false;
		}
	}
	return true;
}

bool findSafeApAddress(IPAddress &address) {
	const bool upstreamAvailable = WiFi.status() == WL_CONNECTED &&
		WiFi.localIP() != IPAddress(0, 0, 0, 0);
	if (!upstreamAvailable) {
		address = configuredApIP == IPAddress(0, 0, 0, 0)
			? IPAddress(192, 168, 4, 1)
			: configuredApIP;
		return true;
	}

	const IPAddress upstream = WiFi.localIP();
	const IPAddress upstreamMask = WiFi.subnetMask();
	for (uint16_t third = 4; third <= 254; ++third) {
		address = IPAddress(192, 168, static_cast<uint8_t>(third), 1);
		if (!overlapsUpstream(address, upstream, upstreamMask)) {
			return true;
		}
	}
	for (uint8_t second = 16; second <= 31; ++second) {
		for (uint16_t third = 0; third <= 255; ++third) {
			address = IPAddress(172, second, static_cast<uint8_t>(third), 1);
			if (!overlapsUpstream(address, upstream, upstreamMask)) {
				return true;
			}
		}
	}
	for (uint16_t second = 0; second <= 255; ++second) {
		for (uint16_t third = 0; third <= 255; ++third) {
			address = IPAddress(10, static_cast<uint8_t>(second), static_cast<uint8_t>(third), 1);
			if (!overlapsUpstream(address, upstream, upstreamMask)) {
				return true;
			}
		}
	}
	return false;
}

void updateApDns() {
	if (!apStarted || WiFi.status() != WL_CONNECTED) {
		return;
	}
	IPAddress dns = WiFi.dnsIP(0);
	if (dns == IPAddress(0, 0, 0, 0)) {
		dns = WiFi.dnsIP(1);
	}
	if (dns != IPAddress(0, 0, 0, 0) && dns != configuredApDns) {
		WiFi.softAPDhcpServer().setDns(dns);
		configuredApDns = dns;
		Debug.addLine(String(F("AP DHCP DNS set to upstream server ")) + dns.toString());
	}
}

bool ensureNormalAp() {
	IPAddress safeAddress;
	if (!findSafeApAddress(safeAddress)) {
		Debug.addLine(F("No non-overlapping private subnet available; AP not started"));
		if (apStarted) {
			WiFi.softAPdisconnect(false);
			apStarted = false;
			configuredApIP = IPAddress(0, 0, 0, 0);
		}
		return false;
	}
	if (safeAddress != configuredApIP) {
		if (!WiFi.softAPConfig(safeAddress, safeAddress, AP_SUBNET_MASK)) {
			Debug.addLine(F("Failed to configure a non-overlapping AP subnet"));
			if (apStarted) {
				WiFi.softAPdisconnect(false);
				apStarted = false;
				configuredApIP = IPAddress(0, 0, 0, 0);
			}
			return false;
		}
		configuredApIP = safeAddress;
		configuredApDns = IPAddress(0, 0, 0, 0);
		Debug.addLine(String(F("AP subnet configured: ")) + safeAddress.toString());
		if (WiFi.status() == WL_CONNECTED && safeAddress != IPAddress(192, 168, 4, 1)) {
			Debug.addLine(F("Warning: upstream subnet overlaps the default AP subnet; the AP was moved"));
		}
	}
	if (!apStarted) {
		bool started;
		if (apPassword[0] != '\0') {
			started = WiFi.softAP(apSSID, apPassword, 1, apHidden ? 1 : 0, 1);
		} else {
			started = WiFi.softAP(apSSID, nullptr, 1, apHidden ? 1 : 0, 1);
		}
		if (!started) {
			Debug.addLine(F("Failed to start configured AP"));
			return false;
		}
		apStarted = true;
		Serial.print(F("AP IP address: "));
		Serial.println(WiFi.softAPIP());
	}
	updateApDns();
	return true;
}

void onApStationConnected(const WiFiEventSoftAPModeStationConnected &event) {
	for (uint8_t i = 0; i < 6; ++i) {
		pendingApMac[i] = event.mac[i];
	}
	pendingApEvent = 1;
}

void onApStationDisconnected(const WiFiEventSoftAPModeStationDisconnected &event) {
	for (uint8_t i = 0; i < 6; ++i) {
		pendingApMac[i] = event.mac[i];
	}
	pendingApEvent = 2;
}

void onStationGotIp(const WiFiEventStationModeGotIP &) {
	pendingUpstreamIp = true;
}

void setNatEnabled(bool enable) {
	if (!natReady || natEnabled == enable) {
		return;
	}
	err_t result = ip_napt_enable_no(SOFTAP_IF, enable ? 1 : 0);
	if (result != ERR_OK) {
		Debug.addLine(String(F("Failed to ")) + (enable ? F("enable") : F("disable")) +
			F(" AP NAPT, error ") + static_cast<int>(result));
		return;
	}
	natEnabled = enable;
	apNoClientSince = 0;
	Debug.addLine(enable ? F("AP NAT enabled for IoT client") : F("AP NAT disabled"));
}

void processApEvents() {
	const uint8_t eventType = pendingApEvent;
	if (eventType == 0) {
		return;
	}
	pendingApEvent = 0;
	uint8_t mac[6];
	for (uint8_t i = 0; i < 6; ++i) {
		mac[i] = pendingApMac[i];
	}

	char macText[18];
	formatMac(mac, macText);
	if (eventType == 1) {
		if (suggestedIotMac[0] == '\0' && iotDeviceMac.isEmpty()) {
			strncpy(suggestedIotMac, macText, sizeof(suggestedIotMac));
			suggestedIotMac[sizeof(suggestedIotMac) - 1] = '\0';
			Debug.addLine(String(F("First AP client MAC; configure as IoT device: ")) + suggestedIotMac);
		}
		const bool allowed = iotDeviceMac.isEmpty() || macMatches(iotDeviceMac.c_str(), mac);
		if (allowed) {
			strncpy(activeNatMac, macText, sizeof(activeNatMac));
			activeNatMac[sizeof(activeNatMac) - 1] = '\0';
			hasActiveNatClient = true;
			setNatEnabled(true);
		} else {
			Debug.addLine(String(F("NAT not opened for unconfigured AP client ")) + macText);
			setNatEnabled(false);
			hasActiveNatClient = false;
		}
	} else if (eventType == 2 && hasActiveNatClient && strcmp(activeNatMac, macText) == 0) {
		setNatEnabled(false);
		hasActiveNatClient = false;
	}
}

void serviceNat() {
	processApEvents();
	if (natEnabled && !hasActiveNatClient) {
		setNatEnabled(false);
		return;
	}
	if (!natEnabled || !hasActiveNatClient) {
		return;
	}
	if (WiFi.softAPgetStationNum() != 0) {
		apNoClientSince = 0;
		return;
	}
	if (apNoClientSince == 0) {
		apNoClientSince = millis();
	} else if (millis() - apNoClientSince >= NAT_DISCONNECT_TIMEOUT_MS) {
		Debug.addLine(F("AP client disconnect event missing; NAT safety timeout expired"));
		setNatEnabled(false);
		hasActiveNatClient = false;
	}
}

ESP8266WebServer::THandlerFunction guardedHandler(ESP8266WebServer::THandlerFunction handler) {
	return [handler]() {
		if (!isWebClientAllowed()) {
			httpServer.send(403, "text/plain", "Web access from the local AP is disabled");
			return;
		}
		handler();
	};
}
}

bool isWebClientAllowed() {
	if (emergencyMode || allowApWeb) {
		return true;
	}
	if (!apStarted || configuredApIP == IPAddress(0, 0, 0, 0)) {
		return true;
	}
	const IPAddress remote = httpServer.client().remoteIP();
	for (uint8_t i = 0; i < 4; ++i) {
		if ((remote[i] & AP_SUBNET_MASK[i]) != (configuredApIP[i] & AP_SUBNET_MASK[i])) {
			return true;
		}
	}
	return false;
}

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
	WiFi.persistent(false);
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
	if (emergencyMode) {
		Serial.println("Double reset detected! Starting unsecured AP 'Cooler'...");
		Debug.addLine(F("Double reset detected: started unsecured AP 'Cooler'"));

		WiFi.mode(WIFI_AP);
		WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), AP_SUBNET_MASK);
		WiFi.softAP("Cooler", nullptr, 1, 0, 4);
		apStarted = true;
		configuredApIP = WiFi.softAPIP();

		IPAddress myIP = WiFi.softAPIP();
		Serial.print("AP IP address: ");
		Serial.println(myIP);

		primeDS18B20();
	} else {
		Serial.println("Connecting to external Wi-Fi...");
		WiFi.mode(WIFI_AP_STA);
		WiFi.setOutputPower(20.5f); // Always use maximum transmit power (+20.5 dBm)

		WiFi.setAutoConnect(false);
		WiFi.setAutoReconnect(true);
		staGotIpHandler = WiFi.onStationModeGotIP(onStationGotIp);
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

		apConnectHandler = WiFi.onSoftAPModeStationConnected(onApStationConnected);
		apDisconnectHandler = WiFi.onSoftAPModeStationDisconnected(onApStationDisconnected);
		if (!ensureNormalAp()) {
			Debug.addLine(F("Normal AP unavailable until a safe subnet can be selected"));
		}
		err_t naptResult = ip_napt_init(NAT_TABLE_SIZE, 0);
		if (naptResult == ERR_OK) {
			natReady = true;
			Debug.addLine(F("NAPT initialized with an 8-entry table (disabled)"));
		} else {
			Debug.addLine(String(F("NAPT initialization failed, error ")) + static_cast<int>(naptResult));
		}
	}


	httpServer.on("/", guardedHandler(handleRoot));
	httpServer.on("/main.js", guardedHandler(handleScript));
  //httpServer.serveStatic("main.js", LittleFS, "/main.js");
	httpServer.on("/setTemp", guardedHandler(handleSetTemp));
	httpServer.on("/settings", guardedHandler(handleSettings));
	httpServer.on("/settings_store", guardedHandler(handleSettingsStore));
	httpServer.on("/pids", guardedHandler(handlePIDs));
	httpServer.on("/pids_store", guardedHandler(handlePIDsStore));
	httpServer.on("/stop", guardedHandler(handleStop));
	httpServer.on("/start", guardedHandler(handleStart));
	httpServer.on("/network", guardedHandler(handleNetwork));
	httpServer.on("/debug", guardedHandler(handleDebug));
	httpServer.on("/webpage.css", guardedHandler(handleCSS));
	//httpServer.serveStatic("webpage.css", LittleFS, "/webpage.css");
	httpServer.on("/network_store", guardedHandler(handleNetworkStore));
	httpServer.on("/status", guardedHandler(handleStatus));
	httpServer.on("/ajax", guardedHandler(handleStatus));
	httpServer.on("/api/settings", HTTP_GET, guardedHandler(handleApiSettings));
	httpServer.on("/api/pids", HTTP_GET, guardedHandler(handleApiPIDs));
	httpServer.on("/api/network", HTTP_GET, guardedHandler(handleApiNetwork));
	httpServer.on("/api/debug", HTTP_GET, guardedHandler(handleApiDebug));
	httpServer.on("/autotune", HTTP_GET, guardedHandler(handleAutotune));
	httpServer.on("/autotune", HTTP_POST, guardedHandler(handleAutotune));
	httpServer.on("/api/autotune", HTTP_GET, guardedHandler(handleApiAutotune));
	httpServer.on("/api/autotune", HTTP_POST, guardedHandler(handleApiAutotune));
  
	handleFSWebServer();

//	httpServer.onNotFound ( handleNotFound );
//	httpServer.on ( "/inline", []() {
//    httpServer.send ( 200, "text/plain", "this works as well" );
//  } );
	
	httpServer.on("/update", HTTP_GET, guardedHandler(handleUpdate));
	httpServer.on("/update", HTTP_POST, guardedHandler(handleUpdateResult), guardedHandler(handleUpdateUpload));
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

	serviceNat();
	if (pendingUpstreamIp) {
		pendingUpstreamIp = false;
		if (!emergencyMode) {
			ensureNormalAp();
		}
	}

	// Monitor Wi-Fi connection state and reconnect if dropped
	if (!emergencyMode && (millis() - millisWIFIcheckInterval >= 3000UL)) {
		millisWIFIcheckInterval = millis();
		bool isConnected = (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0));
		if (!emergencyMode) {
			ensureNormalAp();
		}
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
