#pragma once // Note: Included directly in certain place.

#include <Arduino.h>
#include <EEPROM.h>

namespace CirculatorController {
	constexpr byte ioExpanderPin = 3;
	constexpr unsigned int EEPROMOffset = 0x00C;

	unsigned short activeSeconds = 0;
	unsigned short pauseSeconds = 255;
	bool active = false;
	unsigned long int nextUpdateTime = 0;

	inline void saveSettings() {
		EEPROM.put(EEPROMOffset + 0, activeSeconds);
		EEPROM.put(EEPROMOffset + 2, pauseSeconds);
	}
	inline void readSettings() {
		EEPROM.get(EEPROMOffset + 0, activeSeconds);
		EEPROM.get(EEPROMOffset + 2, pauseSeconds);
	}
	inline bool isActive() {
		return active;
	}

	inline void update() {
		if (pauseSeconds == 0) {
			active = true;
			ioExpander.digitalWrite(ioExpanderPin, LOW);
			return;
		}
		if (activeSeconds == 0) {
			active = false;
			ioExpander.digitalWrite(ioExpanderPin, HIGH);
			return;
		}
		unsigned long int currentTime = millis();
		if (currentTime > nextUpdateTime) {
			if (active) {
				nextUpdateTime = currentTime + pauseSeconds * 1000;
				active = false;
				ioExpander.digitalWrite(ioExpanderPin, HIGH);
#if DEBUG_CIRCULATOR_CONTROLLER >= 1
				Serial.print(F("CirculatorController::update() pause until "));
				Serial.println(nextUpdateTime);
#endif
			}
			else {
				nextUpdateTime = currentTime + activeSeconds * 1000;
				active = true;
				ioExpander.digitalWrite(ioExpanderPin, LOW);
#if DEBUG_CIRCULATOR_CONTROLLER >= 1
				Serial.print(F("CirculatorController::update() active until "));
				Serial.println(nextUpdateTime);
#endif
			}
		}
	}
	inline void setup() {
		readSettings();
	}
};
