#include "HeatingControl.h"
#include "CoolerBox.h"

float hotPID_kP = DEFAULT_PID_KP;
float hotPID_kI = DEFAULT_PID_KI;
float hotPID_kD = DEFAULT_PID_KD;
float setHotPWM = 0.0f;
int maxHotPWM = MAXIMUM_PWM_HOT_WIRE;

static float pidInput = 0.0f;
static float pidOutput = 0.0f;
static float pidSetpoint = 0.0f;

QuickPID hotPID(&pidInput, &pidOutput, &pidSetpoint,
                hotPID_kP, hotPID_kI, hotPID_kD,
                QuickPID::pMode::pOnError,
                QuickPID::dMode::dOnMeas,
                QuickPID::iAwMode::iAwCondition,
                QuickPID::Action::direct);

// Autotune state
static AutoTuneState atState = AutoTuneState::IDLE;
static float atTargetTemp = 20.0f;
static float atTestPWM = 300.0f;
static float atNoiseBand = 0.25f;
static int atTargetCycles = 4;
static int atCycleCount = 0;
static bool atHeaterOn = false;
static float atPeak = -999.0f;
static float atValley = 999.0f;
static unsigned long atStartTime = 0;
static unsigned long atCycleStartTime = 0;
static float atPeriods[8];
static float atAmplitudes[8];
static float atCalcKp = 0.0f;
static float atCalcKi = 0.0f;
static float atCalcKd = 0.0f;
static String atStatusMessage = "Idle";
static float currentRampedPWM = 0.0f;

static void applyHeaterPWM(float targetPWM, bool ramp = true) {
	if (!ramp || targetPWM <= 0.0f) {
		currentRampedPWM = targetPWM;
		setHotPWM = targetPWM;
		analogWrite(HEAT_WIRE, (int)round(targetPWM));
		return;
	}

	// Soft-start ramp: limit PWM step per cycle to prevent current shock and voltage brownout
	const float MAX_STEP_PER_CYCLE = 100.0f;
	if (currentRampedPWM < targetPWM) {
		currentRampedPWM += MAX_STEP_PER_CYCLE;
		if (currentRampedPWM > targetPWM) {
			currentRampedPWM = targetPWM;
		}
	} else if (currentRampedPWM > targetPWM) {
		currentRampedPWM -= MAX_STEP_PER_CYCLE;
		if (currentRampedPWM < targetPWM) {
			currentRampedPWM = targetPWM;
		}
	}
	setHotPWM = currentRampedPWM;
	analogWrite(HEAT_WIRE, (int)round(currentRampedPWM));
}

void initHeating() {
	pinMode(HEAT_WIRE, OUTPUT);
	analogWriteFreq(100); // 100Hz PWM for heating wire
	analogWrite(HEAT_WIRE, 0);
	setHotPWM = 0.0f;
	currentRampedPWM = 0.0f;

	hotPID.SetOutputLimits(0, maxHotPWM);
	hotPID.SetSampleTimeUs(PIDCOMPUTE_INTERVAL * 1000000UL); // 2000000 µs
	hotPID.SetTunings(hotPID_kP, hotPID_kI, hotPID_kD,
	                 QuickPID::pMode::pOnError,
	                 QuickPID::dMode::dOnMeas,
	                 QuickPID::iAwMode::iAwCondition);
	hotPID.SetControllerDirection(QuickPID::Action::direct);
	hotPID.SetMode(QuickPID::Control::automatic);

	if (setTemp > 0.0f && setTemp < 45.0f) {
		atTargetTemp = setTemp;
	}
}

void resetHeatingPID() {
	hotPID.Reset(); // Completely clears pTerm, iTerm, dTerm, and outputSum
	currentRampedPWM = 0.0f;
	setHotPWM = 0.0f;
	pidOutput = 0.0f;
	analogWrite(HEAT_WIRE, 0);
}

void pauseHeatingPWM() {
	analogWrite(HEAT_WIRE, 0);
}

void resumeHeatingPWM() {
	if (setHotPWM > 0.0f) {
		analogWrite(HEAT_WIRE, (int)round(setHotPWM));
	}
}

void setHeatingTunings(float kp, float ki, float kd) {
	if (kp < 0.0f || ki < 0.0f || kd < 0.0f) {
		return;
	}
	hotPID_kP = kp;
	hotPID_kI = ki;
	hotPID_kD = kd;
	hotPID.SetTunings(hotPID_kP, hotPID_kI, hotPID_kD);
}

void getHeatingTunings(float &kp, float &ki, float &kd) {
	kp = hotPID_kP;
	ki = hotPID_kI;
	kd = hotPID_kD;
}

