#include "CoolerBox.h"

#include <time.h>
#include <Ticker.h>
extern "C" {
#include <user_interface.h>
#include <lwip/priv/tcp_priv.h>
}

uint8_t getTcpActivePcbCount() {
	uint8_t count = 0;
	for (struct tcp_pcb *pcb = tcp_active_pcbs; pcb != nullptr; pcb = pcb->next) {
		count++;
	}
	return count;
}

uint8_t getTcpTwPcbCount() {
	uint8_t count = 0;
	for (struct tcp_pcb *pcb = tcp_tw_pcbs; pcb != nullptr; pcb = pcb->next) {
		count++;
	}
	return count;
}

struct RtcStallDiag {
	uint32_t magic;
	char stage[8];
	uint32_t durationMs;
	uint32_t heap;
	uint8_t pcbActive;
	uint8_t pcbTw;
	uint8_t pad[2];
};

static Ticker stallTicker;
static volatile uint32_t loopCycleCounter = 0;
static volatile const char *currentLoopStage = "init";
static volatile unsigned long stageStartMillis = 0;

static void IRAM_ATTR checkStall() {
	static uint32_t lastCycle = 0;
	static uint8_t stallCount = 0;
	if (loopCycleCounter == lastCycle) {
		stallCount++;
		if (stallCount == 3) {
			uint32_t dur = millis() - stageStartMillis;
			ets_printf("\n*** STALL DETECTED in [%s] for %u ms (hp=%u stk=%u pcb=%u/%u) ***\n",
			           currentLoopStage, dur,
			           ESP.getFreeHeap(), ESP.getFreeContStack(),
			           getTcpActivePcbCount(), getTcpTwPcbCount());
			RtcStallDiag diag;
			diag.magic = 0x5354414C;
			strlcpy(diag.stage, (const char *)currentLoopStage, sizeof(diag.stage));
			diag.durationMs = dur;
			diag.heap = ESP.getFreeHeap();
			diag.pcbActive = getTcpActivePcbCount();
			diag.pcbTw = getTcpTwPcbCount();
			ESP.rtcUserMemoryWrite(70, (uint32_t *)&diag, sizeof(diag));
		}
	} else {
		lastCycle = loopCycleCounter;
		stallCount = 0;
	}
}

static inline void setLoopStage(const char *stage) {
	currentLoopStage = stage;
	stageStartMillis = millis();
}

DebugHTML Debug;
const char DEFAULT_TIMEZONE[] = "EET-2EEST,M3.5.0/3,M10.5.0/4";
char tzSetting[64] = "EET-2EEST,M3.5.0/3,M10.5.0/4";

float boxTemp = 0.0f;	// current measured temperature inside the box
float HotTemp = 0.0f;	// current measured temperature of hot radiator
float setTemp = 19.0f;	// preset temperature
bool useISpindelTemp = true;
float iSpindelTemperature = 0.0f;
unsigned long lastISpindelMillis = 0;
int iSpindelRssi = 0;
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
#ifdef WIFI_DEBUG
WiFiEventHandler apStationConnectedHandler;
#endif
volatile bool sendTelemetryNow = false;
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
#ifdef WIFI_DEBUG
			Debug.addLine(F("Warning: upstream subnet overlaps the default AP subnet; the AP was moved"));
#endif
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
#ifdef WIFI_DEBUG
		Debug.addLine(String(F("AP IP address: ")) + WiFi.softAPIP().toString());
#endif
	}
	updateApDns();
	return true;
}

void onStationGotIp(const WiFiEventStationModeGotIP &) {
	pendingUpstreamIp = true;
}

#ifdef WIFI_DEBUG
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
#endif

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

	// Update the values used by control and wake-window scheduling.
	if (localDoc["temperature"].is<float>()) {
		iSpindelTemperature = localDoc["temperature"].as<float>();
		lastISpindelMillis = millis();
	}
	if (localDoc["RSSI"].is<int>()) {
		iSpindelRssi = localDoc["RSSI"].as<int>();
	} else if (localDoc["rssi"].is<int>()) {
		iSpindelRssi = localDoc["rssi"].as<int>();
	}
	uint32_t reportedInterval = 0;
	if (localDoc["interval"].is<int>()) {
		int repInterval = localDoc["interval"].as<int>();
		if (repInterval >= 30 && repInterval <= 86400) {
			iSpindelIntervalSec = repInterval;
			reportedInterval = static_cast<uint32_t>(repInterval);
		}
	}

	if (clockValid()) {
		TelemetryISpindelRecord record = {};
		record.t = epochSeconds();
		record.interval = reportedInterval;
		record.temperature = localDoc["temperature"].is<float>()
			? telemetryFixedPoint(localDoc["temperature"].as<float>(), 100) : INT16_MIN;
		record.gravity = localDoc["gravity"].is<float>()
			? telemetryFixedPoint(localDoc["gravity"].as<float>(), 1000) : INT16_MIN;
		record.angle = localDoc["angle"].is<float>()
			? telemetryFixedPoint(localDoc["angle"].as<float>(), 100) : INT16_MIN;
		record.battery = localDoc["battery"].is<float>()
			? telemetryFixedPoint(localDoc["battery"].as<float>(), 100) : INT16_MIN;
		if (localDoc["RSSI"].is<int>()) {
			int rssi = localDoc["RSSI"].as<int>();
			record.rssi = static_cast<int8_t>(constrain(rssi, INT8_MIN + 1, INT8_MAX));
		} else if (localDoc["rssi"].is<int>()) {
			int rssi = localDoc["rssi"].as<int>();
			record.rssi = static_cast<int8_t>(constrain(rssi, INT8_MIN + 1, INT8_MAX));
		} else {
			record.rssi = INT8_MIN;
		}
		telemetryISpindelPush(record);
		sendTelemetryNow = true;
	}
	if (isBootAutoStartPending()) {
		cancelBootAutoStart();
		setCoolerStopped(LOW);
	}
	updateCascadeTarget();
	httpServer.send(200, "text/plain", "OK");

