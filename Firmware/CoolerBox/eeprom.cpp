#include "CoolerBox.h"

namespace {
const size_t SETTINGS_EEPROM_SIZE = 1024;
const int SETTINGS_EEPROM_HEADER_SIZE = sizeof(uint32_t) + sizeof(uint16_t);
const uint32_t SETTINGS_EEPROM_MAGIC = 0x434F4F4C;
bool settingsEepromReady = false;

void applySettings(JsonDocument &doc) {
	if (doc["apSSID"].is<JsonVariantConst>() && !doc["apSSID"].as<String>().isEmpty()) {
		String s = doc["apSSID"].as<String>();
		s.toCharArray(apSSID, sizeof(apSSID));
	} else if (doc["APSSID"].is<JsonVariantConst>() && !doc["APSSID"].as<String>().isEmpty()) {
		String s = doc["APSSID"].as<String>();
		s.toCharArray(apSSID, sizeof(apSSID));
	} else {
		strncpy(apSSID, "Cooler", sizeof(apSSID));
		apSSID[sizeof(apSSID) - 1] = '\0';
	}

	if (doc["apPassword"].is<JsonVariantConst>()) {
		String s = doc["apPassword"].as<String>();
		s.toCharArray(apPassword, sizeof(apPassword));
	} else if (doc["apPASS"].is<JsonVariantConst>()) {
		String s = doc["apPASS"].as<String>();
		s.toCharArray(apPassword, sizeof(apPassword));
	} else if (doc["APPASS"].is<JsonVariantConst>()) {
		String s = doc["APPASS"].as<String>();
		s.toCharArray(apPassword, sizeof(apPassword));
	} else {
		apPassword[0] = '\0';
	}
	apHidden = doc["apHidden"] | false;
	allowApWeb = doc["allowApWeb"] | false;
	useISpindelTemp = doc["useISpindelTemp"] | true;
	cascadeMaxOffset = doc["cascadeMaxOffset"] | 4.0f;
	cascadeGain = doc["cascadeGain"] | 1.5f;

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
	hotPID_kP = doc["hotPID_kP"];
	hotPID_kI = doc["hotPID_kI"];
	hotPID_kD = doc["hotPID_kD"];
	setHeatingTunings(hotPID_kP, hotPID_kI, hotPID_kD);
	ColdSensorId = doc["ColdSensorId"];
	HotSensorId = doc["HotSensorId"];
	fanDynamic_checked = doc["fanDynamic_checked"];
	boxMode = doc["boxMode"];
	minVoltage = doc["i2cDCDC_minVoltage"];
	maxVoltage = doc["i2cDCDC_maxVoltage"];
	changeVoltageSpeed = doc["i2cDCDC_changeVoltage"];
}
}

void initEEPROM() {
	if (!settingsEepromReady) {
		EEPROM.begin(SETTINGS_EEPROM_SIZE);
		settingsEepromReady = true;
	}
}

bool restoreSettingsFromEEPROM(){
	if (!settingsEepromReady) {
		initEEPROM();
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
	return true;
}

void initDefaultSettings() {
	strncpy(apSSID, "Cooler", sizeof(apSSID));
	apSSID[sizeof(apSSID) - 1] = '\0';
	apPassword[0] = '\0';
	apHidden = false;
	allowApWeb = false;
	useISpindelTemp = true;
	cascadeMaxOffset = 4.0f;
	cascadeGain = 1.5f;
	IntSSID[0] = '\0';
	IntPASS[0] = '\0';
	endpointAddress = "";
	endpointAccessToken = "";
	endpointPath = "/telemetry";
	sendInterval = 60;
	setTemp = 19.0;
	hotPID_kP = DEFAULT_PID_KP;
	hotPID_kI = DEFAULT_PID_KI;
	hotPID_kD = DEFAULT_PID_KD;
	setHeatingTunings(hotPID_kP, hotPID_kI, hotPID_kD);
	ColdSensorId = 0;
	HotSensorId = 0;
	fanDynamic_checked = 0;
	boxMode = 2; // 2-Cooling
	minVoltage = 2.0f;
	maxVoltage = 12.0f;
	changeVoltageSpeed = 0.4f;
}

void restoreSettings(){
	if (restoreSettingsFromEEPROM()) {
		return;
	}

	Debug.addLine(F("No EEPROM settings found; initializing defaults"));
	initDefaultSettings();
	saveSettings();
}

void saveSettings(){
	if (!settingsEepromReady) {
		initEEPROM();
	}
	JsonDocument doc;
	doc["apSSID"] = String(apSSID);
	doc["apPassword"] = String(apPassword);
	doc["apHidden"] = apHidden;
	doc["allowApWeb"] = allowApWeb;
	doc["useISpindelTemp"] = useISpindelTemp;
	doc["cascadeMaxOffset"] = cascadeMaxOffset;
	doc["cascadeGain"] = cascadeGain;
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
