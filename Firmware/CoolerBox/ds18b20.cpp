#include "CoolerBox.h"

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
DeviceAddress DS18B20_adr[2];
uint8_t DS18B20_count = 0;

int boxTemp_SmoothArray[filterSamples];   // array for holding raw sensor values for PT1000 sensor
int HotTemp_SmoothArray[filterSamples];   // array for holding raw sensor values for PT1000 sensor

enum TempConversionState {
	TEMP_CONV_IDLE,
	TEMP_CONV_PENDING
};

static TempConversionState tempConvState = TEMP_CONV_IDLE;
static unsigned long tempRequestStartTime = 0;
const unsigned long TEMP_CONVERSION_DELAY_MS = 1200; // 1200ms provides generous margin for slow clone ICs at cold temperatures

#ifdef DEBUG_TEMPERATURE_SENSORS
namespace {
const unsigned long DEBUG_TEMPERATURE_CYCLE_MS = 24UL * 60UL * 60UL * 1000UL;
const unsigned long DEBUG_ISPINDEL_FIRST_REPORT_DELAY_MS = 20UL * 1000UL;
const unsigned long DEBUG_ISPINDEL_REPORT_INTERVAL_MS = 5UL * 60UL * 1000UL;
const float DEBUG_BOX_BASE_TEMPERATURE_C = 16.0f;
const float DEBUG_BOX_DRIFT_C = 2.0f;
const float DEBUG_HEATER_C_PER_MINUTE_AT_100_PERCENT = 1.0f;
const float DEBUG_LIQUID_HEATER_C_PER_MINUTE_PER_PERCENT = 0.005f;
const float DEBUG_ISPINDEL_MAX_RATE_C_PER_HOUR = 1.0f;
bool debugSensorsInitialized = false;
unsigned long debugStartMillis = 0;
unsigned long debugLastUpdateMillis = 0;
unsigned long debugLastISpindelReportMillis = 0;
bool debugFirstISpindelReportSent = false;
float debugHeaterRiseC = 0.0f;
float debugLiquidHeaterRiseC = 0.0f;

void updateDebugTemperatureSensors() {
	unsigned long now = millis();
	if (!debugSensorsInitialized) {
		debugSensorsInitialized = true;
		debugStartMillis = now;
		debugLastUpdateMillis = now;
		debugLastISpindelReportMillis = now;
		debugFirstISpindelReportSent = false;
		debugHeaterRiseC = 0.0f;
		debugLiquidHeaterRiseC = 0.0f;
		boxTemp = DEBUG_BOX_BASE_TEMPERATURE_C;
		HotTemp = boxTemp;
		iSpindelTemperature = 18.0f;
		iSpindelIntervalSec = DEBUG_ISPINDEL_REPORT_INTERVAL_MS / 1000UL;
		useISpindelTemp = true;
		lastISpindelMillis = 0;
		Debug.addLine(F("DEBUG temperature simulation enabled: box=16C, iSpindel=18C"));
		return;
	}

	unsigned long elapsedMillis = now - debugLastUpdateMillis;
	debugLastUpdateMillis = now;
	float elapsedMinutes = static_cast<float>(elapsedMillis) / 60000.0f;
	float heatingPercent = maxHotPWM > 0
		? constrain(setHotPWM * 100.0f / static_cast<float>(maxHotPWM), 0.0f, 100.0f)
		: 0.0f;
	debugHeaterRiseC += heatingPercent *
	                    DEBUG_HEATER_C_PER_MINUTE_AT_100_PERCENT / 100.0f *
	                    elapsedMinutes;
	debugLiquidHeaterRiseC += heatingPercent *
	                         DEBUG_LIQUID_HEATER_C_PER_MINUTE_PER_PERCENT *
	                         elapsedMinutes;

	unsigned long phaseMillis = (now - debugStartMillis) % DEBUG_TEMPERATURE_CYCLE_MS;
	float phaseHours = static_cast<float>(phaseMillis) / 3600000.0f;
	float driftC = phaseHours <= 12.0f
		? DEBUG_BOX_DRIFT_C * phaseHours / 12.0f
		: DEBUG_BOX_DRIFT_C * (24.0f - phaseHours) / 12.0f;
	boxTemp = DEBUG_BOX_BASE_TEMPERATURE_C + driftC + debugHeaterRiseC;
	HotTemp = boxTemp;

	unsigned long reportInterval = debugFirstISpindelReportSent
		? DEBUG_ISPINDEL_REPORT_INTERVAL_MS
		: DEBUG_ISPINDEL_FIRST_REPORT_DELAY_MS;
	if (now - debugLastISpindelReportMillis >= reportInterval) {
		unsigned long reportElapsedMillis = now - debugLastISpindelReportMillis;
		float reportElapsedHours = static_cast<float>(reportElapsedMillis) / 3600000.0f;
		float liquidRate = constrain(boxTemp - iSpindelTemperature,
		                             -DEBUG_ISPINDEL_MAX_RATE_C_PER_HOUR,
		                             DEBUG_ISPINDEL_MAX_RATE_C_PER_HOUR);
		iSpindelTemperature += liquidRate * reportElapsedHours +
		                       debugLiquidHeaterRiseC;
		debugLiquidHeaterRiseC = 0.0f;
		debugLastISpindelReportMillis = now;
		debugFirstISpindelReportSent = true;
		lastISpindelMillis = now == 0 ? 1 : now;
	}
}
}
#endif