#ifdef WIFI_DEBUG
	String clientMac = getISpindelClientMac();
	String logMsg = F("iSpindel data received: MAC=");
	logMsg += (clientMac.length() > 0 ? clientMac : F("unknown"));
	if (iSpindelRssi != 0) {
		logMsg += F(", RSSI=");
		logMsg += String(iSpindelRssi);
		logMsg += F("dBm");
	}
	Debug.addLine(logMsg);
#else
	Debug.addLine(F("iSpindel data received"));
#endif
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

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	RtcStallDiag prevStall;
	if (ESP.rtcUserMemoryRead(70, (uint32_t *)&prevStall, sizeof(prevStall)) && prevStall.magic == 0x5354414C) {
		Debug.addLine(String(F("Prev stall: [")) + prevStall.stage + F("] ") + prevStall.durationMs +
		              F("ms hp=") + prevStall.heap + F(" pcb=") + prevStall.pcbActive + F("/") + prevStall.pcbTw);
		prevStall.magic = 0;
		ESP.rtcUserMemoryWrite(70, (uint32_t *)&prevStall, sizeof(prevStall));
	}

	stallTicker.attach(1.0f, checkStall);

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
		WiFi.setAutoReconnect(true);
		staGotIpHandler = WiFi.onStationModeGotIP(onStationGotIp);
#ifdef WIFI_DEBUG
		apStationConnectedHandler = WiFi.onSoftAPModeStationConnected(onApStationConnected);
#endif
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
	Debug.stopSerialLogging();
	loopCycleCounter++;
	digitalWrite(LED_BUILTIN, ((millis() / 500) % 2 == 0) ? LOW : HIGH);

	if (Update.isRunning()) {
		drd.stop();
		httpServer.handleClient();
		delay(1);
		return;
	}

	setLoopStage("drd");
	drd.loop();

	if (pendingUpstreamIp) {
		pendingUpstreamIp = false;
		if (!emergencyMode) {
			ensureNormalAp();
		}
	}

	setLoopStage("wifi");
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
#ifdef WIFI_DEBUG
				Debug.addLine(String(F("Wi-Fi connected: ")) + WiFi.localIP().toString());
#else
				Debug.addLine(F("Wi-Fi connected"));
#endif
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

	setLoopStage("hb");
	static unsigned long lastHeartbeatMillis = 0;
	if (millis() - lastHeartbeatMillis >= 60000UL) {
		lastHeartbeatMillis = millis();
		Debug.addLine(String(F("HB: hp=")) + ESP.getFreeHeap() +
		              F(" blk=") + ESP.getMaxFreeBlockSize() +
		              F(" stk=") + ESP.getFreeContStack() +
		              F(" pcb=") + getTcpActivePcbCount() + F("/") + getTcpTwPcbCount() +
		              F(" box=") + telemetryBoxPending() +
		              F(" isp=") + telemetryISpindelPending());
	}

	setLoopStage("tel");
	if (sendTelemetryNow || isTelemetryUploadDue()) {
#ifdef WIFI_DEBUG
		bool wasNow = sendTelemetryNow;
#endif
		sendTelemetryNow = false;
		unsigned long tTel = millis();
		sendTelemetry();
		unsigned long dTel = millis() - tTel;
		if (dTel > 500) {
#ifdef WIFI_DEBUG
			Debug.addLine(String(F("Slow tel (")) + (wasNow ? F("now") : F("due")) + F("): ") + dTel + F("ms"));
#endif
		}
		yield();
	}

	setLoopStage("http");
	unsigned long tHttp = millis();
	httpServer.handleClient();
	unsigned long dHttp = millis() - tHttp;
	if (dHttp > 500) {
		Debug.addLine(String(F("Slow http: ")) + dHttp + F("ms pcb=") +
		              getTcpActivePcbCount() + F("/") + getTcpTwPcbCount());
	}
	delay(1);
	
	setLoopStage("temp");
	unsigned long tTemp = millis();
	handleTemperatureControl();
	unsigned long dTemp = millis() - tTemp;
	if (dTemp > 200) {
		Debug.addLine(String(F("Slow temp: ")) + dTemp + F("ms"));
	}
	
	setLoopStage("i2c");
	if (millis() - millisReadI2CDataInterval > (I2C_INTERVAL * 1000UL)) {
		millisReadI2CDataInterval = millis();
		unsigned long tI2c = millis();
		readI2Cdata();
		unsigned long dI2c = millis() - tI2c;
		if (dI2c > 200) {
			Debug.addLine(String(F("Slow i2c: ")) + dI2c + F("ms"));
		}
	}
	
	setLoopStage("led");
	if (measuredVoltage < 2.0f) {
		digitalWrite(LED_WARN, LOW);
	} else {
		digitalWrite(LED_WARN, HIGH);
	}

	setLoopStage("idle");
}
