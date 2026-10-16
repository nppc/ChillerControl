#include "CoolingControl.h"
#include "CoolerBox.h"

bool flagExtremeTemp = LOW;
float maxVoltage_backup = 12.0f;
float setVoltage = 0.0f;
float changeVoltageSpeed = 0.4f;
float minVoltage = 2.0f;
float maxVoltage = 12.0f;
bool stopCooler = LOW;
bool coolingZeroVoltage = false;

static bool currentCoolingDemand = false;
static float thermalVoltageCap = 12.0f;

void initCooling() {
	thermalVoltageCap = maxVoltage;
	flagExtremeTemp = LOW;
	currentCoolingDemand = false;
	coolingZeroVoltage = false;
}

static bool pendingBootAutoStart = false;
static unsigned long bootAutoStartDeadline = 0;
const unsigned long BOOT_AUTOSTART_TIMEOUT_MS = 15UL * 60UL * 1000UL;

void setCoolerStopped(bool stopped) {
	if (stopCooler == stopped) {
		return;
	}
	stopCooler = stopped;
	updateCoolingOutput();
	Debug.addLine(stopped ? F("Cooler stopped") : F("Cooler resumed"));
}

bool isCoolerStopped() {
	return stopCooler;
}

void initBootAutoStartState(bool useISpindel) {
	if (useISpindel) {
		stopCooler = HIGH;
		pendingBootAutoStart = true;
		bootAutoStartDeadline = millis() + BOOT_AUTOSTART_TIMEOUT_MS;
		updateCoolingOutput();
		Debug.addLine(F("Boot: Stopped. Waiting for iSpindel data or 15m timeout to start."));
	} else {
		stopCooler = LOW;
		pendingBootAutoStart = false;
		bootAutoStartDeadline = 0;
		updateCoolingOutput();
		Debug.addLine(F("Boot: Direct box control active. Process started."));
	}
}

void cancelBootAutoStart() {
	if (pendingBootAutoStart) {
		pendingBootAutoStart = false;
		bootAutoStartDeadline = 0;
		Debug.addLine(F("Boot auto-start cancelled."));
	}
}

void checkBootAutoStartTimeout() {
	if (pendingBootAutoStart && (millis() >= bootAutoStartDeadline)) {
		pendingBootAutoStart = false;
		bootAutoStartDeadline = 0;
		setCoolerStopped(LOW);
		Debug.addLine(F("Boot auto-start: 15m elapsed without iSpindel data. Process started in direct box mode."));
	}
}

bool isBootAutoStartPending() {
	return pendingBootAutoStart;
}

unsigned long getBootAutoStartRemainingSec() {
	if (!pendingBootAutoStart) {
		return 0;
	}
	long diff = (long)(bootAutoStartDeadline - millis());
	return diff > 0 ? (unsigned long)(diff / 1000L) : 0;
}

bool isCoolingDemanded() {
	return currentCoolingDemand;
}

bool evaluateCoolingDemand(float currentTemp, float targetTemp, bool enableCooling) {
	if (!enableCooling || stopCooler) {
		currentCoolingDemand = false;
	} else {
		if (!currentCoolingDemand) {
			if (currentTemp >= (targetTemp + DEFAULT_HYSTERESIS)) {
				currentCoolingDemand = true;
			}
		} else {
			if (currentTemp <= (targetTemp - DEFAULT_HYSTERESIS)) {
				currentCoolingDemand = false;
			}
		}
	}
	return currentCoolingDemand;
}

void processCooling(float boxTemp, float hotTemp, float targetTemp, bool enableCooling) {
	if (enableCooling && !stopCooler) {
		// Thermal protection for radiator
		if (hotTemp >= RADIATOR_EXTREME_TEMP && !flagExtremeTemp) {
			flagExtremeTemp = HIGH;
			thermalVoltageCap = maxVoltage;
		}
		if (flagExtremeTemp) {
			thermalVoltageCap += (hotTemp >= RADIATOR_EXTREME_TEMP ? -0.1f : 0.1f);
			if (thermalVoltageCap > maxVoltage) { thermalVoltageCap = maxVoltage; }
			if (thermalVoltageCap < minVoltage) { thermalVoltageCap = minVoltage; }
			if (hotTemp < RADIATOR_EXTREME_TEMP && thermalVoltageCap >= maxVoltage) {
				flagExtremeTemp = LOW;
				thermalVoltageCap = maxVoltage;
			}
		}
	} else {
		flagExtremeTemp = LOW;
		thermalVoltageCap = maxVoltage;
	}

	if (stopCooler || !enableCooling) {
		setVoltage = 0.0f;
	} else {
		bool demand = evaluateCoolingDemand(boxTemp, targetTemp, enableCooling);
		float effectiveMax = flagExtremeTemp ? thermalVoltageCap : maxVoltage;
		setVoltage = demand ? effectiveMax : (coolingZeroVoltage ? 0.0f : minVoltage);
	}
	sendI2Cdata();
}

void updateCoolingOutput(bool forceSend, bool sendLimits) {
	bool isHeating = (boxMode == 3) || (boxMode == 1 && boxSubMode == 3);
	if (stopCooler || isHeating) {
		setVoltage = 0.0f;
	} else {
		bool demand = evaluateCoolingDemand(boxTemp, effectiveBoxTarget, true);
		setVoltage = demand ? maxVoltage : (coolingZeroVoltage ? 0.0f : minVoltage);
	}
	sendI2Cdata(forceSend, sendLimits);
}

