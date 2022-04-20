#pragma once

#include "common.hpp"

namespace Heating {
	constexpr byte pin = 0;

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
		if (settings->temperatures.maximal <= temperature) {
			heatOff();
			LOG_DEBUG(Heating, "Heat off (max %.2f <= %.2f current)", settings->temperatures.maximal, temperature);
		}
		else if (temperature <= settings->temperatures.minimal) {
			heatOn();
			LOG_DEBUG(Heating, "Heat on (current %.2f <= %.2f min)", temperature, settings->temperatures.minimal);
		}
	}
	void setup() {
		// Nothing
	}
};