static void stepAutoTune(float currentTemp) {
	// Safety checks
	if (currentTemp >= 45.0f || HotTemp >= RADIATOR_EXTREME_TEMP) {
		stopHeatingAutoTune("Safety threshold exceeded (>45°C)");
		return;
	}
	if (millis() - atStartTime > (120UL * 60UL * 1000UL)) {
		stopHeatingAutoTune("Timed out after 120 minutes");
		return;
	}

	if (atState == AutoTuneState::WARMING_UP) {
		applyHeaterPWM(atTestPWM, true);

		if (currentTemp >= atTargetTemp) {
			atState = AutoTuneState::CYCLING;
			atHeaterOn = false;
			applyHeaterPWM(0.0f, false);
			atCycleCount = 0;
			atPeak = currentTemp;
			atValley = currentTemp;
			atCycleStartTime = millis();
			atStatusMessage = F("Warmed up. Starting limit cycles...");
			Debug.addLine(F("Autotune: warmed to target, starting limit cycles"));
		}
		return;
	}

	if (atState == AutoTuneState::CYCLING) {
		if (currentTemp > atPeak) atPeak = currentTemp;
		if (currentTemp < atValley) atValley = currentTemp;

		if (atHeaterOn) {
			applyHeaterPWM(atTestPWM, true);
			if (currentTemp >= (atTargetTemp + atNoiseBand)) {
				atHeaterOn = false;
				applyHeaterPWM(0.0f, false);
				atStatusMessage = "Cycle " + String(atCycleCount + 1) + ": cooling to valley";
			}
		} else {
			applyHeaterPWM(0.0f, false);
			if (currentTemp <= (atTargetTemp - atNoiseBand)) {
				unsigned long now = millis();
				unsigned long cycleDurationMs = now - atCycleStartTime;
				atCycleStartTime = now;
				float amplitude = atPeak - atValley;

				if (atCycleCount < 8) {
					atPeriods[atCycleCount] = (float)cycleDurationMs / 1000.0f;
					atAmplitudes[atCycleCount] = amplitude;
				}
				atCycleCount++;

				String cycleLog = "Autotune Cycle " + String(atCycleCount) + "/" + String(atTargetCycles) +
				                  ": Period=" + String((float)cycleDurationMs / 1000.0f, 1) + "s, Amp=" +
				                  String(amplitude, 2) + "C (Peak=" + String(atPeak, 2) + ", Valley=" + String(atValley, 2) + ")";
				Debug.addLine(cycleLog);

				atPeak = currentTemp;
				atValley = currentTemp;

				if (atCycleCount >= atTargetCycles) {
					// Discard first cycle if we ran >= 3 cycles to ignore initial warm-up transient
					int startIdx = (atCycleCount >= 3) ? 1 : 0;
					int counted = atCycleCount - startIdx;
					float sumPeriod = 0.0f;
					float sumAmp = 0.0f;
					for (int i = startIdx; i < atCycleCount; i++) {
						sumPeriod += atPeriods[i];
						sumAmp += atAmplitudes[i];
					}

					float avgPeriodSec = sumPeriod / (float)counted;
					float avgAmp = sumAmp / (float)counted;
					if (avgAmp < 0.05f) avgAmp = 0.05f;

					// Describing function: Ku = 4 * d / (pi * a)
					// Relay step is 0 to atTestPWM, so d = atTestPWM / 2
					// Peak-to-peak amplitude is avgAmp, so a = avgAmp / 2
					// Ku = (4 * (atTestPWM / 2)) / (pi * (avgAmp / 2)) = (4 * atTestPWM) / (pi * avgAmp)
					float Ku = (4.0f * atTestPWM) / (3.14159265f * avgAmp);
					float Pu = avgPeriodSec;
					if (Pu < 1.0f) Pu = 1.0f;

					// Tyreus-Luyben tuning rules
					atCalcKp = 0.45f * Ku;
					atCalcKi = (0.205f * Ku) / Pu;
					atCalcKd = 0.071f * Ku * Pu;
					if (atCalcKd > 100.0f) atCalcKd = 100.0f; // Cap Kd to prevent digital sensor bit-flip chatter
					if (atCalcKd < 0.0f) atCalcKd = 0.0f;

					setHeatingTunings(atCalcKp, atCalcKi, atCalcKd);
					saveSettings();

					atState = AutoTuneState::COMPLETED;
					applyHeaterPWM(0.0f, false);
					resetHeatingPID();
					setCoolerStopped(true); // Maintain safe stopped state after tuning

					String resMsg = "Autotune complete: Kp=" + String(atCalcKp, 2) +
					                " Ki=" + String(atCalcKi, 4) +
					                " Kd=" + String(atCalcKd, 2) +
					                " (Ku=" + String(Ku, 1) + ", Pu=" + String(Pu, 1) + "s)";
					atStatusMessage = resMsg;
					Debug.addLine(resMsg);
					return;
				}

				atHeaterOn = true;
				applyHeaterPWM(atTestPWM, true);
				atStatusMessage = "Cycle " + String(atCycleCount + 1) + " of " + String(atTargetCycles) + ": heating to peak";
			}
		}
	}
}

