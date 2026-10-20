#include "TemperatureControl.h"
#include "CoolerBox.h"

namespace {
const float CASCADE_GAIN_MIN = 0.5f;
const float CASCADE_GAIN_MAX = 5.0f;
const float CASCADE_GAIN_STEP = 0.1f;
const float CASCADE_COOLING_GAIN_MULTIPLIER = 2.0f;
const float CASCADE_ADAPT_DEADBAND = 0.15f;
const uint8_t CASCADE_ADAPT_READINGS = 3;

const float DEFAULT_CASCADE_GAIN = 2.0f;
float heatingCascadeGain = DEFAULT_CASCADE_GAIN;
float coolingCascadeGain = DEFAULT_CASCADE_GAIN * CASCADE_COOLING_GAIN_MULTIPLIER;
float lastAdaptedLiquidError = 0.0f;
uint8_t nonImprovingReadings = 0;
int8_t adaptationDirection = 0;
unsigned long lastAdaptedISpindelMillis = 0;
float lastConfiguredCascadeGain = -1.0f;
float lastAdaptedSetTemp = 0.0f;
float cascadeBoxBaseline = 18.0f;
bool cascadeBaselineInitialized = false;
unsigned long lastBaselineISpindelMillis = 0;

static inline bool isBoxTempValid() {
	return (boxTemp > -50.0f && boxTemp < 100.0f && boxTemp != 0.0f);
}

void resetCascadeAdaptation() {
	heatingCascadeGain = constrain(DEFAULT_CASCADE_GAIN, CASCADE_GAIN_MIN, CASCADE_GAIN_MAX);
	coolingCascadeGain = constrain(DEFAULT_CASCADE_GAIN * CASCADE_COOLING_GAIN_MULTIPLIER,
	                               CASCADE_GAIN_MIN, CASCADE_GAIN_MAX);
	lastAdaptedLiquidError = 0.0f;
	nonImprovingReadings = 0;
	adaptationDirection = 0;
	lastAdaptedISpindelMillis = lastISpindelMillis;
	lastConfiguredCascadeGain = DEFAULT_CASCADE_GAIN;
	lastAdaptedSetTemp = setTemp;
	if (isBoxTempValid()) {
		cascadeBoxBaseline = boxTemp;
		cascadeBaselineInitialized = true;
	} else {
		cascadeBoxBaseline = setTemp;
		cascadeBaselineInitialized = false;
	}
	lastBaselineISpindelMillis = lastISpindelMillis;
}

float &gainForDirection(int8_t direction) {
	return direction > 0 ? heatingCascadeGain : coolingCascadeGain;
}

bool isDirectionAvailable(int8_t direction) {
	if (isCoolerStopped()) {
		return false;
	}
	if (boxMode == 1) {
		return direction > 0 ? boxSubMode == 3 : boxSubMode == 2;
	}
	return direction > 0 ? boxMode == 3 : boxMode == 2;
}

void adjustCascadeGain(int8_t direction, float adjustment, const char *reason) {
	float &gain = gainForDirection(direction);
	float adjustedGain = constrain(gain + adjustment, CASCADE_GAIN_MIN, CASCADE_GAIN_MAX);
	if (adjustedGain == gain) {
		return;
	}
	gain = adjustedGain;
	Debug.addLine(String(F("Cascade ")) + (direction > 0 ? F("heating") : F("cooling")) +
	              F(" gain ") + String(gain, 1) + F(": ") + reason);
}

void adaptCascadeGain() {
	if (!isISpindelActive() || lastISpindelMillis == lastAdaptedISpindelMillis) {
		return;
	}
	unsigned long sampleGap = lastISpindelMillis - lastAdaptedISpindelMillis;
	unsigned long maximumSampleGap = max(45UL * 60UL * 1000UL,
	                                    static_cast<unsigned long>(iSpindelIntervalSec) * 2500UL);
	lastAdaptedISpindelMillis = lastISpindelMillis;
	if (sampleGap >= maximumSampleGap || fabs(setTemp - lastAdaptedSetTemp) >= 0.01f) {
		nonImprovingReadings = 0;
		adaptationDirection = 0;
		lastAdaptedSetTemp = setTemp;
	}

	float liquidError = setTemp - iSpindelTemperature;
	if (fabs(liquidError) < CASCADE_ADAPT_DEADBAND) {
		nonImprovingReadings = 0;
		return;
	}

	int8_t direction = liquidError > 0.0f ? 1 : -1;
	if (adaptationDirection != 0 && direction != adaptationDirection) {
		adjustCascadeGain(adaptationDirection, -CASCADE_GAIN_STEP, "liquid temperature crossed setpoint");
		nonImprovingReadings = 0;
		adaptationDirection = direction;
		lastAdaptedLiquidError = liquidError;
		return;
	}

	if (!isDirectionAvailable(direction)) {
		nonImprovingReadings = 0;
		adaptationDirection = 0;
		lastAdaptedLiquidError = liquidError;
		return;
	}

	adaptationDirection = direction;
	bool errorNotImproving = nonImprovingReadings > 0 &&
	                         fabs(liquidError) >= fabs(lastAdaptedLiquidError);
	nonImprovingReadings = errorNotImproving
		? static_cast<uint8_t>(nonImprovingReadings + 1)
		: 1;
	lastAdaptedLiquidError = liquidError;

	if (nonImprovingReadings >= CASCADE_ADAPT_READINGS) {
		float rawOffset = liquidError * gainForDirection(direction);
		bool offsetSaturated = fabs(rawOffset) >= cascadeMaxOffset;
		if (!offsetSaturated) {
			adjustCascadeGain(direction, CASCADE_GAIN_STEP, "liquid error not improving across 3 reports");
		}
		nonImprovingReadings = 0;
	}
}
}

