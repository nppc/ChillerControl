#include "CoolerBox.h"

#include <time.h>
extern "C" {
#include <user_interface.h>
}

DebugHTML Debug;
const char DEFAULT_TIMEZONE[] = "EET-2EEST,M3.5.0/3,M10.5.0/4";
char tzSetting[64] = "EET-2EEST,M3.5.0/3,M10.5.0/4";

float boxTemp = 0.0f;	// current measured temperature inside the box
float HotTemp = 0.0f;	// current measured temperature of hot radiator
float setTemp = 19.0f;	// preset temperature
bool useISpindelTemp = true;
float iSpindelTemperature = 0.0f;
float iSpindelGravity = 0.0f;
float iSpindelBattery = 0.0f;
bool hasNewISpindelData = false;
unsigned long lastISpindelMillis = 0;
int iSpindelRssi = 0;
char iSpindelExtraKeys[ISPINDEL_EXTRA_MAX][24] = {};
char iSpindelExtraValues[ISPINDEL_EXTRA_MAX][32] = {};
uint8_t iSpindelExtraCount = 0;
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
int boxMode = 0; // 1-Cooling/Heating, 2-Cooling, 3-Heating
int boxSubMode = 3; // 2-Cooling, 3-Heating

// Define a web server at port 80 for HTTP
ESP8266WebServer httpServer(80);

