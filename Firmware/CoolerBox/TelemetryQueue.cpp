#include "TelemetryQueue.h"
#include "CoolerBox.h"

namespace {
TelemetrySnapshot ring[TELEMETRY_RAM_RECORDS];
uint8_t head = 0;
uint8_t pendingCount = 0;
uint32_t drops = 0;
uint32_t chunksSentCount = 0;
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

void telemetryQueuePush(const TelemetrySnapshot &snapshot) {
	if (pendingCount >= TELEMETRY_RAM_RECORDS) {
		ring[head] = snapshot;
		head = (head + 1) % TELEMETRY_RAM_RECORDS;
		++drops;
	} else {
		uint8_t tail = (head + pendingCount) % TELEMETRY_RAM_RECORDS;
		ring[tail] = snapshot;
		++pendingCount;
	}
}

void captureTelemetrySnapshot() {
	TelemetrySnapshot snap;
	snap.t = epochSeconds();
	snap.box_temperature = boxTemp;
	snap.box_target = effectiveBoxTarget;
	snap.fermenter_temperature = getFermenterTemp();

	float targetVal = isHeatingAutoTuning() ? getHeatingAutoTuneTargetTemp() : setTemp;
	snap.set_temperature = targetVal;
	snap.hot_temperature = HotTemp;
	snap.voltage = measuredVoltage;
	snap.current = measuredCurrent;

	float pwmPct = (maxHotPWM > 0) ? (setHotPWM * 100.0f / static_cast<float>(maxHotPWM)) : 0.0f;
	snap.hot_pwm = pwmPct;
	snap.pwm = pwmPct;

	snap.state = (setHotPWM > 0.0f) ? static_cast<uint8_t>(TelemetryState::Heating) :
		((!isCoolerStopped() && isCoolingDemanded()) ? static_cast<uint8_t>(TelemetryState::Cooling) : static_cast<uint8_t>(TelemetryState::Nothing));

	snap.rssi = static_cast<float>(WiFi.RSSI());

	if (hasNewISpindelData) {
		snap.ispindel_temperature = iSpindelTemperature;
		snap.ispindel_gravity = iSpindelGravity;
		snap.ispindel_battery = iSpindelBattery;
		snap.ispindel_rssi = static_cast<float>(iSpindelRssi);
	} else {
		snap.ispindel_temperature = NAN;
		snap.ispindel_gravity = NAN;
		snap.ispindel_battery = NAN;
		snap.ispindel_rssi = NAN;
	}

	telemetryQueuePush(snap);
}

uint8_t telemetryQueuePending() {
	return pendingCount;
}

uint32_t telemetryQueueDrops() {
	return drops;
}

uint32_t telemetryChunksSent() {
	return chunksSentCount;
}

void telemetryQueueIncrementChunksSent() {
	++chunksSentCount;
}

const TelemetrySnapshot &telemetryQueuePeek(uint8_t index) {
	uint8_t pos = (head + index) % TELEMETRY_RAM_RECORDS;
	return ring[pos];
}

void telemetryQueuePop(uint8_t count) {
	if (count > pendingCount) {
		count = pendingCount;
	}
	head = (head + count) % TELEMETRY_RAM_RECORDS;
	pendingCount -= count;
}

size_t telemetryRingSizeBytes() {
	return sizeof(ring);
}

