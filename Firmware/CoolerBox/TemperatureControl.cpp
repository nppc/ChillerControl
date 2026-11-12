#include "TemperatureControl.h"
#include "CoolerBox.h"

namespace {
const float CASCADE_GAIN_MIN = 0.5f;
const float CASCADE_GAIN_MAX = 5.0f;
const float CASCADE_GAIN_STEP = 0.1f;
const float CASCADE_COOLING_GAIN_MULTIPLIER = 1.5f;
const float CASCADE_ADAPT_DEADBAND = 0.15f;
const uint8_t CASCADE_ADAPT_READINGS = 3;

const float DEFAULT_CASCADE_GAIN = 2.0f;
const float FERMENTER_PREDICTION_HORIZON_HOURS = 0.5f;
const float FERMENTER_MAX_RATE_C_PER_HOUR = 0.75f;
const float FERMENTER_MAX_PREDICTION_C = 0.75f;
const float FERMENTER_RATE_FILTER_ALPHA = 0.15f;
const float FERMENTER_EMERGENCY_OVERTEMP = 0.1f;
const float BOX_TARGET_MAX_SLEW_UP_PER_SEC = 0.08f / 60.0f; // ~0.00133 °C/sec (0.08 °C/min)
const float BOX_TARGET_MAX_SLEW_DOWN_PER_SEC = 0.12f / 60.0f; // ~0.00200 °C/sec (0.12 °C/min)
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
unsigned long lastTrendSampleMillis = 0;
float lastTrendSampleTemp = 0.0f;
float filteredFermenterRate = 0.0f;
unsigned long lastSlewMillis = 0;
bool slewInitialized = false;
bool emergencyHeatLockout = false;
unsigned long lastRampStepMillis = 0;
bool rampInitialized = false;

void updateFermenterTrend() {
	if (!isISpindelActive() || lastISpindelMillis == lastTrendSampleMillis) {
		return;
	}

	unsigned long sampleMillis = lastISpindelMillis;
	float sampleTemp = iSpindelTemperature;
	if (lastTrendSampleMillis != 0) {
		unsigned long elapsedMillis = sampleMillis - lastTrendSampleMillis;
		unsigned long maximumSampleGap = max(45UL * 60UL * 1000UL,
		                                    static_cast<unsigned long>(iSpindelIntervalSec) * 2500UL);
		if (elapsedMillis == 0 || elapsedMillis > maximumSampleGap) {
			filteredFermenterRate = 0.0f;
		} else {
			float elapsedHours = static_cast<float>(elapsedMillis) / 3600000.0f;
			float sampleRate = (sampleTemp - lastTrendSampleTemp) / elapsedHours;
			sampleRate = constrain(sampleRate, -FERMENTER_MAX_RATE_C_PER_HOUR,
			                       FERMENTER_MAX_RATE_C_PER_HOUR);
			filteredFermenterRate += FERMENTER_RATE_FILTER_ALPHA *
			                         (sampleRate - filteredFermenterRate);
		}
	} else {
		filteredFermenterRate = 0.0f;
	}

	lastTrendSampleMillis = sampleMillis;
	lastTrendSampleTemp = sampleTemp;
	float predictedTemp = sampleTemp + filteredFermenterRate * FERMENTER_PREDICTION_HORIZON_HOURS;
	Debug.addLine(String(F("iSpindel trend: liquid=")) + String(sampleTemp, 2) +
	              F("C rate=") + String(filteredFermenterRate, 3) +
	              F("C/h predicted=") + String(predictedTemp, 2) + F("C"));
}

float predictedFermenterTemp() {
	if (!isISpindelActive()) {
		return getFermenterTemp();
	}

	float prediction = filteredFermenterRate * FERMENTER_PREDICTION_HORIZON_HOURS;
	prediction = constrain(prediction, -FERMENTER_MAX_PREDICTION_C,
	                       FERMENTER_MAX_PREDICTION_C);
	return iSpindelTemperature + prediction;
}

bool updateEmergencyHeatLockout() {
	if (!isISpindelActive()) {
		emergencyHeatLockout = false;
		return false;
	}

	if (!emergencyHeatLockout &&
	    iSpindelTemperature > setTemp + FERMENTER_EMERGENCY_OVERTEMP) {
		emergencyHeatLockout = true;
		Debug.addLine(String(F("Emergency heater cutoff: fermenter=")) +
		              String(iSpindelTemperature, 2) + F("C target=") +
		              String(setTemp, 2) + F("C"));
	} else if (emergencyHeatLockout && iSpindelTemperature <= setTemp) {
		emergencyHeatLockout = false;
		Debug.addLine(String(F("Emergency heater cutoff released: fermenter=")) +
		              String(iSpindelTemperature, 2) + F("C target=") +
		              String(setTemp, 2) + F("C"));
	}

	return emergencyHeatLockout;
}

static inline bool isBoxTempValid() {
	return (boxTemp > -50.0f && boxTemp < 100.0f && boxTemp != 0.0f);
}

void resetCascadeAdaptation() {
	heatingCascadeGain = constrain(cascadeGain, CASCADE_GAIN_MIN, CASCADE_GAIN_MAX);
	coolingCascadeGain = constrain(cascadeGain * CASCADE_COOLING_GAIN_MULTIPLIER,
	                               CASCADE_GAIN_MIN, CASCADE_GAIN_MAX);
	lastAdaptedLiquidError = 0.0f;
	nonImprovingReadings = 0;
	adaptationDirection = 0;
	lastAdaptedISpindelMillis = lastISpindelMillis;
	lastConfiguredCascadeGain = cascadeGain;
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
float rampTargetTemp = 19.0f;
float rampStepC = 0.1f;
uint16_t rampIntervalMinutes = 120;
bool rampBootSyncDone = false;

bool setTemperatureTarget(float target, bool immediate, float stepC, uint16_t intervalMinutes) {
	if (!isfinite(target) || target < 0.0f || target > 45.0f) {
		return false;
	}
	if (!immediate && (!isfinite(stepC) || stepC < 0.01f || stepC > 5.0f ||
	                   intervalMinutes < 1 || intervalMinutes > 10080)) {
		return false;
	}

	rampTargetTemp = target;
	if (immediate) {
		setTemp = target;
	} else {
		rampStepC = stepC;
		rampIntervalMinutes = intervalMinutes;
	}
	lastRampStepMillis = millis();
	rampInitialized = true;
	return true;
}

float getRampTargetTemp() {
	return rampTargetTemp;
}

float getRampStepC() {
	return rampStepC;
}

uint16_t getRampIntervalMinutes() {
	return rampIntervalMinutes;
}

bool isSetTempRamping() {
	return fabs(rampTargetTemp - setTemp) >= 0.001f;
}

void initTemperatureControl() {
	cascadeBaselineInitialized = false;
	effectiveBoxTarget = setTemp;
	lastSlewMillis = 0;
	lastRampStepMillis = millis();
	rampInitialized = true;
	rampBootSyncDone = false;
	resetCascadeAdaptation();
}

static void updateSetTempRamp() {
	if (!rampInitialized) {
		lastRampStepMillis = millis();
		rampInitialized = true;
		return;
	}

	if (!isSetTempRamping()) {
		bool targetChanged = setTemp != rampTargetTemp;
		setTemp = rampTargetTemp;
		lastRampStepMillis = millis();
		if (targetChanged) {
			saveSettings();
		}
		return;
	}

	// On first call after boot where iSpindel is active, snap setTemp to one
	// step ahead of the fermenter temperature so the ramp resumes near where
	// it was interrupted rather than restarting from the saved start point.
	if (!rampBootSyncDone && isISpindelActive()) {
		rampBootSyncDone = true;
		if (rampTargetTemp > setTemp) {
			// Rising ramp: resume from fermenter + one step, clamped to [setTemp, rampTargetTemp]
			float resumeFrom = iSpindelTemperature + rampStepC;
			setTemp = min(rampTargetTemp, max(setTemp, resumeFrom));
		} else {
			// Falling ramp: resume from fermenter - one step, clamped to [rampTargetTemp, setTemp]
			float resumeFrom = iSpindelTemperature - rampStepC;
			setTemp = max(rampTargetTemp, min(setTemp, resumeFrom));
		}
		lastRampStepMillis = millis();
		return;
	}

	unsigned long now = millis();
	unsigned long intervalMs = static_cast<unsigned long>(rampIntervalMinutes) * 60000UL;
	unsigned long elapsedMs = now - lastRampStepMillis;
	unsigned long steps = elapsedMs / intervalMs;
	if (steps == 0) {
		return;
	}

	lastRampStepMillis += steps * intervalMs;
	float change = rampStepC * static_cast<float>(steps);
	if (rampTargetTemp > setTemp) {
		setTemp = min(rampTargetTemp, setTemp + change);
	} else {
		setTemp = max(rampTargetTemp, setTemp - change);
	}
	if (!isSetTempRamping()) {
		setTemp = rampTargetTemp;
		saveSettings();
	}
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

float getPredictedFermenterTemp() {
	return predictedFermenterTemp();
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
	updateSetTempRamp();
	updateFermenterTrend();

	if (fabs(cascadeGain - lastConfiguredCascadeGain) >= 0.001f) {
		resetCascadeAdaptation();
	}
	if (fabs(setTemp - lastAdaptedSetTemp) >= 0.01f) {
		resetCascadeAdaptation();
	}

	// Latch real box temperature as initial baseline as soon as sensor reading becomes valid
	if (!cascadeBaselineInitialized && isBoxTempValid()) {
		cascadeBoxBaseline = boxTemp;
		cascadeBaselineInitialized = true;
	}

	float targetRaw;
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

		// Forecast liquid temperature to account for the container's thermal lag.
		float predictedError = setTemp - predictedFermenterTemp();

		// Use separate learned gains because heating and cooling capacity differ.
		// When heating, avoid using coolingCascadeGain (which is 2x higher for weak Peltier) on negative errors.
		bool isHeating = (boxMode == 3) || (boxMode == 1 && boxSubMode == 3);
		float gain = (isHeating || predictedError >= 0.0f) ? heatingCascadeGain : coolingCascadeGain;
		float offset = constrain(predictedError * gain, -cascadeMaxOffset, cascadeMaxOffset);

		// Box target is anchored at the thermal equilibrium baseline instead of hardcoded setTemp
		targetRaw = constrain(cascadeBoxBaseline + offset, setTemp - cascadeMaxOffset, setTemp + cascadeMaxOffset);
	} else {
		if (useISpindelTemp) {
			// iSpindel is configured but waiting for first report or temporarily offline:
			// anchor target to baseline/boxTemp so we don't blast heat/cool before receiving wort telemetry
			targetRaw = isBoxTempValid() ? cascadeBoxBaseline : setTemp;
		} else {
			// Direct box control mode: user explicitly regulates box air to setTemp
			targetRaw = setTemp;
			cascadeBoxBaseline = setTemp;
		}
	}

	unsigned long now = millis();
	if (!slewInitialized) {
		lastSlewMillis = now;
		slewInitialized = true;
		effectiveBoxTarget = targetRaw;
	} else {
		float dt = static_cast<float>(now - lastSlewMillis) / 1000.0f;
		lastSlewMillis = now;

		float effectiveDt = (dt > 10.0f) ? 10.0f : (dt > 0.0f ? dt : 0.0f);
		if (targetRaw > effectiveBoxTarget) {
			float maxStepUp = BOX_TARGET_MAX_SLEW_UP_PER_SEC * effectiveDt;
			effectiveBoxTarget = min(targetRaw, effectiveBoxTarget + maxStepUp);
		} else if (targetRaw < effectiveBoxTarget) {
			float maxStepDown = BOX_TARGET_MAX_SLEW_DOWN_PER_SEC * effectiveDt;
			effectiveBoxTarget = max(targetRaw, effectiveBoxTarget - maxStepDown);
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
		 * - Use the predicted fermenter temperature to select heating or cooling.
		 * - Anticipate both falling liquid temperature and stored-heat overshoot.
		 * - Dynamic fan stays ON until hot-side radiator drops to or below box temperature.
		 */
		bool isStopped = isCoolerStopped();
		bool emergencyCutoff = updateEmergencyHeatLockout();
		float controlTemp = getFermenterTemp();

		if (boxMode == 1 && !isStopped) {
			if (boxSubMode == 3) { // Currently in Heating sub-mode
				// Switch to cooling only after the measured liquid (or box air when no iSpindel) exceeds
				// target by the emergency margin.
				if (controlTemp > (setTemp + FERMENTER_EMERGENCY_OVERTEMP)) {
					boxSubMode = 2; // Switch to Cooling
					Debug.addLine(String(F("Fermenter temp (")) + String(controlTemp, 2) +
					              F("°C) > target + ") + String(FERMENTER_EMERGENCY_OVERTEMP, 2) +
					              F("°C: switch to Cooling"));
				}
			} else { // Currently in Cooling sub-mode (boxSubMode == 2)
				// Use a 0.1°C return band to avoid switching back and forth at the setpoint.
				if (controlTemp <= setTemp) {
					boxSubMode = 3; // Switch to Heating
					Debug.addLine(String(F("Fermenter returned to target: liquid=")) +
					              String(controlTemp, 2) + F("C, switch to Heating"));
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
		// In Heating-only mode (boxMode == 3) the cascade already manages the transition: when
		// the fermenter reaches setTemp the cascade pulls effectiveBoxTarget down and the PID
		// reduces output naturally. Applying the emergency hard-cutoff on top would abruptly
		// zero the heater the instant iSpindel crosses setTemp+0.1°C, which is exactly the
		// symptom reported. Reserve the hard cutoff for Auto mode (boxMode == 1) where it guards
		// the gap between the fermenter probe crossing and the sub-mode switch completing.
		bool fermenterNeedsHeating = !isISpindelActive() || (boxMode != 1) || !emergencyCutoff;
		bool fermenterNeedsCooling = !isISpindelActive() || (iSpindelTemperature > setTemp);
		bool isCooling = !isHeating && !isStopped && fermenterNeedsCooling;

		// Reset PID memory ONLY when resuming from stopped state or changing major operating mode
		static bool wasStopped = true;
		static int lastMajorMode = -1;

		bool enteredHeating = (wasStopped && !isStopped && isHeating) ||
		                      (lastMajorMode != boxMode && isHeating && !isStopped);
		if (enteredHeating) {
			resetHeatingPID();
			Debug.addLine(F("Switched to Heating: PID memory initialized"));
		}
		wasStopped = isStopped;
		lastMajorMode = boxMode;

		// Execute separated cooling logic (slave loop drives boxTemp to effectiveBoxTarget)
		processCooling(boxTemp, HotTemp, effectiveBoxTarget, isCooling);

		// Execute separated heating logic (strictly locked out while fermenter is at/above target)
		// Peltier interlock: running the heater simultaneously with an active Peltier would brownout
		// the shared power supply. The interlock uses setVoltage (the value commanded this cycle,
		// set by processCooling just above) as the primary gate. measuredVoltage is only an additional
		// guard when the Peltier IS commanded — it can lag by up to I2C_INTERVAL seconds and would
		// spuriously block heating in heating-only mode if the board reports a residual/hardware-floor
		// voltage while the Peltier is commanded off.
		float safePeltierThreshold = max(2.5f, minVoltage + 0.5f);
		bool peltierCommanded = (setVoltage >= minVoltage);
		bool peltierSafe = !I2C_PRESENT || !peltierCommanded || (measuredVoltage < safePeltierThreshold);
		bool heatingAllowed = !isStopped && fermenterNeedsHeating && peltierSafe && isHeating;
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
