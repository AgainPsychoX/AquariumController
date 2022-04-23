#pragma once

#include "common.hpp"

namespace Heating {
	constexpr byte coolingPin = D0; // D0
	constexpr byte heatingPin = 0; // on IO expander

	bool coolingActive;

	bool isCooling() {
		return coolingActive;
	}
	void coolingOn() {
		coolingActive = true;
		digitalWrite(coolingPin, HIGH);
	}
	void coolingOff() {
		coolingActive = false;
		digitalWrite(coolingPin, LOW);
	}

	bool isHeating() {
		return ioExpander.digitalReadCached(heatingPin) == LOW;
	}
	void heatingOn() {
		ioExpander.digitalWrite(heatingPin, LOW);
	}
	void heatingOff() {
		ioExpander.digitalWrite(heatingPin, HIGH);
	}

	enum Status : int8_t {
		COOLING = -1,
		NONE = 0,
		HEATING = 1,
	};

	Status getStatus() {
		LOG_TRACE(
			Heating, "Status %s = %d", 
			isHeating() ? "HEATING" : isCooling() ? "COOLING" : "NONE",
			isHeating() ? HEATING : isCooling() ? COOLING : NONE
		);
		return isHeating() ? HEATING : isCooling() ? COOLING : NONE;
	}

	void update(float temperature) {
		if (settings->temperatures.optimal <= temperature) {
			heatingOff();
			if (isHeating()) {
				LOG_DEBUG(Heating, "Heating off (optimal %.2f <= %.2f current)", settings->temperatures.optimal, temperature);
			}
		}
		else if (temperature < settings->temperatures.minimal) {
			heatingOn();
			if (!isHeating()) {
				LOG_DEBUG(Heating, "Heating on (current %.2f <= %.2f minimal)", temperature, settings->temperatures.minimal);
			}
		}

		if (settings->temperatures.maximal < temperature) {
			coolingOn();
			if (!isCooling()) {
				LOG_DEBUG(Heating, "Cooling on (maximal %.2f <= %.2f current)", settings->temperatures.maximal, temperature);
			}
		}
		else if (temperature <= settings->temperatures.optimal) {
			coolingOff();
			if (isCooling()) {
				LOG_DEBUG(Heating, "Cooling off (maximal %.2f <= %.2f current)", settings->temperatures.maximal, temperature);
			}
		}
	}

	void earlySetup() {
		pinMode(coolingPin, OUTPUT);
		coolingOff();
		heatingOff();
	}

	void setup() {
		if (settings->temperatures.optimal <= settings->temperatures.minimal || settings->temperatures.maximal <= settings->temperatures.optimal) {
			settings->temperatures.optimal = (settings->temperatures.maximal + settings->temperatures.minimal) / 2;
		}
	}
};
