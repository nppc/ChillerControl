#include "TelemetryQueue.h"
#include "CoolerBox.h"

#include <math.h>

namespace {
TelemetryBoxRecord boxRing[TELEMETRY_RAM_RECORDS];
uint8_t boxHead = 0;
uint8_t boxCount = 0;

TelemetryISpindelRecord iSpindelRing[ISPINDEL_RAM_RECORDS];
uint8_t iSpindelHead = 0;
uint8_t iSpindelCount = 0;

int8_t clampRssi(int value) {
	if (value < INT8_MIN + 1) {
		return INT8_MIN + 1;
	}
	if (value > INT8_MAX) {
		return INT8_MAX;
	}
	return static_cast<int8_t>(value);
}
}

int16_t telemetryFixedPoint(float value, int32_t scale) {
	if (!isfinite(value)) {
		return INT16_MIN;
	}

	double scaled = static_cast<double>(value) * scale;
	if (scaled <= INT16_MIN + 1) {
		return INT16_MIN + 1;
	}
	if (scaled >= INT16_MAX) {
		return INT16_MAX;
	}
	return static_cast<int16_t>(lround(scaled));
}

const char *telemetryStateToString(TelemetryState state) {
	switch (state) {
	case TelemetryState::Heating: return "heating";
	case TelemetryState::Cooling: return "cooling";
	case TelemetryState::Nothing:
	default:
		return "nothing";
	}
}

void captureTelemetrySnapshot() {
	TelemetryBoxRecord record = {};
	record.t = epochSeconds();
	record.box_temperature = telemetryFixedPoint(boxTemp, 100);
	record.box_target = telemetryFixedPoint(effectiveBoxTarget, 100);
	record.fermenter_temperature = telemetryFixedPoint(getFermenterTemp(), 100);
	record.set_temperature = telemetryFixedPoint(
		isHeatingAutoTuning() ? getHeatingAutoTuneTargetTemp() : setTemp, 100);
	record.hot_temperature = telemetryFixedPoint(HotTemp, 100);
	record.voltage = telemetryFixedPoint(measuredVoltage, 100);
	record.current = telemetryFixedPoint(measuredCurrent, 100);
	record.pwm = telemetryFixedPoint(
		(maxHotPWM > 0) ? (setHotPWM * 100.0f / static_cast<float>(maxHotPWM)) : 0.0f, 100);
	record.rssi = clampRssi(WiFi.RSSI());
	record.state = (setHotPWM > 0.0f) ? static_cast<uint8_t>(TelemetryState::Heating) :
		((!isCoolerStopped() && isCoolingDemanded()) ? static_cast<uint8_t>(TelemetryState::Cooling) :
		 static_cast<uint8_t>(TelemetryState::Nothing));

	uint8_t tail = static_cast<uint8_t>((boxHead + boxCount) % TELEMETRY_RAM_RECORDS);
	if (boxCount == TELEMETRY_RAM_RECORDS) {
		boxRing[boxHead] = record;
		boxHead = static_cast<uint8_t>((boxHead + 1) % TELEMETRY_RAM_RECORDS);
	} else {
		boxRing[tail] = record;
		++boxCount;
	}
}

void telemetryISpindelPush(const TelemetryISpindelRecord &record) {
	uint8_t tail = static_cast<uint8_t>((iSpindelHead + iSpindelCount) % ISPINDEL_RAM_RECORDS);
	if (iSpindelCount == ISPINDEL_RAM_RECORDS) {
		iSpindelRing[iSpindelHead] = record;
		iSpindelHead = static_cast<uint8_t>((iSpindelHead + 1) % ISPINDEL_RAM_RECORDS);
	} else {
		iSpindelRing[tail] = record;
		++iSpindelCount;
	}
}

uint8_t telemetryQueuePending() {
	return static_cast<uint8_t>(boxCount + iSpindelCount);
}

uint8_t telemetryBoxPending() {
	return boxCount;
}

uint8_t telemetryISpindelPending() {
	return iSpindelCount;
}

const TelemetryBoxRecord &telemetryBoxPeek(uint8_t index) {
	return boxRing[(boxHead + index) % TELEMETRY_RAM_RECORDS];
}

const TelemetryISpindelRecord &telemetryISpindelPeek(uint8_t index) {
	return iSpindelRing[(iSpindelHead + index) % ISPINDEL_RAM_RECORDS];
}

void telemetryBoxPop(uint8_t count) {
	if (count > boxCount) {
		count = boxCount;
	}
	boxHead = static_cast<uint8_t>((boxHead + count) % TELEMETRY_RAM_RECORDS);
	boxCount -= count;
}

void telemetryISpindelPop(uint8_t count) {
	if (count > iSpindelCount) {
		count = iSpindelCount;
	}
	iSpindelHead = static_cast<uint8_t>((iSpindelHead + count) % ISPINDEL_RAM_RECORDS);
	iSpindelCount -= count;
}
