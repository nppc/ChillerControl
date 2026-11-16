#include "PidFastDebug.h"
#include "CoolerBox.h"

#ifdef ENABLE_PID_FAST_DEBUG

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

namespace {

// Static capture buffers - strictly bounded, no heap allocation
TelemetryRow sampleBuffer[PID_CAPTURE_MAX_ROWS];
uint8_t sampleCount = 0;

uint8_t txBuffer[PID_CAPTURE_MAX_ROWS * 20 + 4];
size_t txLength = 0;
uint8_t txRowCount = 0;
bool txReady = false;
bool pendingStopFlush = false;

// Session and status state
bool captureActive = false;
unsigned long captureStartMillis = 0;
String pidCaptureUrl = "";
uint32_t totalRowsSent = 0;
uint32_t totalPacketsSent = 0;
int lastHttpStatus = 0;
unsigned long lastPostDurationMs = 0;
unsigned long maxPostDurationMs = 0;

// Preserved diagnostic PID values (independent of applied PWM / forced-off zeroing)
float diagLastPTerm = 0.0f;
float diagLastITerm = 0.0f;
float diagLastDTerm = 0.0f;
float diagLastPidOutput = 0.0f;
float diagLastRequestedPWM = 0.0f;
bool diagPidFresh = false;

// Standard IEEE 802.3 / ISO-HDLC CRC-32 (polynomial 0xEDB88320)
uint32_t calculateCRC32(const uint8_t *data, size_t length) {
	uint32_t crc = 0xFFFFFFFF;
	for (size_t i = 0; i < length; i++) {
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xEDB88320UL;
			} else {
				crc = crc >> 1;
			}
		}
	}
	return crc ^ 0xFFFFFFFF;
}

