#pragma once
#include <Arduino.h>
#include <time.h>

#define DEBUG_MAX_RECORDS 20
#define DEBUG_LINE_MAX_LEN 128

class DebugHTML {
private:
	char debugStr[DEBUG_MAX_RECORDS][DEBUG_LINE_MAX_LEN];
	uint32_t recordMillis[DEBUG_MAX_RECORDS] = {};
	int curRecord = 0;

public:
	void init() {
		curRecord = 0;
		for (int i = 0; i < DEBUG_MAX_RECORDS; i++) {
			debugStr[i][0] = '\0';
			recordMillis[i] = 0;
		}
	}

	void addLine(const String &strLine) {
		addLine(strLine.c_str());
	}

	void addLine(const __FlashStringHelper *ifsh) {
		addLine(String(ifsh).c_str());
	}

	void addLine(const char *strLine) {
		if (!strLine) return;
		strlcpy(debugStr[curRecord], strLine, DEBUG_LINE_MAX_LEN);
		recordMillis[curRecord] = static_cast<uint32_t>(millis());
		curRecord = (curRecord + 1) % DEBUG_MAX_RECORDS;
	}

	String getHTML() const {
		String out;
		out.reserve(DEBUG_MAX_RECORDS * (DEBUG_LINE_MAX_LEN + 24 + 4));
		time_t now = time(nullptr);
		bool clockValid = now >= 1700000000;
		for (int i = 0; i < DEBUG_MAX_RECORDS; i++) {
			int idx = (curRecord + i) % DEBUG_MAX_RECORDS;
			if (debugStr[idx][0] != '\0') {
				uint32_t elapsedMillis = static_cast<uint32_t>(millis()) - recordMillis[idx];
				if (clockValid) {
					time_t recordTime = now - elapsedMillis / 1000U;
					struct tm localTime;
					if (localtime_r(&recordTime, &localTime)) {
						char timestamp[24];
						if (strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", &localTime) > 0) {
							out += timestamp;
						}
					}
				} else {
					unsigned long seconds = elapsedMillis / 1000U;
					unsigned long days = seconds / 86400;
					unsigned long hours = (seconds / 3600) % 24;
					unsigned long minutes = (seconds / 60) % 60;
					seconds %= 60;
					char timestamp[32];
					if (days > 0) {
						snprintf(timestamp, sizeof(timestamp), "[%lud %02lu:%02lu:%02lu] ",
						         days, hours, minutes, seconds);
					} else {
						snprintf(timestamp, sizeof(timestamp), "[%02lu:%02lu:%02lu] ",
						         hours, minutes, seconds);
					}
					out += timestamp;
				}
				out += debugStr[idx];
				out += F("<BR>");
			}
		}
		return out;
	}
};