// smooth algorithm for ADC reading
int digitalSmooth(int rawIn, int *sensSmoothArray) {
	int j, k, temp, top, bottom;
	long total;
	static int i;
	static int sorted[filterSamples];
	boolean done;

	i = (i + 1) % filterSamples;    // increment counter and roll over if necc.
	sensSmoothArray[i] = rawIn;     // input new data into the oldest slot

	for (j = 0; j < filterSamples; j++) {
		sorted[j] = sensSmoothArray[j];
	}

	done = 0;
	while (done != 1) {
		done = 1;
		for (j = 0; j < (filterSamples - 1); j++) {
			if (sorted[j] > sorted[j + 1]) {
				temp = sorted[j + 1];
				sorted[j + 1] = sorted[j];
				sorted[j] = temp;
				done = 0;
			}
		}
	}

	// throw out top and bottom 15% of samples - limit to throw out at least one from top and bottom
	bottom = max(((filterSamples * 15) / 100), 1);
	top = min((((filterSamples * 85) / 100) + 1), (filterSamples - 1));
	k = 0;
	total = 0;
	for (j = bottom; j < top; j++) {
		total += sorted[j];
		k++;
	}

	return total / k;
}

static bool firstRequestPending = true;
static bool boxTempInitialized = false;
static bool hotTempInitialized = false;

void resetTempConversionState() {
	tempConvState = TEMP_CONV_IDLE;
	tempRequestStartTime = 0;
	millisPIDinterval = millis();
	firstRequestPending = true;
	boxTempInitialized = false;
	hotTempInitialized = false;
}

void INIT_DS18B20() {
	DS18B20.begin();
	DS18B20_count = 0;
	oneWire.reset_search();

	DeviceAddress address;
	while (DS18B20_count < 2 && oneWire.search(address)) {
		if (OneWire::crc8(address, 7) != address[7] || address[0] != 0x28) {
			continue;
		}

		memcpy(DS18B20_adr[DS18B20_count], address, sizeof(DeviceAddress));
		DS18B20_count++;
	}

	for (uint8_t x = 0; x < DS18B20_count; x++) {
		DS18B20.setResolution(DS18B20_adr[x], 12);
	}

	DS18B20.setWaitForConversion(false);
	resetTempConversionState();

	if (DS18B20_count == 0) {
		Debug.addLine(F("No DS18B20 found by direct OneWire scan"));
	}
}