inline void serializeU16LE(uint8_t *buf, size_t &idx, uint16_t val) {
	buf[idx++] = static_cast<uint8_t>(val & 0xFF);
	buf[idx++] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

inline void serializeI16LE(uint8_t *buf, size_t &idx, int16_t val) {
	uint16_t uval = static_cast<uint16_t>(val);
	buf[idx++] = static_cast<uint8_t>(uval & 0xFF);
	buf[idx++] = static_cast<uint8_t>((uval >> 8) & 0xFF);
}

inline void serializeU32LE(uint8_t *buf, size_t &idx, uint32_t val) {
	buf[idx++] = static_cast<uint8_t>(val & 0xFF);
	buf[idx++] = static_cast<uint8_t>((val >> 8) & 0xFF);
	buf[idx++] = static_cast<uint8_t>((val >> 16) & 0xFF);
	buf[idx++] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

inline int16_t scaleAndClampI16(float value, float scale) {
	if (isnan(value)) return 0;
	float scaled = value * scale;
	if (scaled >= 32767.0f) return 32767;
	if (scaled <= -32768.0f) return -32768;
	return static_cast<int16_t>(lroundf(scaled));
}

inline uint16_t scalePwmPercent(float pwm, float maxPwm) {
	if (isnan(pwm) || maxPwm <= 0.0f || pwm <= 0.0f) return 0;
	float pct = (pwm / maxPwm) * 10000.0f;
	if (pct >= 10000.0f) return 10000;
	return static_cast<uint16_t>(lroundf(pct));
}

void serializeRow(uint8_t *dest, size_t &idx, const TelemetryRow &row) {
	serializeU16LE(dest, idx, row.dt_sec);
	serializeI16LE(dest, idx, row.box_temp_c);
	serializeI16LE(dest, idx, row.box_target_c);
	serializeI16LE(dest, idx, row.pid_p_pct);
	serializeI16LE(dest, idx, row.pid_i_pct);
	serializeI16LE(dest, idx, row.pid_d_pct);
	serializeI16LE(dest, idx, row.pid_out_pct);
	serializeU16LE(dest, idx, row.req_pwm_pct);
	serializeU16LE(dest, idx, row.app_pwm_pct);
	dest[idx++] = row.state_flags;
	dest[idx++] = row.cutoff_reason;
}

void stageTxPacket() {
	if (sampleCount == 0 || txReady) {
		return;
	}
	uint8_t count = sampleCount;
	if (count > PID_CAPTURE_MAX_ROWS) {
		count = PID_CAPTURE_MAX_ROWS;
	}

	size_t offset = 0;
	for (uint8_t i = 0; i < count; i++) {
		serializeRow(txBuffer, offset, sampleBuffer[i]);
	}

	uint32_t crc = calculateCRC32(txBuffer, offset);
	serializeU32LE(txBuffer, offset, crc);

	// Gateway contract: row count between 1 and PID_CAPTURE_MAX_ROWS (15), body length == count * 20 + 4
	if (count == 0 || count > PID_CAPTURE_MAX_ROWS || offset != static_cast<size_t>(count * 20 + 4)) {
		sampleCount = 0;
		return;
	}

	txLength = offset;
	txRowCount = count;
	txReady = true;

	if (sampleCount > count) {
		uint8_t remaining = sampleCount - count;
		for (uint8_t i = 0; i < remaining; i++) {
			sampleBuffer[i] = sampleBuffer[count + i];
		}
		sampleCount = remaining;
	} else {
		sampleCount = 0;
	}
}

} // namespace

void pidFastDebugInit() {
	sampleCount = 0;
	txReady = false;
	txLength = 0;
	txRowCount = 0;
	captureActive = false;
	captureStartMillis = 0;
	totalRowsSent = 0;
	totalPacketsSent = 0;
	lastHttpStatus = 0;
	lastPostDurationMs = 0;
	maxPostDurationMs = 0;
	pendingStopFlush = false;
	diagLastPTerm = 0.0f;
	diagLastITerm = 0.0f;
	diagLastDTerm = 0.0f;
	diagLastPidOutput = 0.0f;
	diagLastRequestedPWM = 0.0f;
	diagPidFresh = false;
}

bool isPidCaptureActive() {
	return captureActive;
}

void startPidCapture() {
	if (captureActive) {
		stopPidCapture();
	}
	captureStartMillis = millis();
	sampleCount = 0;
	captureActive = true;
	totalRowsSent = 0;
	totalPacketsSent = 0;
	maxPostDurationMs = 0;
	lastPostDurationMs = 0;
	lastHttpStatus = 0;
	pendingStopFlush = false;
	Debug.addLine(LOG_PID, F("PID fast diagnostic capture started"));
}

void stopPidCapture() {
	if (!captureActive && sampleCount == 0) {
		return;
	}
	captureActive = false;
	if (sampleCount > 0) {
		if (!txReady) {
			stageTxPacket();
		} else {
			pendingStopFlush = true;
		}
	}
	Debug.addLine(LOG_PID, F("PID fast diagnostic capture stopped"));
}

const String &getPidCaptureUrl() {
	return pidCaptureUrl;
}

void setPidCaptureUrl(const String &url) {
	pidCaptureUrl = url;
	pidCaptureUrl.trim();
}

uint32_t getPidCaptureElapsedSec() {
	if (!captureActive) return 0;
	return (millis() - captureStartMillis) / 1000UL;
}

uint8_t getPidCaptureRowsPending() {
	return sampleCount;
}

uint32_t getPidCaptureRowsSent() {
	return totalRowsSent;
}

uint32_t getPidCapturePacketsSent() {
	return totalPacketsSent;
}

int getPidCaptureLastStatus() {
	return lastHttpStatus;
}

unsigned long getPidCaptureMaxBlockMs() {
	return maxPostDurationMs;
}

void pidFastDebugOnCompute(bool fresh, float pTerm, float iTerm, float dTerm, float pidOut, float requestedPwm) {
	if (fresh) {
		diagLastPTerm = pTerm;
		diagLastITerm = iTerm;
		diagLastDTerm = dTerm;
		diagLastPidOutput = pidOut;
		diagLastRequestedPWM = requestedPwm;
		diagPidFresh = true;
	} else {
		diagPidFresh = false;
	}
}

void pidFastDebugOnReset() {
	diagLastPTerm = 0.0f;
	diagLastITerm = 0.0f;
	diagLastDTerm = 0.0f;
	diagLastPidOutput = 0.0f;
	diagLastRequestedPWM = 0.0f;
	diagPidFresh = false;
}

void pidFastDebugRecordSample(float boxTemp, float effectiveBoxTarget,
                              bool heatingAllowed, bool coolerStopped,
                              bool heatingMode, bool peltierCommanded,
                              bool peltierSafe, uint8_t cutoffReason,
                              float appliedPwm, float maxPwm) {
	if (!captureActive) {
		return;
	}

	unsigned long now = millis();
	unsigned long elapsedMs = now - captureStartMillis;
	unsigned long elapsedSec = elapsedMs / 1000UL;

	// Stop cleanly before 16-bit seconds counter overflow (65535 s ~ 18.2 hours)
	if (elapsedSec >= 65535UL) {
		stopPidCapture();
		return;
	}

	TelemetryRow row;
	row.dt_sec = static_cast<uint16_t>(elapsedSec);
	row.box_temp_c = scaleAndClampI16(boxTemp, 100.0f);
	row.box_target_c = scaleAndClampI16(effectiveBoxTarget, 100.0f);
	row.pid_p_pct = scaleAndClampI16(diagLastPTerm, 10.0f);
	row.pid_i_pct = scaleAndClampI16(diagLastITerm, 10.0f);
	row.pid_d_pct = scaleAndClampI16(diagLastDTerm, 10.0f);
	row.pid_out_pct = scaleAndClampI16(diagLastPidOutput, 10.0f);
	row.req_pwm_pct = scalePwmPercent(diagLastRequestedPWM, maxPwm);
	row.app_pwm_pct = scalePwmPercent(appliedPwm, maxPwm);

	uint8_t flags = 0;
	if (heatingAllowed)   flags |= PID_FLAG_HEATING_ALLOWED;
	if (coolerStopped)    flags |= PID_FLAG_COOLER_STOPPED;
	if (heatingMode)      flags |= PID_FLAG_HEATING_MODE;
	if (peltierCommanded) flags |= PID_FLAG_PELTIER_COMMANDED;
	if (peltierSafe)      flags |= PID_FLAG_PELTIER_SAFE;
	if (diagPidFresh)     flags |= PID_FLAG_PID_FRESH;
	row.state_flags = flags;
	row.cutoff_reason = cutoffReason;

	if (sampleCount < PID_CAPTURE_MAX_ROWS) {
		sampleBuffer[sampleCount++] = row;
	}

	// Consume fresh flag for this control cycle
	diagPidFresh = false;

	// Staging trigger: when we reach batch size (15 rows) and transmit buffer is free
	if (sampleCount >= PID_CAPTURE_BATCH_ROWS && !txReady) {
		stageTxPacket();
	}
}

void pidFastDebugLoop() {
	if (!txReady) {
		if (pendingStopFlush && sampleCount > 0) {
			pendingStopFlush = false;
			stageTxPacket();
		}
		return;
	}

	if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
		lastHttpStatus = -1;
		txReady = false;
		txLength = 0;
		txRowCount = 0;
		Debug.addLine(LOG_PID, F("PID capture drop: Wi-Fi not connected"));
		return;
	}

	if (pidCaptureUrl.length() == 0 || !pidCaptureUrl.startsWith(F("http://"))) {
		lastHttpStatus = -2;
		txReady = false;
		txLength = 0;
		txRowCount = 0;
		Debug.addLine(LOG_PID, F("PID capture drop: invalid HTTP URL"));
		return;
	}

	WiFiClient client;
	client.setTimeout(1200);

	HTTPClient http;
	http.setTimeout(1200);
	http.setReuse(false);

	if (!http.begin(client, pidCaptureUrl)) {
		lastHttpStatus = -3;
		txReady = false;
		txLength = 0;
		txRowCount = 0;
		Debug.addLine(LOG_PID, F("PID capture drop: HTTP begin failed"));
		return;
	}

	http.addHeader(F("Content-Type"), F("application/octet-stream"));
	http.addHeader(F("Connection"), F("close"));

	unsigned long t0 = millis();
	yield();
	int httpCode = http.POST(txBuffer, txLength);
	yield();
	unsigned long duration = millis() - t0;
	http.end();

	lastPostDurationMs = duration;
	if (duration > maxPostDurationMs) {
		maxPostDurationMs = duration;
	}

	lastHttpStatus = httpCode;
	if (httpCode == 200 || httpCode == 204) {
		totalRowsSent += txRowCount;
		totalPacketsSent++;
	} else {
		// Plain HTTP batch sender does not retry on failure
		Debug.addLine(LOG_PID, String(F("PID capture POST failed: st=")) + httpCode +
		              F(" (") + duration + F("ms)"));
	}

	txReady = false;
	txLength = 0;
	txRowCount = 0;

	if (pendingStopFlush && sampleCount > 0) {
		pendingStopFlush = false;
		stageTxPacket();
	}
}

#endif
