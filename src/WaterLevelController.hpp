#pragma once

#include "common.hpp"

namespace WaterLevel {
	// Pins (on the IO extender)
	constexpr byte detectorPin = 7;
	constexpr byte refillerPin = 1;
	constexpr byte backupTankDetectorPin = 6;

	bool refillingRequired = false;
	bool backupTankLow = false;

	void update() {
		refillingRequired = ioExpander.digitalRead(detectorPin) == HIGH;
		ioExpander.digitalWrite(refillerPin, refillingRequired ? LOW : HIGH);

		backupTankLow = ioExpander.digitalRead(backupTankDetectorPin) == HIGH;
	}
}
