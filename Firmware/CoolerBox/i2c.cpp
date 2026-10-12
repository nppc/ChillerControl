#include "CoolerBox.h"

bool I2C_PRESENT = false;
uint8_t i2c_data[I2C_DATALEN];
byte i2c_PWM_value = 0;

void initI2C() {
	Wire.begin(D2, D1);  // D2-sda, D1-scl
	delay(100);          // Allow I2C bus lines to stabilize
	Wire.setClock(50000); // 50kHz clock rate for stable communication with slave MCU
	Wire.setClockStretchLimit(150000L); // 150ms timeout to allow slave time for clock stretching

	// Detect I2C device
	Wire.beginTransmission(I2C_ADDRESS);
	byte error = Wire.endTransmission();
	if (error == 0) {
		I2C_PRESENT = HIGH;
		Serial.println(F("I2C Buck converter found at address 0x5E"));
	} else {
		I2C_PRESENT = LOW;
		Serial.println(F("I2C Buck converter is not connected."));
		Debug.addLine(F("I2C device not detected"));
	}

	delay(100);
	millisReadI2CDataInterval = millis();
}

void sendI2Cdata(bool force, bool sendLimits) {
	static uint8_t lastSentVoltageCommand = 0xFF;
	static uint8_t lastSentSpeedCommand = 0xFF;
	static uint8_t lastSentMinVoltCommand = 0xFF;
	static uint8_t lastSentMaxVoltCommand = 0xFF;

	const uint8_t voltageCommand = static_cast<uint8_t>(setVoltage * 10.0f);
	const uint8_t speedCommand = static_cast<uint8_t>(changeVoltageSpeed * 10.0f);
	const uint8_t minVoltCommand = static_cast<uint8_t>(minVoltage * 10.0f);
	const uint8_t maxVoltCommand = static_cast<uint8_t>(maxVoltage * 10.0f);

	bool limitsChanged = (speedCommand != lastSentSpeedCommand ||
	                      minVoltCommand != lastSentMinVoltCommand ||
	                      maxVoltCommand != lastSentMaxVoltCommand);
	bool voltageChanged = (voltageCommand != lastSentVoltageCommand);

	bool shouldSendLimits = sendLimits || limitsChanged;

	if (!force && !shouldSendLimits && !voltageChanged) {
		return;
	}

	Wire.beginTransmission(I2C_ADDRESS);
	Wire.write(voltageCommand); // Byte 0: Set Voltage (V*10)

	if (shouldSendLimits) {
		Wire.write(speedCommand);   // Byte 1: Voltage change rate ((V*10)/s)
		Wire.write(minVoltCommand);  // Byte 2: Min Voltage (V*10)
		Wire.write(maxVoltCommand);  // Byte 3: Max Voltage (V*10)
	}
	byte error = Wire.endTransmission();

	if (error == 0) {
		if (!I2C_PRESENT) {
			I2C_PRESENT = HIGH;
			Debug.addLine(F("I2C connection restored"));
		}
		if (shouldSendLimits) {
			Debug.addLine(String(F("I2C limits updated: min=")) +
				String(minVoltCommand / 10.0, 1) + F("V max=") +
				String(maxVoltCommand / 10.0, 1) + F("V rate=") +
				String(speedCommand / 10.0, 1) + F("V/s"));
			lastSentSpeedCommand = speedCommand;
			lastSentMinVoltCommand = minVoltCommand;
			lastSentMaxVoltCommand = maxVoltCommand;
		}
		if (voltageChanged || shouldSendLimits) {
			Debug.addLine(String(F("I2C voltage command ACK: ")) +
				String(voltageCommand / 10.0, 1) + F("V"));
			lastSentVoltageCommand = voltageCommand;
		}
	} else {
		if (I2C_PRESENT) {
			I2C_PRESENT = LOW;
			Debug.addLine(String(F("I2C write failed; voltage command was ")) +
				String(voltageCommand / 10.0, 1) + F("V"));
		}
		lastSentVoltageCommand = 0xFF;
		lastSentSpeedCommand = 0xFF;
		lastSentMinVoltCommand = 0xFF;
		lastSentMaxVoltCommand = 0xFF;
	}
}

void readI2Cdata() {
	bool readSucceeded = false;

	Wire.requestFrom(static_cast<uint8_t>(I2C_ADDRESS), static_cast<uint8_t>(I2C_DATALEN));
	uint8_t b_avail = Wire.available();

	if (b_avail == I2C_DATALEN) {
		for (uint8_t i = 0; i < I2C_DATALEN; i++) {
			i2c_data[i] = Wire.read();
		}
		if (!I2C_PRESENT) {
			I2C_PRESENT = HIGH;
			Debug.addLine(F("I2C connection restored"));
			sendI2Cdata(true, true); // Sync current settings to the reconnected device
		}
		readSucceeded = true;
	} else {
		while (Wire.available()) {
			Wire.read();
		}
		if (I2C_PRESENT) {
			I2C_PRESENT = LOW;
			Debug.addLine(String(F("I2C read failed; received ")) + b_avail + F(" bytes"));
		}
	}

	if (readSucceeded) {
		i2c_PWM_value = i2c_data[4];
		measuredVoltage = static_cast<float>(i2c_data[5]) / 10.0f;
		float current = static_cast<float>(i2c_data[6]) / 10.0f;
		measuredCurrent = (current > 13.0f ? 0.0f : current);
	}
}

