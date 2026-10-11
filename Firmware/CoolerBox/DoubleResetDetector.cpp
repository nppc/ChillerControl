#include "DoubleResetDetector.h"

DoubleResetDetector::DoubleResetDetector(unsigned long timeoutSeconds, uint32_t rtcAddress)
	: timeoutMs(timeoutSeconds * 1000UL),
	  rtcAddress(rtcAddress),
	  waitingForDoubleReset(false),
	  doubleResetDetected(false),
	  resetStartTime(0) {
}

bool DoubleResetDetector::detectDoubleReset() {
	doubleResetDetected = detectRecentlyResetFlag();
	if (doubleResetDetected) {
		clearRecentlyResetFlag();
		waitingForDoubleReset = false;
	} else {
		setRecentlyResetFlag();
		waitingForDoubleReset = true;
		resetStartTime = millis();
	}
	return doubleResetDetected;
}

void DoubleResetDetector::notifySetupComplete() {
	if (waitingForDoubleReset) {
		resetStartTime = millis();
	}
}

void DoubleResetDetector::loop() {
	if (waitingForDoubleReset && (millis() - resetStartTime > timeoutMs)) {
		stop();
	}
}

void DoubleResetDetector::stop() {
	clearRecentlyResetFlag();
	waitingForDoubleReset = false;
}

bool DoubleResetDetector::isDoubleResetDetected() const {
	return doubleResetDetected;
}

bool DoubleResetDetector::detectRecentlyResetFlag() {
	uint32_t flag = DOUBLERESETDETECTOR_FLAG_CLEAR;
	if (ESP.rtcUserMemoryRead(rtcAddress, &flag, sizeof(flag))) {
		return (flag == DOUBLERESETDETECTOR_FLAG_SET);
	}
	return false;
}

void DoubleResetDetector::setRecentlyResetFlag() {
	uint32_t flag = DOUBLERESETDETECTOR_FLAG_SET;
	ESP.rtcUserMemoryWrite(rtcAddress, &flag, sizeof(flag));
}

void DoubleResetDetector::clearRecentlyResetFlag() {
	uint32_t flag = DOUBLERESETDETECTOR_FLAG_CLEAR;
	ESP.rtcUserMemoryWrite(rtcAddress, &flag, sizeof(flag));
}

