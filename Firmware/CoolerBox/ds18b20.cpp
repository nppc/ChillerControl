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
const unsigned long TEMP_CONVERSION_DELAY_MS = 800; // DS18B20 12-bit max conversion is 750ms + 50ms margin

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

void resetTempConversionState() {
	tempConvState = TEMP_CONV_IDLE;
	tempRequestStartTime = 0;
	millisPIDinterval = millis();
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

void readDS18B20Temperatures() {
	float rawCold = DS18B20.getTempCByIndex(ColdSensorId);
	float rawHot = DS18B20.getTempCByIndex(HotSensorId);

	static bool coldFailLogged = false;
	if (rawCold != DEVICE_DISCONNECTED_C && rawCold > -55.0f && rawCold < 125.0f) {
		int tmpT = (int)round(rawCold * 100.0f);
		boxTemp = (float)digitalSmooth(tmpT, boxTemp_SmoothArray) / 100.0f;
		coldFailLogged = false;
	} else if (!coldFailLogged) {
		Debug.addLine(F("Warn: Cold sensor read failed"));
		coldFailLogged = true;
	}

	static bool hotFailLogged = false;
	if (rawHot != DEVICE_DISCONNECTED_C && rawHot > -55.0f && rawHot < 125.0f) {
		int tmpT = (int)round(rawHot * 100.0f);
		HotTemp = (float)digitalSmooth(tmpT, HotTemp_SmoothArray) / 100.0f;
		hotFailLogged = false;
	} else if (!hotFailLogged) {
		Debug.addLine(F("Warn: Hot sensor read failed"));
		hotFailLogged = true;
	}
}

void primeDS18B20() {
	for (byte i1 = 0; i1 < filterSamples * 2; i1++) {
		DS18B20.requestTemperatures();
		delay(1000);
		readDS18B20Temperatures();
	}
}

bool updateDS18B20() {
	if (tempConvState == TEMP_CONV_IDLE) {
		if (millis() - millisPIDinterval > (PIDCOMPUTE_INTERVAL * 1000UL)) {
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

