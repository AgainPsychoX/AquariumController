#pragma once

#include "common.hpp"

namespace Heating {
	constexpr byte pin = 0;
	constexpr unsigned int EEPROMOffset = 0x00A;

	float maxTemperature = 21.75f;
	float minTemperature = 24.25f;

	void saveSettings() {
		byte a = 0; // encoding: 21.25 -> 2125 -> 85
		a = abs(floor(minTemperature * 100)) / 25;
		EEPROM.put(EEPROMOffset + 0, a);
		a = abs(floor(maxTemperature * 100)) / 25;
		EEPROM.put(EEPROMOffset + 1, a);
	}
	void readSettings() {
		byte a = 0; // decoding: 85 -> 2125 -> 21.25
		EEPROM.get(EEPROMOffset + 0, a);
		minTemperature = static_cast<float>(a) * 25 / 100;
		EEPROM.get(EEPROMOffset + 1, a);
		maxTemperature = static_cast<float>(a) * 25 / 100;
	}
	bool isHeating() {
		return ioExpander.digitalRead(pin) == LOW;
	}
	void heatOn() {
		ioExpander.digitalWrite(pin, LOW);
	}
	void heatOff() {
		ioExpander.digitalWrite(pin, HIGH);
	}

	void update(float temperature) {
		if (maxTemperature <= temperature) {
			heatOff();
			LOG_DEBUG(Heating, "Heat off (max %.2f <= %.2f current)", maxTemperature, temperature);
		}
		else if (temperature <= minTemperature) {
			heatOn();
			LOG_DEBUG(Heating, "Heat on (current %.2f <= %.2f min)", temperature, minTemperature);
		}
	}
	void setup() {
		readSettings();
	}
};
