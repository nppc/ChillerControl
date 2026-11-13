#pragma once

#include <Arduino.h>

// iSpindel measurement data & state
extern float iSpindelTemperature;
extern float iSpindelPlato;
extern unsigned long lastISpindelMillis;
extern int iSpindelRssi;
extern int iSpindelIntervalSec;

// Web server handler for iSpindel HTTP reports
void handleISpindel();

// Stability & fermentation completion tracking (Option A: 48 hourly buckets stored as int16_t)
void recordPlatoReading(float plato);
void updatePlatoStability();
bool isFermentationComplete();
uint8_t getPlatoHistoryHours();
uint8_t getPlatoStableHours();
float getPlato48hSlope();
float getPlato48hDelta();