void processHeating(float currentTemp, float targetTemp, bool enableHeating) {
	if (isHeatingAutoTuning()) {
		if (!enableHeating) {
			applyHeaterPWM(0.0f, false);
			float safePeltierThreshold = max(2.5f, minVoltage + 0.5f);
			atStatusMessage = "Waiting for Peltier voltage to drop (" + String(measuredVoltage, 1) + "V >= " + String(safePeltierThreshold, 1) + "V)...";
			return;
		}
		stepAutoTune(currentTemp);
		return;
	}

	if (enableHeating) {
		pidInput = currentTemp;
		pidSetpoint = targetTemp;
		hotPID.Compute();
		setHotPWM = pidOutput;
		currentRampedPWM = pidOutput;
		analogWrite(HEAT_WIRE, (int)round(setHotPWM));
	} else {
		currentRampedPWM = 0.0f;
		setHotPWM = 0.0f;
		pidOutput = 0.0f;
		analogWrite(HEAT_WIRE, 0);
	}
}

bool startHeatingAutoTune(float targetTemp, float testPWM, float noiseBand, int cycles) {
	if (isHeatingAutoTuning()) {
		return false;
	}
	if (cycles < 2) cycles = 2;
	if (cycles > 8) cycles = 8;
	if (testPWM < 50.0f) testPWM = 50.0f;
	if (testPWM > (float)maxHotPWM) testPWM = (float)maxHotPWM;
	if (noiseBand < 0.05f) noiseBand = 0.05f;

	// Place system in stopped state so normal control does not run
	setCoolerStopped(true);

	atTargetTemp = targetTemp;
	atTestPWM = testPWM;
	atNoiseBand = noiseBand;
	atTargetCycles = cycles;
	atCycleCount = 0;
	atStartTime = millis();
	atCycleStartTime = millis();
	atPeak = -999.0f;
	atValley = 999.0f;
	atHeaterOn = true;
	currentRampedPWM = 0.0f;
	atState = AutoTuneState::WARMING_UP;
	atStatusMessage = F("Autotune started: warming up to target...");

	Debug.addLine(String(F("Autotune started: target=")) + String(targetTemp, 1) +
	              F("C, PWM=") + String(testPWM, 0) +
	              F(", noise=±") + String(noiseBand, 2) + F("C"));
	return true;
}

void stopHeatingAutoTune(const char *reason) {
	if (!isHeatingAutoTuning()) {
		return;
	}
	atState = AutoTuneState::FAILED;
	applyHeaterPWM(0.0f, false);
	resetHeatingPID();
	setCoolerStopped(true);
	atStatusMessage = String(F("Autotune stopped: ")) + reason;
	Debug.addLine(atStatusMessage);
}

bool isHeatingAutoTuning() {
	return (atState == AutoTuneState::WARMING_UP || atState == AutoTuneState::CYCLING);
}

AutoTuneStatus getHeatingAutoTuneStatus() {
	AutoTuneStatus st;
	st.state = atState;
	st.currentCycle = atCycleCount;
	st.targetCycles = atTargetCycles;
	st.targetTemp = atTargetTemp;
	st.testPWM = atTestPWM;
	st.currentTemp = boxTemp;
	st.peakTemp = (atPeak > -900.0f) ? atPeak : boxTemp;
	st.valleyTemp = (atValley < 900.0f) ? atValley : boxTemp;
	st.calculatedKp = atCalcKp;
	st.calculatedKi = atCalcKi;
	st.calculatedKd = atCalcKd;
	st.message = atStatusMessage;
	return st;
}

float getHeatingAutoTuneTargetTemp() {
	if (atState == AutoTuneState::IDLE && atTargetTemp == 20.0f && setTemp > 0.0f) {
		return setTemp;
	}
	return atTargetTemp;
}

void setHeatingAutoTuneTargetTemp(float target) {
	if (target >= 5.0f && target <= 45.0f) {
		atTargetTemp = target;
	}
}

float getHeatingAutoTuneTestPWM() {
	return atTestPWM;
}

void setHeatingAutoTuneTestPWM(float pwm) {
	if (pwm >= 50.0f && pwm <= (float)maxHotPWM) {
		atTestPWM = pwm;
	}
}