const float CASCADE_MAX_OFFSET = 4.0f;  // Maximum offset (°C) box air is allowed to depart from setTemp
float cascadeMaxOffset = CASCADE_MAX_OFFSET;
float cascadeGain = DEFAULT_CASCADE_GAIN;
float effectiveBoxTarget = 19.0f;
int iSpindelIntervalSec = 900;  // 15 minutes default

void initTemperatureControl() {
	cascadeBaselineInitialized = false;
	effectiveBoxTarget = setTemp;
	resetCascadeAdaptation();
}

bool isISpindelActive() {
	if (!useISpindelTemp || lastISpindelMillis == 0) {
		return false;
	}
	// Fallback timeout: 2.5x the iSpindel interval or at least 45 minutes
	unsigned long timeoutMs = max(45UL * 60UL * 1000UL, (unsigned long)iSpindelIntervalSec * 2500UL);
	return (millis() - lastISpindelMillis < timeoutMs) &&
	       (iSpindelTemperature > -50.0f && iSpindelTemperature < 100.0f && iSpindelTemperature != 0.0f);
}

float getFermenterTemp() {
	if (isISpindelActive()) {
		return iSpindelTemperature;
	}
	if (useISpindelTemp && lastISpindelMillis == 0) {
		// Waiting for the first iSpindel report after boot:
		// Assume setTemp to avoid premature heating/cooling before actual liquid telemetry is known
		return setTemp;
	}
	return boxTemp;
}

float getEffectiveBoxTarget() {
	return effectiveBoxTarget;
}

float getHeatingCascadeGain() {
	return heatingCascadeGain;
}

float getCoolingCascadeGain() {
	return coolingCascadeGain;
}

float getCascadeBoxBaseline() {
	return cascadeBoxBaseline;
}

void updateCascadeTarget() {
	if (fabs(cascadeGain - lastConfiguredCascadeGain) >= 0.001f) {
		resetCascadeAdaptation();
	}
	if (fabs(setTemp - lastAdaptedSetTemp) >= 0.01f) {
		lastAdaptedSetTemp = setTemp;
		if (isBoxTempValid()) {
			cascadeBoxBaseline = boxTemp;
			cascadeBaselineInitialized = true;
		} else {
			cascadeBoxBaseline = setTemp;
			cascadeBaselineInitialized = false;
		}
	}

	// Latch real box temperature as initial baseline as soon as sensor reading becomes valid
	if (!cascadeBaselineInitialized && isBoxTempValid()) {
		cascadeBoxBaseline = boxTemp;
		cascadeBaselineInitialized = true;
	}

	if (isISpindelActive()) {
		adaptCascadeGain();

		float error = setTemp - iSpindelTemperature;

		// Adapt thermal baseline when liquid is holding near target (|error| <= CASCADE_ADAPT_DEADBAND)
		// and a new iSpindel report has arrived.
		if (lastISpindelMillis != lastBaselineISpindelMillis) {
			lastBaselineISpindelMillis = lastISpindelMillis;
			if (isBoxTempValid()) {
				if (!cascadeBaselineInitialized) {
					cascadeBoxBaseline = boxTemp;
					cascadeBaselineInitialized = true;
				} else if (fabs(error) <= CASCADE_ADAPT_DEADBAND) {
					// Smoothly adapt baseline toward current steady box air temperature (15% per report)
					cascadeBoxBaseline = 0.85f * cascadeBoxBaseline + 0.15f * boxTemp;
				}
				cascadeBoxBaseline = constrain(cascadeBoxBaseline, setTemp - cascadeMaxOffset, setTemp + cascadeMaxOffset);
			}
		}

		// Use separate learned gains because heating and cooling capacity differ.
		float gain = error >= 0.0f ? heatingCascadeGain : coolingCascadeGain;
		float offset = constrain(error * gain, -cascadeMaxOffset, cascadeMaxOffset);

		// Box target is anchored at the thermal equilibrium baseline instead of hardcoded setTemp
		effectiveBoxTarget = constrain(cascadeBoxBaseline + offset, setTemp - cascadeMaxOffset, setTemp + cascadeMaxOffset);
	} else {
		if (useISpindelTemp) {
			// iSpindel is configured but waiting for first report or temporarily offline:
			// anchor target to baseline/boxTemp so we don't blast heat/cool before receiving wort telemetry
			effectiveBoxTarget = isBoxTempValid() ? cascadeBoxBaseline : setTemp;
		} else {
			// Direct box control mode: user explicitly regulates box air to setTemp
			effectiveBoxTarget = setTemp;
			cascadeBoxBaseline = setTemp;
		}
	}
}

