#include "TelemetryQueue.h"
#include "CoolerBox.h"

#include <math.h>
#include <sys/time.h>
extern "C" {
#include <user_interface.h>
}

namespace {
template <typename T, size_t N>
class Ring {
public:
	void push(const T &record) {
		if (count == N) {
			head = (head + 1) % N;
			--count;
			Debug.addLine(String(F("Ring drop: cnt=")) + count);
		}
		buf[(head + count) % N] = record;
		++count;
	}

	void clear() {
		head = 0;
		count = 0;
	}

	bool isFull() const {
		return count >= N;
	}

	uint8_t pending() const {
		return static_cast<uint8_t>(count);
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

	void restore(const T *records, uint8_t recordCount) {
		if (recordCount > N) {
			recordCount = N;
		}
		for (size_t i = 0; i < recordCount; ++i) {
			buf[i] = records[i];
		}
		head = 0;
		count = recordCount;
	}

private:
	T buf[N] = {};
	size_t head = 0;
	size_t count = 0;
};

Ring<TelemetryBoxRecord, TELEMETRY_RAM_RECORDS> boxRing;
Ring<TelemetryISpindelRecord, ISPINDEL_RAM_RECORDS> iSpindelRing;

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
	static uint8_t previousPending = 0;
	if (boxRing.isFull()) {
		bool wifiOffline = (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0));
		if (wifiOffline) {
			telemetryFSSaveCurrentRAM();
		}
	}
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

	uint8_t pending = boxRing.pending();
	if (pending > 1 && pending > previousPending) {
		Debug.addLine(String(F("Snap #")) + pending +
		              F(" hp=") + ESP.getFreeHeap() +
		              F(" blk=") + ESP.getMaxFreeBlockSize() +
		              F(" stk=") + ESP.getFreeContStack());
	}
	previousPending = pending;
}

void telemetryISpindelPush(const TelemetryISpindelRecord &record) {
	if (iSpindelRing.isFull()) {
		bool wifiOffline = (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0));
		if (wifiOffline) {
			telemetryFSSaveCurrentRAM();
		}
	}
	iSpindelRing.push(record);
	Debug.addLine(String(F("iSpindel #")) + iSpindelRing.pending());
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

bool telemetryBoxIsFull() {
	return boxRing.isFull();
}

bool telemetryISpindelIsFull() {
	return iSpindelRing.isFull();
}

namespace {
const char TELEMETRY_TMP_PATH[] = "/tel_tmp.dat";
const size_t TELEMETRY_FS_MAX_FILES = 300;

struct TelemetryFileHeader {
	uint32_t magic;         // 0x54454C51 ('TELQ')
	uint16_t version;       // 1
	uint16_t headerSize;    // sizeof(TelemetryFileHeader)
	uint32_t savedEpoch;    // epoch time when saved
	uint8_t boxCount;       // number of TelemetryBoxRecord entries
	uint8_t ispindelCount;  // number of TelemetryISpindelRecord entries
	uint16_t reserved1;     // alignment padding
	uint32_t reserved2;     // alignment padding
	uint32_t payloadCrc;    // CRC32 of all records following the header
};
static_assert(sizeof(TelemetryFileHeader) == 24, "Telemetry file header must be 24 bytes");

uint32_t calculateCrc32(const uint8_t *data, size_t length, uint32_t crc = 0xFFFFFFFF) {
	for (size_t i = 0; i < length; ++i) {
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; ++j) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xEDB88320UL;
			} else {
				crc = crc >> 1;
			}
		}
	}
	return crc;
}

bool parseTelemetryFileIndex(const char *filename, uint32_t &index) {
	if (!filename) return false;
	if (filename[0] == '/') {
		filename++;
	}
	if (strncmp(filename, "tel_", 4) != 0) {
		return false;
	}
	const char *numStart = filename + 4;
	char *endPtr = nullptr;
	unsigned long val = strtoul(numStart, &endPtr, 10);
	if (endPtr == nullptr || strcmp(endPtr, ".dat") != 0) {
		return false;
	}
	index = static_cast<uint32_t>(val);
	return true;
}