namespace {
const IPAddress AP_SUBNET_MASK(255, 255, 255, 0);
volatile bool pendingUpstreamIp = false;
WiFiEventHandler staGotIpHandler;
WiFiEventHandler staConnectedHandler;
WiFiEventHandler staDisconnectedHandler;
WiFiEventHandler apStationConnectedHandler;
WiFiEventHandler apStationDisconnectedHandler;
volatile bool sendTelemetryNow = false;
struct WifiLogEvent {
	uint8_t type;
	uint8_t reason;
	uint8_t mac[6];
};
const uint8_t WIFI_LOG_QUEUE_SIZE = 8;
const uint8_t WIFI_LOG_QUEUE_MASK = WIFI_LOG_QUEUE_SIZE - 1;
enum WifiLogEventType : uint8_t {
	STA_CONNECTED_EVENT = 1,
	STA_DISCONNECTED_EVENT,
	AP_STATION_CONNECTED_EVENT,
	AP_STATION_DISCONNECTED_EVENT
};
volatile WifiLogEvent wifiLogQueue[WIFI_LOG_QUEUE_SIZE] = {};
volatile uint8_t wifiLogHead = 0;
volatile uint8_t wifiLogTail = 0;
volatile uint8_t wifiLogDropped = 0;
uint8_t lastBssid[6] = {};
int lastCh = 0;
uint8_t reconnectAttemptCount = 0;
uint32_t reconnectScheduleStartMillis = 0;
uint32_t lastReconnectBeginMillis = 0;
uint32_t reconnectScheduleAttemptCount = 0;
bool reconnectScheduleActive = false;
bool reconnectBeginIssued = false;
int lastStaRssi = 0;
bool apStarted = false;
IPAddress configuredApIP(0, 0, 0, 0);
IPAddress configuredApDns(0, 0, 0, 0);

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
			started = WiFi.softAP(apSSID, apPassword, 1, apHidden ? 1 : 0, 3);
		} else {
			started = WiFi.softAP(apSSID, nullptr, 1, apHidden ? 1 : 0, 3);
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

void onStationGotIp(const WiFiEventStationModeGotIP &) {
	pendingUpstreamIp = true;
}

void enqueueWifiLogEvent(uint8_t type, uint8_t reason, const uint8_t *mac) {
	uint8_t head = wifiLogHead;
	uint8_t next = (head + 1) & WIFI_LOG_QUEUE_MASK;
	if (next == wifiLogTail) {
		if (wifiLogDropped < 255) {
			++wifiLogDropped;
		}
		return;
	}
	wifiLogQueue[head].type = type;
	wifiLogQueue[head].reason = reason;
	for (uint8_t i = 0; i < 6; ++i) {
		wifiLogQueue[head].mac[i] = mac == nullptr ? 0 : mac[i];
	}
	wifiLogHead = next;
}

void onStationConnected(const WiFiEventStationModeConnected &event) {
	(void)event;
	enqueueWifiLogEvent(STA_CONNECTED_EVENT, 0, nullptr);
}

void onStationDisconnected(const WiFiEventStationModeDisconnected &event) {
	enqueueWifiLogEvent(STA_DISCONNECTED_EVENT, static_cast<uint8_t>(event.reason), nullptr);
}

static char lastApStationMac[18] = "";

String getISpindelClientMac() {
	IPAddress remoteIp = httpServer.client().remoteIP();
	struct station_info *stat_info = wifi_softap_get_station_info();
	char macBuf[18] = "";
	while (stat_info != nullptr) {
		if (IPAddress(stat_info->ip.addr) == remoteIp) {
			snprintf(macBuf, sizeof(macBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
			         stat_info->bssid[0], stat_info->bssid[1], stat_info->bssid[2],
			         stat_info->bssid[3], stat_info->bssid[4], stat_info->bssid[5]);
			break;
		}
		stat_info = STAILQ_NEXT(stat_info, next);
	}
	wifi_softap_free_station_info();
	if (macBuf[0] != '\0') {
		return String(macBuf);
	}
	if (lastApStationMac[0] != '\0') {
		return String(lastApStationMac);
	}
	return String();
}

void onApStationConnected(const WiFiEventSoftAPModeStationConnected &event) {
	snprintf(lastApStationMac, sizeof(lastApStationMac), "%02X:%02X:%02X:%02X:%02X:%02X",
	         event.mac[0], event.mac[1], event.mac[2],
	         event.mac[3], event.mac[4], event.mac[5]);
}

void onApStationDisconnected(const WiFiEventSoftAPModeStationDisconnected &event) {
	(void)event;
}

void processWifiEventLogs() {
	while (wifiLogTail != wifiLogHead) {
		uint8_t tail = wifiLogTail;
		uint8_t type = wifiLogQueue[tail].type;
		uint8_t reason = wifiLogQueue[tail].reason;
		wifiLogTail = (tail + 1) & WIFI_LOG_QUEUE_MASK;

		if (type == STA_CONNECTED_EVENT) {
			Debug.addLine(String(F("STA connected: channel=")) + WiFi.channel());
		} else if (type == STA_DISCONNECTED_EVENT) {
			recordLinkDiagnostic(LinkDiagnosticEvent::StaDisconnected, telemetryBackoffLevelMinutes());
			String message = String(F("STA disconnected: reason=")) + reason;
			if (lastStaRssi != 0) {
				message += F(" RSSI=");
				message += lastStaRssi;
			} else {
				message += F(" RSSI=unavailable");
			}
			message += F(" channel=");
			message += WiFi.channel();
			Debug.addLine(message);
		}
	}
	if (wifiLogDropped != 0) {
		uint8_t dropped = wifiLogDropped;
		wifiLogDropped = 0;
		Debug.addLine(String(F("Wi-Fi event log queue overflow; dropped ")) + dropped + F(" events"));
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

void handleISpindel() {
	if (!httpServer.hasArg("plain")) {
		httpServer.send(400, "text/plain", "Missing body");
		return;
	}

	JsonDocument localDoc;
	DeserializationError error = deserializeJson(localDoc, httpServer.arg("plain"));
	if (error) {
		httpServer.send(400, "text/plain", "JSON parse error");
		return;
	}

	// Extract known fields into typed globals
	if (localDoc["temperature"].is<float>()) {
		iSpindelTemperature = localDoc["temperature"].as<float>();
		lastISpindelMillis = millis();
	}
	if (localDoc["gravity"].is<float>()) {
		iSpindelGravity = localDoc["gravity"].as<float>();
	}
	if (localDoc["battery"].is<float>()) {
		iSpindelBattery = localDoc["battery"].as<float>();
	}
	if (localDoc["RSSI"].is<int>()) {
		iSpindelRssi = localDoc["RSSI"].as<int>();
	} else if (localDoc["rssi"].is<int>()) {
		iSpindelRssi = localDoc["rssi"].as<int>();
	}
	if (localDoc["interval"].is<int>()) {
		int repInterval = localDoc["interval"].as<int>();
		if (repInterval >= 30 && repInterval <= 86400) {
			iSpindelIntervalSec = repInterval;
		}
	}

	// Capture any additional fields for verbatim forwarding to the telemetry endpoint.
	// Known fields that are already forwarded as typed values are skipped.
	iSpindelExtraCount = 0;
	static const char *const KNOWN_KEYS[] = {
		"temperature", "gravity", "battery", "RSSI", "rssi", "interval", nullptr
	};
	for (JsonPairConst kv : localDoc.as<JsonObjectConst>()) {
		const char *rawKey = kv.key().c_str();
		bool known = false;
		for (uint8_t k = 0; KNOWN_KEYS[k] != nullptr; ++k) {
			if (strcmp(rawKey, KNOWN_KEYS[k]) == 0) {
				known = true;
				break;
			}
		}
		if (known || iSpindelExtraCount >= ISPINDEL_EXTRA_MAX) {
			continue;
		}
		// Build "ispindel_<key>" prefix (lower-cased)
		char prefixed[24];
		snprintf(prefixed, sizeof(prefixed), "ispindel_");
		size_t offset = strlen(prefixed);
		for (size_t i = 0; rawKey[i] && offset < sizeof(prefixed) - 1; ++i) {
			prefixed[offset++] = static_cast<char>(tolower(static_cast<unsigned char>(rawKey[i])));
		}
		prefixed[offset] = '\0';
		strlcpy(iSpindelExtraKeys[iSpindelExtraCount], prefixed, sizeof(iSpindelExtraKeys[0]));
		// Serialise the value as compact JSON so any type (number/string/bool) is preserved
		String valStr;
		serializeJson(kv.value(), valStr);
		strlcpy(iSpindelExtraValues[iSpindelExtraCount], valStr.c_str(), sizeof(iSpindelExtraValues[0]));
		++iSpindelExtraCount;
	}

	hasNewISpindelData = true;
	if (isBootAutoStartPending()) {
		cancelBootAutoStart();
		setCoolerStopped(LOW);
	}
	updateCascadeTarget();
	httpServer.send(200, "text/plain", "OK");
	sendTelemetryNow = true;

	String clientMac = getISpindelClientMac();
	String logMsg = F("iSpindel data received: MAC=");
	logMsg += (clientMac.length() > 0 ? clientMac : F("unknown"));
	if (iSpindelRssi != 0) {
		logMsg += F(", RSSI=");
		logMsg += String(iSpindelRssi);
		logMsg += F("dBm");
	}
	Debug.addLine(logMsg);
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

bool clockValid() {
	return time(nullptr) >= 1700000000;
}

uint32_t epochSeconds() {
	return static_cast<uint32_t>(time(nullptr));
}

void setup() {
	pinMode(HOT_FAN, OUTPUT);
	digitalWrite(HOT_FAN, HIGH);
	pinMode(LED_WARN, OUTPUT);
	digitalWrite(LED_WARN, LOW);

	initHeating();
	initCooling();
	initTemperatureControl();

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
	setenv("TZ", tzSetting, 1);
	tzset();
	maxVoltage_backup = maxVoltage; // update backup
	initBootAutoStartState(useISpindelTemp);

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
		configTime(tzSetting, "pool.ntp.org", "time.google.com");
		WiFi.mode(WIFI_AP_STA);
		WiFi.setOutputPower(20.5f); // Always use maximum transmit power (+20.5 dBm)

		WiFi.setAutoConnect(false);
		WiFi.setAutoReconnect(false);
		staGotIpHandler = WiFi.onStationModeGotIP(onStationGotIp);
		staConnectedHandler = WiFi.onStationModeConnected(onStationConnected);
		staDisconnectedHandler = WiFi.onStationModeDisconnected(onStationDisconnected);
		apStationConnectedHandler = WiFi.onSoftAPModeStationConnected(onApStationConnected);
		apStationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(onApStationDisconnected);
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

		if (!ensureNormalAp()) {
			Debug.addLine(F("Normal AP unavailable until a safe subnet can be selected"));
		}
	}

	httpServer.on("/ispindel", handleISpindel);
	httpServer.on("/api/ispindel", handleISpindel);
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
	httpServer.on("/api/link-log", HTTP_GET, guardedHandler(handleLinkLog));
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

void handleTemperatureControl() {
	checkBootAutoStartTimeout();
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
	processWifiEventLogs();

	if (pendingUpstreamIp) {
		pendingUpstreamIp = false;
		if (!emergencyMode) {
			int connectedChannel = WiFi.channel();
			uint8_t *connectedBssid = WiFi.BSSID();
			if (connectedChannel > 0 && connectedBssid != nullptr) {
				lastCh = connectedChannel;
				for (uint8_t i = 0; i < 6; ++i) {
					lastBssid[i] = connectedBssid[i];
				}
				Debug.addLine(String(F("Cached router BSSID/channel for reconnect: channel=")) + lastCh);
			}
			reconnectAttemptCount = 0;
			reconnectScheduleAttemptCount = 0;
			reconnectScheduleActive = false;
			reconnectBeginIssued = false;
			ensureNormalAp();
		}
	}

	// Monitor Wi-Fi connection state and reconnect if dropped
	if (!emergencyMode && (millis() - millisWIFIcheckInterval >= 3000UL)) {
		millisWIFIcheckInterval = millis();
		static bool clockWasValid = false;
		bool clockIsValid = time(nullptr) >= 1700000000;
		if (clockIsValid && !clockWasValid) {
			Debug.addLine(String(F("Time synchronized (timezone: ")) + tzSetting + F(")"));
		}
		clockWasValid = clockIsValid;
		bool isConnected = (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0));
		if (!emergencyMode) {
			ensureNormalAp();
		}
		if (!isConnected) {
			if (wifiWasConnected) {
				wifiWasConnected = false;
				wifiDropOccurred = true;
				lastWifiDropMillis = millis();
				reconnectScheduleStartMillis = lastWifiDropMillis;
				reconnectScheduleAttemptCount = 0;
				reconnectScheduleActive = true;
				reconnectBeginIssued = false;
				Debug.addLine(F("Wi-Fi connection lost"));
			}
			if (lastStaRssi != 0) {
				lastStaRssi = 0;
			}
			if (!reconnectScheduleActive) {
				reconnectScheduleStartMillis = millis();
				reconnectScheduleAttemptCount = 0;
				reconnectScheduleActive = true;
				reconnectBeginIssued = false;
			}
			uint32_t now = millis();
			uint32_t sinceDrop = static_cast<uint32_t>(now - reconnectScheduleStartMillis);
			bool reconnectDue;
			if (reconnectScheduleAttemptCount == 0) {
				reconnectDue = sinceDrop >= 5000UL;
			} else if (reconnectScheduleAttemptCount == 1) {
				reconnectDue = sinceDrop >= 15000UL;
			} else if (reconnectScheduleAttemptCount == 2) {
				reconnectDue = sinceDrop >= 30000UL;
			} else {
				reconnectDue =
					static_cast<uint32_t>(now - lastReconnectBeginMillis) >= 60000UL;
			}
			bool previousAttemptStillStarting =
				reconnectBeginIssued &&
				WiFi.status() == WL_IDLE_STATUS &&
				static_cast<uint32_t>(now - lastReconnectBeginMillis) < 20000UL;
			if (reconnectDue && !previousAttemptStillStarting) {
				Debug.addLine(F("Wi-Fi disconnected: attempting reconnect"));
				lastReconnectBeginMillis = now;
				reconnectBeginIssued = true;
				++reconnectScheduleAttemptCount;
				if (lastCh > 0 && reconnectAttemptCount < 3) {
					++reconnectAttemptCount;
					Debug.addLine(String(F("Reconnecting on cached channel (attempt ")) +
					              reconnectAttemptCount + F("/3, channel=") + lastCh + F(")"));
					WiFi.begin(IntSSID, IntPASS, lastCh, lastBssid, true);
				} else {
					Debug.addLine(F("Reconnecting with full channel scan"));
					WiFi.begin(IntSSID, IntPASS);
					reconnectAttemptCount = 0;
				}
			}
		} else {
			lastStaRssi = WiFi.RSSI();
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

	if (sendTelemetryNow || isTelemetryUploadDue()) {
		sendTelemetryNow = false;
		sendTelemetry();
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
