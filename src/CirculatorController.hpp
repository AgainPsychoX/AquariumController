#pragma once

#include "common.hpp"

namespace Circulator {
	constexpr byte ioExpanderPin = 3;

	bool active = false;

	void set(bool active) {
		Circulator::active = active;
		ioExpander.digitalWrite(ioExpanderPin, !active);
	}

	void printSettings() {
		LOG_INFO(Circulator, 
			"Settings: %lu ms active, %lu ms pause.", 
			settings->circulator.activeTime, settings->circulator.pauseTime
		);
	}

	void update(unsigned long& currentMillis) {
		const unsigned long delay = active ? settings->circulator.activeTime : settings->circulator.pauseTime;
		static unsigned long lastUpdateMillis = 0;
		if (currentMillis - lastUpdateMillis > delay) { // 1024 (division by power of 2 is much faster)
			currentMillis = lastUpdateMillis = millis();

			if (settings->circulator.activeTime == 0) {
				set(false);
				return;
			}
			if (settings->circulator.pauseTime == 0) {
				set(true);
				return;
			}

			set(!active);
		}
	}

	void setup() {
		printSettings();
	}
};
