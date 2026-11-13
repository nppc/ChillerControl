#pragma once

#include <Arduino.h>
#include "Globals.h"
#include "iSpindel.h"

// Cascade tuning parameters
extern const float CASCADE_MAX_OFFSET;
extern float cascadeMaxOffset;  // Max temperature offset (°C) box is allowed to depart from setTemp (e.g. 4.0)
extern float effectiveBoxTarget;// Current computed target setpoint for the box air loop
extern float outerPID_kP;
extern float outerPID_kIPerHour;
extern float outerPID_tdHours;
extern float outerPID_forecastLimitC;
extern float outerPID_freezeRateCPerHour;

void initTemperatureControl();
bool setTemperatureTarget(float target, bool immediate, float stepC, uint16_t intervalMinutes);
float getRampTargetTemp();
float getRampStepC();
uint16_t getRampIntervalMinutes();
bool isSetTempRamping();
bool setOuterPIDSettings(float kp, float kiPerHour, float tdHours,
                         float forecastLimitC, float freezeRateCPerHour,
                         float maxOffset);
void resetOuterPIDSettings();
void processControl();
void updateCascadeTarget();
bool isISpindelActive();
float getFermenterTemp();
float getPredictedFermenterTemp();
inline float getControlTemp() { return getFermenterTemp(); }
float getEffectiveBoxTarget();
float getHeatingCascadeGain();
float getCoolingCascadeGain();
float getCascadeBoxBaseline();