#ifndef DEBUG_TEMPERATURE_SENSORS
static bool isTemperatureValid(float temp) {
	return (temp != DEVICE_DISCONNECTED_C && temp != 85.0f && temp > -55.0f && temp < 125.0f);
}

static float fetchSensorTemperature(int sensorId, const char* sensorName) {
	float temp = DEVICE_DISCONNECTED_C;
	for (int attempt = 0; attempt < 3; attempt++) {
		if (attempt > 0) {
			oneWire.reset();
			delay(25);
		}

		if (sensorId >= 0 && sensorId < DS18B20_count) {
			temp = DS18B20.getTempC(DS18B20_adr[sensorId]);
		} else {
			temp = DS18B20.getTempCByIndex(sensorId);
		}

		if (isTemperatureValid(temp)) {
			return temp;
		}
	}

	Debug.addLine(String(F("Notice: ")) + sensorName + F(" sensor read retry failed (raw=") + String(temp, 1) + F(")"));
	return temp;
}
#endif

void readDS18B20Temperatures() {
#ifdef DEBUG_TEMPERATURE_SENSORS
	updateDebugTemperatureSensors();
#else
	oneWire.reset(); // Pre-settle OneWire bus after conversion sleep to avoid clone wake-up timeout
	delay(5);
	float rawCold = fetchSensorTemperature(ColdSensorId, "Cold");
	float rawHot = fetchSensorTemperature(HotSensorId, "Hot");

	static bool coldFailLogged = false;
	if (isTemperatureValid(rawCold)) {
		int tmpT = (int)round(rawCold * 100.0f);
		if (!boxTempInitialized) {
			for (int idx = 0; idx < filterSamples; idx++) {
				boxTemp_SmoothArray[idx] = tmpT;
			}
			boxTempInitialized = true;
			boxTemp = rawCold;
		} else {
			boxTemp = (float)digitalSmooth(tmpT, boxTemp_SmoothArray) / 100.0f;
		}
		coldFailLogged = false;
	} else if (!coldFailLogged) {
		Debug.addLine(F("Warn: Cold sensor read failed"));
		coldFailLogged = true;
	}

	static bool hotFailLogged = false;
	if (isTemperatureValid(rawHot)) {
		int tmpT = (int)round(rawHot * 100.0f);
		if (!hotTempInitialized) {
			for (int idx = 0; idx < filterSamples; idx++) {
				HotTemp_SmoothArray[idx] = tmpT;
			}
			hotTempInitialized = true;
			HotTemp = rawHot;
		} else {
			HotTemp = (float)digitalSmooth(tmpT, HotTemp_SmoothArray) / 100.0f;
		}
		hotFailLogged = false;
	} else if (!hotFailLogged) {
		Debug.addLine(F("Warn: Hot sensor read failed"));
		hotFailLogged = true;
	}
#endif
}

void primeDS18B20() {
	DS18B20.requestTemperatures();
	delay(TEMP_CONVERSION_DELAY_MS);
	readDS18B20Temperatures();
	firstRequestPending = false;
	millisPIDinterval = millis();
}

bool updateDS18B20() {
	if (tempConvState == TEMP_CONV_IDLE) {
		if (firstRequestPending || (millis() - millisPIDinterval > (PIDCOMPUTE_INTERVAL * 1000UL))) {
			firstRequestPending = false;
			millisPIDinterval = millis();
			DS18B20.requestTemperatures();
			tempRequestStartTime = millis();
			tempConvState = TEMP_CONV_PENDING;
		}
	} else if (tempConvState == TEMP_CONV_PENDING) {
		if (millis() - tempRequestStartTime >= TEMP_CONVERSION_DELAY_MS) {
			tempConvState = TEMP_CONV_IDLE;
			readDS18B20Temperatures();
			return true;
		}
	}
	return false;
}
