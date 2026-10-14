#pragma once

#include <Arduino.h>
#include "Globals.h"

extern bool flagExtremeTemp;
extern float maxVoltage_backup;
extern float setVoltage;
extern float changeVoltageSpeed;
extern float minVoltage;
extern float maxVoltage;
extern bool stopCooler;
extern bool coolingZeroVoltage;

void initCooling();
void setCoolerStopped(bool stopped);
bool isCoolerStopped();
bool isCoolingDemanded();
bool evaluateCoolingDemand(float currentTemp, float targetTemp, bool enableCooling);
void processCooling(float boxTemp, float hotTemp, float targetTemp, bool enableCooling);
void updateCoolingOutput(bool forceSend = false, bool sendLimits = false);

