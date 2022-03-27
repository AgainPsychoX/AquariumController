#pragma once

#include "common.hpp"

namespace Circulator {
	constexpr byte ioExpanderPin = 3;
	constexpr unsigned int EEPROMOffset = 0x00C;

	unsigned short activeSeconds = 0;
	unsigned short pauseSeconds = 255;
	bool active = false;
	unsigned long int nextUpdateTime = 0;

	void saveSettings() {
		EEPROM.put(EEPROMOffset + 0, activeSeconds);
		EEPROM.put(EEPROMOffset + 2, pauseSeconds);
	}
	void readSettings() {
		EEPROM.get(EEPROMOffset + 0, activeSeconds);
		EEPROM.get(EEPROMOffset + 2, pauseSeconds);
	}
	inline bool isActive() {
		return active;
	}

	void update() {
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
				LOG_DEBUG(Circulator, "pause util %u", nextUpdateTime);
			}
			else {
				nextUpdateTime = currentTime + activeSeconds * 1000;
				active = true;
				ioExpander.digitalWrite(ioExpanderPin, LOW);
				LOG_DEBUG(Circulator, "active util %u", nextUpdateTime);
			}
		}
	}
	void setup() {
		readSettings();
	}
};
