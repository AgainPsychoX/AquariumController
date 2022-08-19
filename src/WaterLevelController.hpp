#pragma once

#include "common.hpp"

namespace {
	// I guess it could be optimized with some bit manipulation magic...
	constexpr uint8_t countLSBsSet(uint32_t a) {
		uint8_t i = 0;
		while (a & 1) {
			i++;
			a >>= 1;
		}
		return i;
	}
	static_assert(countLSBsSet(0b0101011111) == 5);

	constexpr uint8_t countLSBsClear(uint32_t a) {
		return countLSBsSet(~a);
	}
	static_assert(countLSBsClear(0b1010100000) == 5);
}

namespace WaterLevel {
	template <uint8_t pin, uint8_t satisfiedValue = LOW, bool readCached = true>
	struct DetectorViaIOExpander {
		uint32_t lastReadings;

		inline void setup() {
			lastReadings = this->isSatisfiedNow() ? 0xFFFFFFFF : 0;
		}

		inline bool wasSatisfiedNow() const {
			return lastReadings & 1;
		}

		/**
		 * Reads the input port and returns true whenever detector is satisfied (high level of water).
		 * Doesn't register the reading in last readings.
		 */
		inline bool isSatisfiedNow() const {
			const auto currentValue = readCached
				? ioExpander.digitalReadCached(pin)
				: ioExpander.digitalRead(pin)
			;
			return currentValue == satisfiedValue;
		}

		/**
		 * Reads the input port and returns true whenever detector is satisfied (high level of water).
		 * Registers the reading in last readings.
		 */
		inline bool checkIsSatisfiedNow() {
			const bool state = isSatisfiedNow();
			lastReadings <<= 1;
			lastReadings |= state ? 1 : 0;
			return state;
		}

		inline bool isContinuouslyDissatisfied() const {
			return countLSBsClear(lastReadings) >= 3;
		}
	};

	// Pins (on the IO extender)
	extern DetectorViaIOExpander<7> mainTankDetector;
	extern DetectorViaIOExpander<6> backupTankDetector;
	constexpr byte refillerPin = 1;

	extern bool refillingRequired;
	extern bool backupTankLow;

	/**
	 * Writes the output port of refilling control. If true, starts refilling. If false, stops.
	 */
	void doRefilling(bool state);

	void setup();

	void update();
}