void processControl() {
	static bool lastISpindelActive = false;
	bool currentISpindelActive = isISpindelActive();
	if (currentISpindelActive != lastISpindelActive) {
		if (currentISpindelActive) {
			Debug.addLine(String(F("Cascade control active: iSpindel=")) + String(iSpindelTemperature, 2) + F("°C"));
		} else if (useISpindelTemp) {
			Debug.addLine(String(F("iSpindel offline/timed out; falling back to direct Box control (")) + String(boxTemp, 2) + F("°C)"));
		}
		lastISpindelActive = currentISpindelActive;
	}

	// Update the dynamic box setpoint from cascade master loop
	updateCascadeTarget();

	// Autotune mode override: dedicated to tuning the fast box air loop
	if (isHeatingAutoTuning()) {
		processCooling(boxTemp, HotTemp, setTemp, false); // cooling disabled
		digitalWrite(HOT_FAN, LOW); // Fan is always OFF during heating autotune

		float safePeltierThreshold = max(2.5f, minVoltage + 0.5f);
		bool peltierSafe = !I2C_PRESENT || (measuredVoltage < safePeltierThreshold);
		processHeating(boxTemp, getHeatingAutoTuneTargetTemp(), peltierSafe);
	} else {
		/*
		 * Asymmetric Mode Switching & Cascade Safety Interlock
		 * ======================================================
		 * For outdoor balcony setups where ambient is cool and heating is 95% more powerful:
		 * - While fermenter >= target: cooling is active; heating is strictly locked out.
		 * - When fermenter drops below (target - hysteresis): gentle heating is engaged.
		 * - Dynamic fan stays ON until hot-side radiator drops to or below box temperature.
		 */
		bool isStopped = isCoolerStopped();
		float controlTemp = getFermenterTemp();

		if (boxMode == 1 && !isStopped) {
			if (boxSubMode == 3) { // Currently in Heating sub-mode
				// Switch to Cooling only when control temperature rises above target by hysteresis
				if (controlTemp >= (setTemp + DEFAULT_HYSTERESIS)) {
					boxSubMode = 2; // Switch to Cooling
					Debug.addLine(String(F("Control temp (")) + String(controlTemp, 2) +
					              F("°C) >= target (") + String(setTemp + DEFAULT_HYSTERESIS, 2) + F("°C): switch to Cooling"));
				}
			} else { // Currently in Cooling sub-mode (boxSubMode == 2)
				// Switch to Heating ONLY when control temperature drops below target by hysteresis.
				// Heating is NEVER turned on while fermenter is at or above target!
				if (controlTemp < (setTemp - DEFAULT_HYSTERESIS)) {
					boxSubMode = 3; // Switch to Heating
					Debug.addLine(String(F("Control temp (")) + String(controlTemp, 2) +
					              F("°C) < target (") + String(setTemp - DEFAULT_HYSTERESIS, 2) + F("°C): switch to Heating"));
				}
			}

			// In cooling sub-mode, cut Peltier voltage to 0.0V when boxTemp is at or below target
			if (boxSubMode == 2) {
				coolingZeroVoltage = (boxTemp < (effectiveBoxTarget - DEFAULT_HYSTERESIS));
			} else {
				coolingZeroVoltage = false;
			}
		} else {
			if (boxMode == 2 && !isStopped) {
				coolingZeroVoltage = (boxTemp < (effectiveBoxTarget - DEFAULT_HYSTERESIS));
			} else {
				coolingZeroVoltage = false;
			}
		}

		bool isHeating = (boxMode == 3) || (boxMode == 1 && boxSubMode == 3);
		bool fermenterNeedsHeating = !isISpindelActive() || (iSpindelTemperature <= (setTemp + DEFAULT_HYSTERESIS));
		bool fermenterNeedsCooling = !isISpindelActive() || (iSpindelTemperature > (setTemp - DEFAULT_HYSTERESIS));
		bool isCooling = !isHeating && !isStopped && fermenterNeedsCooling;

		// Reset PID memory whenever transitioning into Heating mode
		static bool wasHeating = false;
		static bool wasStopped = true;
		static int lastSubMode = -1;

		bool enteredHeating = (!wasHeating && isHeating && !isStopped) ||
		                      (wasStopped && !isStopped && isHeating) ||
		                      (boxSubMode == 3 && lastSubMode != 3 && !isStopped);
		if (enteredHeating) {
			resetHeatingPID();
			Debug.addLine(F("Switched to Heating: PID memory fully reset"));
		}
		wasHeating = isHeating;
		wasStopped = isStopped;
		lastSubMode = boxSubMode;

		// Execute separated cooling logic (slave loop drives boxTemp to effectiveBoxTarget)
		processCooling(boxTemp, HotTemp, effectiveBoxTarget, isCooling);

		// Execute separated heating logic (strictly locked out while fermenter is at/above target)
		float safePeltierThreshold = max(2.5f, minVoltage + 0.5f);
		bool heatingAllowed = !isStopped && fermenterNeedsHeating && (measuredVoltage < safePeltierThreshold) && isHeating;
		processHeating(boxTemp, effectiveBoxTarget, heatingAllowed);

		// Log control state changes
		static int lastLoggedMode = -1;
		static int lastLoggedSubMode = -1;
		static bool lastLoggedStopState = false;
		static bool lastLoggedDemand = false;
		static float lastLoggedSetVoltage = -1.0f;
		static bool lastLoggedHeating = false;

		bool demand = isCoolingDemanded();
		bool stateChanged = (boxMode != lastLoggedMode || boxSubMode != lastLoggedSubMode ||
		                     isStopped != lastLoggedStopState || isHeating != lastLoggedHeating);
		bool coolingChanged = (!isHeating && !isStopped && (demand != lastLoggedDemand || setVoltage != lastLoggedSetVoltage));

		if (stateChanged || coolingChanged) {
			String msg;
			if (isStopped) {
				msg = F("Cooler stopped: boxTemp=");
				msg += String(boxTemp, 2);
				msg += F(" boxTarget=");
				msg += String(effectiveBoxTarget, 2);
			} else if (isHeating) {
				msg = F("Heating: boxTemp=");
				msg += String(boxTemp, 2);
				msg += F(" boxTarget=");
				msg += String(effectiveBoxTarget, 2);
			} else {
				msg = F("Cooling: boxTemp=");
				msg += String(boxTemp, 2);
				msg += F(" boxTarget=");
				msg += String(effectiveBoxTarget, 2);
				msg += F(" reqVoltage=");
				msg += String(setVoltage, 1);
				msg += F("V");
			}
			if (isISpindelActive()) {
				msg += F(" (Cascade: fermenter=") + String(iSpindelTemperature, 2) + F(" setTemp=") + String(setTemp, 1) + F(")");
			}
			Debug.addLine(msg);

			lastLoggedMode = boxMode;
			lastLoggedSubMode = boxSubMode;
			lastLoggedStopState = isStopped;
			lastLoggedDemand = demand;
			lastLoggedSetVoltage = setVoltage;
			lastLoggedHeating = isHeating;
		}

		// Control Fan: turn off ONLY when hot temperature gets equal or below box temperature
		if (HotSensorId == ColdSensorId) {
			digitalWrite(HOT_FAN, HIGH);
		} else {
			bool coolingActive = !isHeating && !isStopped && (isCoolingDemanded() || setVoltage > 0.0f);
			if (coolingActive || HotTemp > (boxTemp + 0.5f)) {
				digitalWrite(HOT_FAN, HIGH);
			} else if (HotTemp <= boxTemp) {
				digitalWrite(HOT_FAN, LOW);
			}
		}
	}

	// Send data to IoT
	if (!emergencyMode && (millis() - millisSendDataInterval > static_cast<unsigned long>(sendInterval) * 1000UL)) {
		if (clockValid()) {
			captureTelemetrySnapshot();
			millisSendDataInterval = millis();
		}
	}
}