void getTelemetryFilePath(char *buffer, size_t capacity, uint32_t index) {
	snprintf(buffer, capacity, "/tel_%05lu.dat", static_cast<unsigned long>(index));
}

void scanTelemetryFS(size_t &count, uint32_t &minIndex, uint32_t &maxIndex) {
	count = 0;
	minIndex = UINT32_MAX;
	maxIndex = 0;

	if (!LittleFS.begin()) {
		return;
	}

	Dir dir = LittleFS.openDir("/");
	while (dir.next()) {
		uint32_t idx = 0;
		if (parseTelemetryFileIndex(dir.fileName().c_str(), idx)) {
			count++;
			if (idx < minIndex) {
				minIndex = idx;
			}
			if (idx > maxIndex) {
				maxIndex = idx;
			}
		}
	}
}

static size_t fsFileCount = 0;
static uint32_t fsNextWriteIndex = 0;
}

void telemetryFSInit() {
	if (LittleFS.exists(TELEMETRY_TMP_PATH)) {
		LittleFS.remove(TELEMETRY_TMP_PATH);
	}
	size_t count = 0;
	uint32_t minIdx = 0;
	uint32_t maxIdx = 0;
	scanTelemetryFS(count, minIdx, maxIdx);
	fsFileCount = count;
	fsNextWriteIndex = (count > 0) ? (maxIdx + 1) : 0;
	Debug.addLine(String(F("LittleFS telemetry queue: files=")) + fsFileCount +
	              (count > 0 ? (F(" nextIdx=") + String(fsNextWriteIndex)) : F("")));
}

bool telemetryFSHasData() {
	return fsFileCount > 0;
}

size_t telemetryFSFileCount() {
	return fsFileCount;
}

bool telemetryFSSaveCurrentRAM() {
	uint8_t boxCnt = telemetryBoxPending();
	uint8_t ispindelCnt = telemetryISpindelPending();
	if (boxCnt == 0 && ispindelCnt == 0) {
		return true;
	}

	if (fsFileCount >= TELEMETRY_FS_MAX_FILES) {
		Debug.addLine(F("LittleFS queue full, cannot save file"));
		return false;
	}

	if (!LittleFS.begin()) {
		Debug.addLine(F("LittleFS unavailable to save queue"));
		return false;
	}

	char finalPath[24];
	getTelemetryFilePath(finalPath, sizeof(finalPath), fsNextWriteIndex);

	File file = LittleFS.open(TELEMETRY_TMP_PATH, "w");
	if (!file) {
		Debug.addLine(F("Failed to open /tel_tmp.dat for write"));
		return false;
	}

	TelemetryFileHeader header = {};
	header.magic = 0x54454C51;
	header.version = 1;
	header.headerSize = sizeof(TelemetryFileHeader);
	header.savedEpoch = clockValid() ? epochSeconds() : 0;
	header.boxCount = boxCnt;
	header.ispindelCount = ispindelCnt;

	uint32_t crc = 0xFFFFFFFF;
	for (uint8_t i = 0; i < boxCnt; ++i) {
		const TelemetryBoxRecord &rec = telemetryBoxPeek(i);
		crc = calculateCrc32(reinterpret_cast<const uint8_t *>(&rec), sizeof(TelemetryBoxRecord), crc);
	}
	for (uint8_t i = 0; i < ispindelCnt; ++i) {
		const TelemetryISpindelRecord &rec = telemetryISpindelPeek(i);
		crc = calculateCrc32(reinterpret_cast<const uint8_t *>(&rec), sizeof(TelemetryISpindelRecord), crc);
	}
	header.payloadCrc = crc;

	if (file.write(reinterpret_cast<const uint8_t *>(&header), sizeof(header)) != sizeof(header)) {
		file.close();
		LittleFS.remove(TELEMETRY_TMP_PATH);
		Debug.addLine(F("Failed writing queue header"));
		return false;
	}

	for (uint8_t i = 0; i < boxCnt; ++i) {
		const TelemetryBoxRecord &rec = telemetryBoxPeek(i);
		if (file.write(reinterpret_cast<const uint8_t *>(&rec), sizeof(TelemetryBoxRecord)) != sizeof(TelemetryBoxRecord)) {
			file.close();
			LittleFS.remove(TELEMETRY_TMP_PATH);
			Debug.addLine(F("Failed writing box records"));
			return false;
		}
	}

	for (uint8_t i = 0; i < ispindelCnt; ++i) {
		const TelemetryISpindelRecord &rec = telemetryISpindelPeek(i);
		if (file.write(reinterpret_cast<const uint8_t *>(&rec), sizeof(TelemetryISpindelRecord)) != sizeof(TelemetryISpindelRecord)) {
			file.close();
			LittleFS.remove(TELEMETRY_TMP_PATH);
			Debug.addLine(F("Failed writing ispindel records"));
			return false;
		}
	}

	file.flush();
	file.close();

	if (LittleFS.exists(finalPath)) {
		LittleFS.remove(finalPath);
	}
	if (!LittleFS.rename(TELEMETRY_TMP_PATH, finalPath)) {
		Debug.addLine(String(F("Failed renaming to ")) + finalPath);
		return false;
	}

	boxRing.clear();
	iSpindelRing.clear();
	fsNextWriteIndex++;
	fsFileCount++;

	Debug.addLine(String(F("Spilled to LittleFS: file=")) + finalPath +
	              F(" box=") + boxCnt + F(" isp=") + ispindelCnt +
	              F(" totalFiles=") + fsFileCount);
	return true;
}

