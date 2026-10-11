#pragma once

#include <Arduino.h>

#define DOUBLERESETDETECTOR_FLAG_SET 0xD0D01234
#define DOUBLERESETDETECTOR_FLAG_CLEAR 0xD0D04321

class DoubleResetDetector {
public:
	DoubleResetDetector(unsigned long timeoutSeconds = 10, uint32_t rtcAddress = 0);

	bool detectDoubleReset();
	void notifySetupComplete();
	void loop();
	void stop();
	bool isDoubleResetDetected() const;

private:
	unsigned long timeoutMs;
	uint32_t rtcAddress;
	bool waitingForDoubleReset;
	bool doubleResetDetected;
	unsigned long resetStartTime;

	bool detectRecentlyResetFlag();
	void clearRecentlyResetFlag();
	void setRecentlyResetFlag();
};

