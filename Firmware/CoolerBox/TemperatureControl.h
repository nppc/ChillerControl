#pragma once

#include <Arduino.h>
#include "Globals.h"

// Cascade tuning parameters
extern const float CASCADE_MAX_OFFSET;
extern float cascadeMaxOffset;  // Max temperature offset (°C) box is allowed to depart from setTemp (e.g. 4.0)
extern float cascadeGain;       // Configured base cascade gain (e.g. 1.5)
extern float effectiveBoxTarget;// Current computed target setpoint for the box air loop
extern int iSpindelIntervalSec; // Reported iSpindel interval in seconds (default 900)

void initTemperatureControl();
void processControl();
void updateCascadeTarget();
bool isISpindelActive();
float getFermenterTemp();
inline float getControlTemp() { return getFermenterTemp(); }
float getEffectiveBoxTarget();
float getHeatingCascadeGain();
float getCoolingCascadeGain();
