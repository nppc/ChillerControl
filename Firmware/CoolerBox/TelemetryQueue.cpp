#include "TelemetryQueue.h"
#include "CoolerBox.h"

#include <math.h>

namespace {
template <typename T, size_t N, uint8_t MAX_STRIDE>
class Ring {
public:
	void push(const T &record) {
		if (++skip < stride) {
			return;
		}
		skip = 0;

		if (count == N) {
			if (stride < MAX_STRIDE) {
				for (size_t i = 0; i < N / 2; ++i) {
					buf[(head + i) % N] = buf[(head + 2 * i) % N];
				}
				count = N / 2;
				stride *= 2;
				Debug.addLine(String(F("Ring downsample: cnt=")) + count + F(" str=") + stride);
			} else {
				head = (head + 1) % N;
				--count;
				Debug.addLine(String(F("Ring drop: cnt=")) + count);
			}
		}
		buf[(head + count) % N] = record;
		++count;
	}

	void reset() {
		stride = 1;
		skip = 0;
	}

	uint8_t pending() const {
		return static_cast<uint8_t>(count);
	}

	uint8_t getStride() const {
		return stride;
	}

	const T &peek(uint8_t index) const {
		return buf[(head + index) % N];
	}

	void pop(uint8_t amount) {
		if (amount > count) {
			amount = static_cast<uint8_t>(count);
		}
		head = (head + amount) % N;
		count -= amount;
	}

private:
	T buf[N] = {};
	size_t head = 0;
	size_t count = 0;
	uint8_t stride = 1;
	uint8_t skip = 0;
};

Ring<TelemetryBoxRecord, TELEMETRY_RAM_RECORDS, 16> boxRing;
Ring<TelemetryISpindelRecord, ISPINDEL_RAM_RECORDS, 8> iSpindelRing;

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

	boxRing.push(record);

	Debug.addLine(String(F("Snap #")) + boxRing.pending() +
	              F(" str=") + boxRing.getStride() +
	              F(" hp=") + ESP.getFreeHeap() +
	              F(" blk=") + ESP.getMaxFreeBlockSize() +
	              F(" stk=") + ESP.getFreeContStack());
}

void telemetryISpindelPush(const TelemetryISpindelRecord &record) {
	iSpindelRing.push(record);
	Debug.addLine(String(F("iSpindel #")) + iSpindelRing.pending() +
	              F(" str=") + iSpindelRing.getStride());
}

void telemetryQueueReset() {
	boxRing.reset();
	iSpindelRing.reset();
}

uint8_t telemetryQueuePending() {
	return static_cast<uint8_t>(boxRing.pending() + iSpindelRing.pending());
}

uint8_t telemetryBoxPending() {
	return boxRing.pending();
}

uint8_t telemetryISpindelPending() {
	return iSpindelRing.pending();
}

uint8_t telemetryBoxStride() {
	return boxRing.getStride();
}

uint8_t telemetryISpindelStride() {
	return iSpindelRing.getStride();
}

const TelemetryBoxRecord &telemetryBoxPeek(uint8_t index) {
	return boxRing.peek(index);
}

const TelemetryISpindelRecord &telemetryISpindelPeek(uint8_t index) {
	return iSpindelRing.peek(index);
}

void telemetryBoxPop(uint8_t count) {
	boxRing.pop(count);
}

void telemetryISpindelPop(uint8_t count) {
	iSpindelRing.pop(count);
}
