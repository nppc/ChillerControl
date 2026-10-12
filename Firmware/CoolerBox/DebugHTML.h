#pragma once
#include <Arduino.h>

#define DEBUG_MAX_RECORDS 20
#define DEBUG_LINE_MAX_LEN 128

class DebugHTML {
private:
	char debugStr[DEBUG_MAX_RECORDS][DEBUG_LINE_MAX_LEN];
	int curRecord = 0;

public:
	void init() {
		curRecord = 0;
		for (int i = 0; i < DEBUG_MAX_RECORDS; i++) {
			debugStr[i][0] = '\0';
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
		unsigned long ms = millis();
		unsigned long sec = ms / 1000;
		unsigned long min = sec / 60;
		unsigned long hr = min / 60;
		unsigned long days = hr / 24;

		if (days > 0) {
			snprintf(debugStr[curRecord], DEBUG_LINE_MAX_LEN, "[%lud %02lu:%02lu:%02lu] %s",
			         days, hr % 24, min % 60, sec % 60, strLine);
		} else {
			snprintf(debugStr[curRecord], DEBUG_LINE_MAX_LEN, "[%02lu:%02lu:%02lu] %s",
			         hr, min % 60, sec % 60, strLine);
		}
		curRecord = (curRecord + 1) % DEBUG_MAX_RECORDS;
	}

	String getHTML() const {
		String out;
		out.reserve(DEBUG_MAX_RECORDS * (DEBUG_LINE_MAX_LEN + 4));
		for (int i = 0; i < DEBUG_MAX_RECORDS; i++) {
			int idx = (curRecord + i) % DEBUG_MAX_RECORDS;
			if (debugStr[idx][0] != '\0') {
				out += debugStr[idx];
				out += F("<BR>");
			}
		}
		return out;
	}
};
