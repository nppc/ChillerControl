#include "TemperatureControl.h"
#include "CoolerBox.h"

float cascadeMaxOffset = 4.0f;  // Maximum offset (°C) box air is allowed to depart from setTemp
float cascadeGain = 1.5f;       // Multiplier for liquid error
float effectiveBoxTarget = 19.0f;
int iSpindelIntervalSec = 900;  // 15 minutes default

void initTemperatureControl() {
	effectiveBoxTarget = setTemp;
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
	return isISpindelActive() ? iSpindelTemperature : boxTemp;
}

float getEffectiveBoxTarget() {
	return effectiveBoxTarget;
}

void updateCascadeTarget() {
	if (isISpindelActive()) {
		// Master cascade loop: BoxTarget = Setpoint + Gain * (Setpoint - LiquidTemp)
		float error = setTemp - iSpindelTemperature;
		float offset = constrain(error * cascadeGain, -cascadeMaxOffset, cascadeMaxOffset);
		effectiveBoxTarget = setTemp + offset;
	} else {
		effectiveBoxTarget = setTemp;
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
		 * Tendency-Based Dual Mode Switchover (5-Minute Window)
		 * ======================================================
		 * Evaluates the slave loop (box air temperature) against effectiveBoxTarget.
		 */
		static unsigned long heatingIdleStartTime = 0;
		static float heatingPeakTemp = -999.0f;

		static unsigned long coolingZeroStartTime = 0;
		static float coolingTroughTemp = 999.0f;

		static float lastTargetTemp = -999.0f;
		if (lastTargetTemp < -900.0f) {
			lastTargetTemp = effectiveBoxTarget;
		}

		// If dynamic box target changed significantly (>0.2°C), decide sub-mode immediately
		if (fabs(effectiveBoxTarget - lastTargetTemp) >= 0.2f) {
			if (boxMode == 1) {
				if (boxTemp < effectiveBoxTarget) {
					boxSubMode = 3; // Switch to Heating
					Debug.addLine(String(F("Target changed to ")) + String(effectiveBoxTarget, 1) +
					              F("°C: immediate switch to Heating (boxTemp=") + String(boxTemp, 2) + F("°C)"));
				} else if (boxTemp > effectiveBoxTarget) {
					boxSubMode = 2; // Switch to Cooling
					Debug.addLine(String(F("Target changed to ")) + String(effectiveBoxTarget, 1) +
					              F("°C: immediate switch to Cooling (boxTemp=") + String(boxTemp, 2) + F("°C)"));
				}
			}
			heatingIdleStartTime = 0;
			heatingPeakTemp = -999.0f;
			coolingZeroStartTime = 0;
			coolingTroughTemp = 999.0f;
			coolingZeroVoltage = false;
			lastTargetTemp = effectiveBoxTarget;
		}

		bool isStopped = isCoolerStopped();

		if (boxMode == 1 && !isStopped) {
			if (boxSubMode == 2) { // Currently in Cooling sub-mode
				heatingIdleStartTime = 0;
				heatingPeakTemp = -999.0f;

				// In cooling mode, demand turns off at effectiveBoxTarget - DEFAULT_HYSTERESIS
				if (boxTemp >= (effectiveBoxTarget - DEFAULT_HYSTERESIS)) {
					coolingZeroVoltage = false;
					coolingZeroStartTime = 0;
					coolingTroughTemp = 999.0f;
				} else {
					// Stage 1: Cut Peltier voltage to 0.0V to eliminate idle bias
					coolingZeroVoltage = true;

					if (coolingZeroStartTime == 0) {
						coolingZeroStartTime = millis();
						coolingTroughTemp = boxTemp;
						Debug.addLine(String(F("Cooling idle: temp below target (")) + String(boxTemp, 2) +
						              F("°C). Cutting Peltier to 0.0V. Observing tendency (5m)..."));
					} else {
						// Track minimum temperature reached
						if (boxTemp < coolingTroughTemp) {
							coolingTroughTemp = boxTemp;
						}

						// Stage 2: Check tendency over time
						if (boxTemp >= (coolingTroughTemp + 0.05f)) {
							coolingTroughTemp = boxTemp;
							coolingZeroStartTime = millis();
						} else if (millis() - coolingZeroStartTime >= (MODE_SWITCH_WINDOW_SEC * 1000UL)) {
							if (boxTemp < effectiveBoxTarget) {
								boxSubMode = 3; // Switch to Heating mode
								coolingZeroVoltage = false;
								coolingZeroStartTime = 0;
								coolingTroughTemp = 999.0f;
								Debug.addLine(String(F("Switching Cooling -> Heating: 5m at 0.0V, temp still dropping (")) +
								              String(boxTemp, 2) + F("°C)"));
							}
						}
					}
				}
			} else { // Currently in Heating sub-mode (boxSubMode == 3)
				coolingZeroVoltage = false;
				coolingZeroStartTime = 0;
				coolingTroughTemp = 999.0f;

				// Heater is idle when PID output is 0 and temperature is above target
				bool heatingIdle = (setHotPWM <= 0.0f) && (boxTemp > effectiveBoxTarget);

				if (!heatingIdle) {
					heatingIdleStartTime = 0;
					heatingPeakTemp = -999.0f;
				} else {
					if (heatingIdleStartTime == 0) {
						heatingIdleStartTime = millis();
						heatingPeakTemp = boxTemp;
						Debug.addLine(String(F("Heating idle: heater at 0%, temp above target (")) + String(boxTemp, 2) +
						              F("°C). Observing tendency (5m)..."));
					} else {
						if (boxTemp > heatingPeakTemp) {
							heatingPeakTemp = boxTemp;
						}

						// If temperature has started cooling down (-0.05°C), renew window
						if (boxTemp <= (heatingPeakTemp - 0.05f)) {
							heatingPeakTemp = boxTemp;
							heatingIdleStartTime = millis();
						} else if (millis() - heatingIdleStartTime >= (MODE_SWITCH_WINDOW_SEC * 1000UL)) {
							if (boxTemp > effectiveBoxTarget) {
								boxSubMode = 2; // Switch to Cooling mode
								heatingIdleStartTime = 0;
								heatingPeakTemp = -999.0f;
								Debug.addLine(String(F("Switching Heating -> Cooling: 5m at 0% PWM, temp still rising (")) +
								              String(boxTemp, 2) + F("°C)"));
							}
						}
					}
				}
			}
		} else {
			heatingIdleStartTime = 0;
			heatingPeakTemp = -999.0f;
			coolingZeroStartTime = 0;
			coolingTroughTemp = 999.0f;
			if (boxMode == 2 && !isStopped) {
				coolingZeroVoltage = (boxTemp < (effectiveBoxTarget - DEFAULT_HYSTERESIS));
			} else {
				coolingZeroVoltage = false;
			}
		}

		bool isHeating = (boxMode == 3) || (boxMode == 1 && boxSubMode == 3);
		bool isCooling = !isHeating && !isStopped;

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

		// Execute separated heating logic
		float safePeltierThreshold = max(2.5f, minVoltage + 0.5f);
		bool heatingAllowed = !isStopped && (measuredVoltage < safePeltierThreshold) && ((boxMode == 3) || (boxMode == 1 && boxSubMode == 3));
		processHeating(boxTemp, effectiveBoxTarget, isHeating && heatingAllowed);

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

		// Control Fan
		if (isHeating) {
			digitalWrite(HOT_FAN, LOW); // Fan is always OFF when heating
		} else if (HotSensorId == ColdSensorId || fanDynamic_checked == 0) {
			digitalWrite(HOT_FAN, HIGH);
		} else {
			if (HotTemp >= 32.0f && digitalRead(HOT_FAN) == LOW) {
				digitalWrite(HOT_FAN, HIGH);
			} else if (HotTemp <= 24.0f && digitalRead(HOT_FAN) == HIGH) {
				digitalWrite(HOT_FAN, LOW);
			}
		}
	}

	// Send data to IoT
	if (!emergencyMode && (millis() - millisSendDataInterval > static_cast<unsigned long>(sendInterval) * 1000UL)) {
		millisSendDataInterval = millis();
		sendTelemetry();
	}
}

