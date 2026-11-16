#pragma once

#include <Arduino.h>
#include "Globals.h"

#ifdef ENABLE_PID_FAST_DEBUG

enum PidCutoffReason : uint8_t {
	PID_CUTOFF_NONE              = 0,
	PID_CUTOFF_COOLER_STOPPED    = 1,
	PID_CUTOFF_NOT_HEATING_MODE  = 2,
	PID_CUTOFF_PELTIER_INTERLOCK = 3,
	PID_CUTOFF_FERMENTER_LOCKOUT = 4,
	PID_CUTOFF_OTHER             = 5
};

enum PidStateFlag : uint8_t {
	PID_FLAG_HEATING_ALLOWED   = 1 << 0,
	PID_FLAG_COOLER_STOPPED    = 1 << 1,
	PID_FLAG_HEATING_MODE      = 1 << 2,
	PID_FLAG_PELTIER_COMMANDED = 1 << 3,
	PID_FLAG_PELTIER_SAFE      = 1 << 4,
	PID_FLAG_PID_FRESH         = 1 << 5
};

// Maximum rows per HTTP packet and batch size trigger (strictly bounded to 15 rows)
const uint8_t PID_CAPTURE_BATCH_ROWS = 15;
const uint8_t PID_CAPTURE_MAX_ROWS = 15;

struct TelemetryRow {
	uint16_t dt_sec;
	int16_t box_temp_c;
	int16_t box_target_c;
	int16_t pid_p_pct;
	int16_t pid_i_pct;
	int16_t pid_d_pct;
	int16_t pid_out_pct;
	uint16_t req_pwm_pct;
	uint16_t app_pwm_pct;
	uint8_t state_flags;
	uint8_t cutoff_reason;
};

void pidFastDebugInit();
bool isPidCaptureActive();
void startPidCapture();
void stopPidCapture();

const String &getPidCaptureUrl();
void setPidCaptureUrl(const String &url);

uint32_t getPidCaptureElapsedSec();
uint8_t getPidCaptureRowsPending();
uint32_t getPidCaptureRowsSent();
uint32_t getPidCapturePacketsSent();
int getPidCaptureLastStatus();
unsigned long getPidCaptureMaxBlockMs();

void pidFastDebugOnCompute(bool fresh, float pTerm, float iTerm, float dTerm, float pidOut, float requestedPwm);
void pidFastDebugOnReset();

void pidFastDebugRecordSample(float boxTemp, float effectiveBoxTarget,
                              bool heatingAllowed, bool coolerStopped,
                              bool heatingMode, bool peltierCommanded,
                              bool peltierSafe, uint8_t cutoffReason,
                              float appliedPwm, float maxPwm);

void pidFastDebugLoop();

#else

// Inline no-op stubs when fast debug is disabled at compile time
inline void pidFastDebugInit() {}
inline bool isPidCaptureActive() { return false; }
inline void startPidCapture() {}
inline void stopPidCapture() {}

inline const String &getPidCaptureUrl() { static const String empty; return empty; }
inline void setPidCaptureUrl(const String &) {}

inline uint32_t getPidCaptureElapsedSec() { return 0; }
inline uint8_t getPidCaptureRowsPending() { return 0; }
inline uint32_t getPidCaptureRowsSent() { return 0; }
inline uint32_t getPidCapturePacketsSent() { return 0; }
inline int getPidCaptureLastStatus() { return 0; }
inline unsigned long getPidCaptureMaxBlockMs() { return 0; }

inline void pidFastDebugOnCompute(bool, float, float, float, float, float) {}
inline void pidFastDebugOnReset() {}

inline void pidFastDebugRecordSample(float, float, bool, bool, bool, bool, bool, uint8_t, float, float) {}

inline void pidFastDebugLoop() {}

#endif
