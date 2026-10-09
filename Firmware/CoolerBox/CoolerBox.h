#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <OneWire.h>
#include <Wire.h>
#include <DallasTemperature.h>
#include <PID_v1.h>
#include <ArduinoJson.h>
#include "FS.h"

#include "Globals.h"
#include "webpage.htm.h"
#include "DebugHTML.h"

#define I2C_ADDRESS 0x5E
#define I2C_DATALEN 11
#define LED_WARN D0
#define ONE_WIRE_BUS D6
#define HOT_FAN D5
#define HEAT_WIRE D7
#define filterSamples 5

extern DebugHTML Debug;

extern unsigned long I2C_error;
extern bool I2C_PRESENT;
extern uint8_t i2c_data[I2C_DATALEN];
extern bool flagExtremeTemp;
extern float maxVoltage_backup;
extern double boxTemp;
extern double HotTemp;
extern double setTemp;
extern double dynamicTemp;
#if ENABLE_ISPINDEL_DYNAMIC_TEMP
extern double iSpindelTemperature;
extern float dynamicTempDiffCoeff;
#endif
extern double setVoltage;
extern float changeVoltageSpeed;
extern float minVoltage;
extern float maxVoltage;
extern byte i2c_PWM_value;
extern float measuredVoltage;
extern float measuredCurrent;
extern bool stopCooler;
extern double setHotPWM;
extern int maxHotPWM;
extern OneWire oneWire;
extern DallasTemperature DS18B20;
extern DeviceAddress DS18B20_adr[2];
extern unsigned long millisPIDinterval;
extern unsigned long millisSendDataInterval;
extern unsigned long millisReadI2CDataInterval;
extern unsigned long millisWIFIcheckInterval;
extern int ColdSensorId;
extern int HotSensorId;
extern int boxTemp_SmoothArray[filterSamples];
extern int HotTemp_SmoothArray[filterSamples];
extern const char *ssid;
extern const char *password;
extern char IntSSID[20];
extern char IntPASS[20];
extern String endpointAddress;
extern String endpointAccessToken;
extern String endpointPath;
extern int sendInterval;
extern int fanDynamic_checked;
extern int boxMode;
extern int boxSubMode;
extern ESP8266WebServer httpServer;
extern ESP8266HTTPUpdateServer httpUpdater;
extern float coldPID_kP;
extern float coldPID_kI;
extern float coldPID_kD;
extern float hotPID_kP;
extern float hotPID_kI;
extern float hotPID_kD;
extern PID coldPID;
extern PID hotPID;

int digitalSmooth(int rawIn, int *sensSmoothArray);
void INIT_DS18B20();
void restoreSettings();
void saveSettings();
void sendI2Cdata();
void readI2Cdata();
String formatBytes(size_t bytes);
String getContentType(String filename);
bool handleFileRead(String path);
void handleFileUpload();
void handleFileDelete();
void handleFileList();
void file_download();
void handleFSWebServer();
void handleAJAXrequest();
void handleRoot();
String ConstructDS18B20list(int idx);
void handleSettings();
void handleSettingsStore();
void handlePIDs();
void handlePIDsStore();
void handleSetTemp();
void handleStop();
void handleNotFound();
void handleNetwork();
void handleNetworkStore();
void handleDebug();
void handleCSS();
void handleScript();
void sendTelemetry();
