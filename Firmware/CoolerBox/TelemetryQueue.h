#pragma once

#include <Arduino.h>
#include <limits.h>

const size_t TELEMETRY_RAM_RECORDS = 48;
const size_t ISPINDEL_RAM_RECORDS = 10;
const size_t TELEMETRY_CHUNK_MAX = 4;
const size_t TELEMETRY_MAX_PAYLOAD = 2048;
const unsigned long TELEMETRY_CHUNK_GAP_MS = 2000UL;
const size_t TELEMETRY_MIN_FREE_BLOCK = 12000;

enum class TelemetryState : uint8_t {
	Nothing = 0,
	Cooling = 1,
	Heating = 2
};

struct TelemetryBoxRecord {
	uint32_t t;
	int16_t box_temperature;
	int16_t box_target;
	int16_t fermenter_temperature;
	int16_t set_temperature;
	int16_t hot_temperature;
	int16_t voltage;
	int16_t current;
	int16_t pwm;
	int8_t rssi;
	uint8_t state;
};

struct TelemetryISpindelRecord {
	uint32_t t;
	uint32_t interval;
	int16_t temperature;
	int16_t gravity;
	int16_t angle;
	int16_t battery;
	int8_t rssi;
};

static_assert(sizeof(TelemetryBoxRecord) == 24, "Telemetry box record must remain 24 bytes");

int16_t telemetryFixedPoint(float value, int32_t scale);
const char *telemetryStateToString(TelemetryState state);
void captureTelemetrySnapshot();
void telemetryISpindelPush(const TelemetryISpindelRecord &record);
void telemetryQueueReset();
uint8_t telemetryQueuePending();
uint8_t telemetryBoxPending();
uint8_t telemetryISpindelPending();
uint8_t telemetryBoxStride();
uint8_t telemetryISpindelStride();
const TelemetryBoxRecord &telemetryBoxPeek(uint8_t index);
const TelemetryISpindelRecord &telemetryISpindelPeek(uint8_t index);
void telemetryBoxPop(uint8_t count);
void telemetryISpindelPop(uint8_t count);
