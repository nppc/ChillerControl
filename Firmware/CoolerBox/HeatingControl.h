#pragma once

#include <Arduino.h>
#include <QuickPID.h>

#include "Globals.h"

enum class AutoTuneState : uint8_t {
	IDLE = 0,
	WARMING_UP,
	CYCLING,
	COMPLETED,
	FAILED
};

struct AutoTuneStatus {
	AutoTuneState state;
	int currentCycle;
	int targetCycles;
	float targetTemp;
	float currentTemp;
	float peakTemp;
	float valleyTemp;
	float calculatedKp;
	float calculatedKi;
	float calculatedKd;
	String message;
};

extern QuickPID hotPID;
extern float hotPID_kP;
extern float hotPID_kI;
extern float hotPID_kD;
extern float setHotPWM;
extern int maxHotPWM;

void initHeating();
void resetHeatingPID();
void setHeatingTunings(float kp, float ki, float kd);
void getHeatingTunings(float &kp, float &ki, float &kd);
void processHeating(float currentTemp, float targetTemp, bool enableHeating);

bool startHeatingAutoTune(float targetTemp, float testPWM = 500.0f, float noiseBand = 0.25f, int cycles = 4);
void stopHeatingAutoTune(const char *reason = "Aborted by user");
bool isHeatingAutoTuning();
AutoTuneStatus getHeatingAutoTuneStatus();