bool telemetryFSLoadOldestToRAM() {
	if (fsFileCount == 0) {
		return false;
	}

	size_t count = 0;
	uint32_t minIdx = 0;
	uint32_t maxIdx = 0;
	scanTelemetryFS(count, minIdx, maxIdx);
	fsFileCount = count;
	if (count == 0) {
		return false;
	}

	char filePath[24];
	getTelemetryFilePath(filePath, sizeof(filePath), minIdx);

	File file = LittleFS.open(filePath, "r");
	if (!file) {
		Debug.addLine(String(F("Failed opening ")) + filePath);
		LittleFS.remove(filePath);
		if (fsFileCount > 0) fsFileCount--;
		return false;
	}

	TelemetryFileHeader header = {};
	if (file.read(reinterpret_cast<uint8_t *>(&header), sizeof(header)) != sizeof(header)) {
		Debug.addLine(String(F("Truncated header in ")) + filePath);
		file.close();
		LittleFS.remove(filePath);
		if (fsFileCount > 0) fsFileCount--;
		return false;
	}

	if (header.magic != 0x54454C51 || header.version != 1 || header.headerSize != sizeof(TelemetryFileHeader)) {
		Debug.addLine(String(F("Bad header in ")) + filePath);
		file.close();
		LittleFS.remove(filePath);
		if (fsFileCount > 0) fsFileCount--;
		return false;
	}

	if (header.boxCount > TELEMETRY_RAM_RECORDS || header.ispindelCount > ISPINDEL_RAM_RECORDS) {
		Debug.addLine(String(F("Invalid counts in ")) + filePath);
		file.close();
		LittleFS.remove(filePath);
		if (fsFileCount > 0) fsFileCount--;
		return false;
	}

	size_t expectedSize = sizeof(header) +
	                      header.boxCount * sizeof(TelemetryBoxRecord) +
	                      header.ispindelCount * sizeof(TelemetryISpindelRecord);
	if (file.size() < expectedSize) {
		Debug.addLine(String(F("File size too small: ")) + filePath);
		file.close();
		LittleFS.remove(filePath);
		if (fsFileCount > 0) fsFileCount--;
		return false;
	}

	TelemetryBoxRecord boxBuf[TELEMETRY_RAM_RECORDS];
	TelemetryISpindelRecord ispindelBuf[ISPINDEL_RAM_RECORDS];

	uint32_t crc = 0xFFFFFFFF;
	for (uint8_t i = 0; i < header.boxCount; ++i) {
		if (file.read(reinterpret_cast<uint8_t *>(&boxBuf[i]), sizeof(TelemetryBoxRecord)) != sizeof(TelemetryBoxRecord)) {
			Debug.addLine(F("Failed reading box record"));
			file.close();
			LittleFS.remove(filePath);
			if (fsFileCount > 0) fsFileCount--;
			return false;
		}
		crc = calculateCrc32(reinterpret_cast<const uint8_t *>(&boxBuf[i]), sizeof(TelemetryBoxRecord), crc);
	}

	for (uint8_t i = 0; i < header.ispindelCount; ++i) {
		if (file.read(reinterpret_cast<uint8_t *>(&ispindelBuf[i]), sizeof(TelemetryISpindelRecord)) != sizeof(TelemetryISpindelRecord)) {
			Debug.addLine(F("Failed reading ispindel record"));
			file.close();
			LittleFS.remove(filePath);
			if (fsFileCount > 0) fsFileCount--;
			return false;
		}
		crc = calculateCrc32(reinterpret_cast<const uint8_t *>(&ispindelBuf[i]), sizeof(TelemetryISpindelRecord), crc);
	}

	file.close();
	LittleFS.remove(filePath);
	if (fsFileCount > 0) fsFileCount--;

	if (crc != header.payloadCrc) {
		Debug.addLine(String(F("CRC mismatch in ")) + filePath);
		return false;
	}

	boxRing.restore(boxBuf, header.boxCount);
	iSpindelRing.restore(ispindelBuf, header.ispindelCount);

	// If software restart and clock not valid yet, restore clock from header if available
	rst_info *resetInfo = ESP.getResetInfoPtr();
	if (resetInfo != nullptr && resetInfo->reason == REASON_SOFT_RESTART && !clockValid()) {
		if (header.savedEpoch >= 1700000000) {
			struct timeval tv = { static_cast<time_t>(header.savedEpoch + 2), 0 };
			settimeofday(&tv, nullptr);
			Debug.addLine(String(F("Restored clock after soft restart: ")) + static_cast<unsigned long>(header.savedEpoch + 2));
		}
	}

	// Restore iSpindel state if fresh
	if (header.ispindelCount > 0 && header.savedEpoch >= 1700000000 && clockValid()) {
		const TelemetryISpindelRecord &latestISpindel = ispindelBuf[header.ispindelCount - 1];
		uint32_t lastT = latestISpindel.t;
		if (epochSeconds() >= lastT) {
			uint32_t elapsedSec = epochSeconds() - lastT;
			unsigned long timeoutSec = max(45UL * 60UL, static_cast<unsigned long>(iSpindelIntervalSec) * 2UL);
			if (elapsedSec < timeoutSec) {
				if (latestISpindel.temperature != INT16_MIN) {
					iSpindelTemperature = static_cast<float>(latestISpindel.temperature) / 100.0f;
				}
				if (latestISpindel.rssi != INT8_MIN) {
					iSpindelRssi = latestISpindel.rssi;
				}
				if (latestISpindel.interval >= 30 && latestISpindel.interval <= 86400) {
					iSpindelIntervalSec = latestISpindel.interval;
				}
				unsigned long elapsedMs = elapsedSec * 1000UL;
				unsigned long now = millis();
				lastISpindelMillis = (now > elapsedMs) ? (now - elapsedMs) : 1;
				if (isBootAutoStartPending()) {
					cancelBootAutoStart();
					setCoolerStopped(LOW);
				}
				updateCascadeTarget();
			}
		}
	}

	Debug.addLine(String(F("Loaded from LittleFS: file=")) + filePath +
	              F(" box=") + header.boxCount + F(" isp=") + header.ispindelCount +
	              F(" remainingFiles=") + fsFileCount);
	return true;
}
