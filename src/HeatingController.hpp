#pragma once // Note: Included directly in certain place.

#include <Arduino.h>
#include <EEPROM.h>

namespace HeatingController {
	constexpr byte pin = 0;
	constexpr unsigned int EEPROMOffset = 0x00A;

	float maxTemperature = 21.75f;
	float minTemperature = 24.25f;

	inline void saveSettings() {
		byte a = 0; // encoding: 21.25 -> 2125 -> 85
		a = abs(floor(minTemperature * 100)) / 25;
		EEPROM.put(EEPROMOffset + 0, a);
		a = abs(floor(maxTemperature * 100)) / 25;
		EEPROM.put(EEPROMOffset + 1, a);
	}
	inline void readSettings() {
		byte a = 0; // decoding: 85 -> 2125 -> 21.25
		EEPROM.get(EEPROMOffset + 0, a);
		minTemperature = static_cast<float>(a) * 25 / 100;
		EEPROM.get(EEPROMOffset + 1, a);
		maxTemperature = static_cast<float>(a) * 25 / 100;
	}
	inline bool isHeating() {
		return ioExpander.digitalRead(pin) == LOW;
	}
	inline void heatOn() {
		ioExpander.digitalWrite(pin, LOW);
	}
	inline void heatOff() {
		ioExpander.digitalWrite(pin, HIGH);
	}

	inline void update(float temperature) {
		if (maxTemperature <= temperature) {
			heatOff();
#if DEBUG_HEATING_CONTROLLER >= 1
			Serial.print(F("HeatingController::update() Heat off ("));
			Serial.print(maxTemperature);
			Serial.print(" <= ");
			Serial.print(temperature);
			Serial.println(") ");
#endif
		}
		else if (temperature <= minTemperature) {
			heatOn();
#if DEBUG_HEATING_CONTROLLER >= 1
			Serial.print(F("HeatingController::update() Heat on! ("));
			Serial.print(minTemperature);
			Serial.print(" <= ");
			Serial.print(temperature);
			Serial.println(") ");
#endif
		}
	}
	inline void setup() {
		readSettings();
	}
};
