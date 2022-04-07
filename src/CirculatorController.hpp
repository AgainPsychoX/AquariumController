#pragma once

#include "common.hpp"

namespace Circulator {
	constexpr byte ioExpanderPin = 3;

	uint16_t activeSeconds;
	uint16_t pauseSeconds;
	bool active = false;

	void set(bool active) {
		Circulator::active = active;
		ioExpander.digitalWrite(ioExpanderPin, !active);
	}

	constexpr unsigned int EEPROMOffset = 0x00C;

	void saveSettings() {
		EEPROM.put(EEPROMOffset + 0 * sizeof(uint16_t), activeSeconds);
		EEPROM.put(EEPROMOffset + 1 * sizeof(uint16_t), pauseSeconds);
	}
	void readSettings() {
		EEPROM.get(EEPROMOffset + 0 * sizeof(uint16_t), activeSeconds);
		EEPROM.get(EEPROMOffset + 1 * sizeof(uint16_t), pauseSeconds);
	}
	void printSettings() {
		LOG_INFO(Circulator, "Settings: %u seconds active, %u seconds pause.", activeSeconds, pauseSeconds);
	}

	void update(unsigned long& currentMillis) {
		static unsigned long lastUpdateMillis = 0;
		if ((currentMillis - lastUpdateMillis) / 1000 > (active ? activeSeconds : pauseSeconds)) {
			currentMillis = lastUpdateMillis = millis();

			if (activeSeconds == 0) {
				set(false);
				return;
			}
			if (pauseSeconds == 0) {
				set(true);
				return;
			}

			set(!active);
		}
	}

	void setup() {
		readSettings();
		printSettings();
	}
};
