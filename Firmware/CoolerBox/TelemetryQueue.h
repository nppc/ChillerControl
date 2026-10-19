#pragma once

#include <Arduino.h>

const size_t TELEMETRY_RAM_RECORDS = 64;
const size_t TELEMETRY_CHUNK_MAX = 12;
const size_t TELEMETRY_MAX_PAYLOAD = 3072;
const unsigned long TELEMETRY_CHUNK_GAP_MS = 2000UL;
const size_t TELEMETRY_MIN_FREE_BLOCK = 12000;

enum class TelemetryState : uint8_t {
	Nothing = 0,
	Cooling = 1,
	Heating = 2
};

const char *telemetryStateToString(TelemetryState state);

struct TelemetrySnapshot {
	uint32_t t;
	float box_temperature;
	float box_target;
	float fermenter_temperature;
	float set_temperature;
	float hot_temperature;
	float voltage;
	float current;
	float hot_pwm;
	float pwm;
	float rssi;
	float ispindel_temperature;
	float ispindel_gravity;
	float ispindel_battery;
	float ispindel_rssi;
	uint8_t state;
};

void telemetryQueuePush(const TelemetrySnapshot &snapshot);
void captureTelemetrySnapshot();
uint8_t telemetryQueuePending();
uint32_t telemetryQueueDrops();
uint32_t telemetryChunksSent();
void telemetryQueueIncrementChunksSent();
const TelemetrySnapshot &telemetryQueuePeek(uint8_t index);
void telemetryQueuePop(uint8_t count);
size_t telemetryRingSizeBytes();

