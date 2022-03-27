#pragma once

#include "common.hpp"
#include <Arduino.h>
#include <EEPROM.h>
#include <memory> // unique_ptr
#include <DS3231.h> // DateTime

template <uint8_t pin>
struct PWM {
	byte current;

	PWM() {
		pinMode(pin, OUTPUT);
		analogWrite(pin, current); 
	}

	inline void set(byte power) {
		current = power;
		analogWrite(pin, current); 
	}

	inline byte get() {
		return current;
	}
};

// Pulse Width Modulation outputs settings
// PINs
extern PWM<13> redPWM;
extern PWM<15> greenPWM;
extern PWM<12> bluePWM;
extern PWM<14> whitePWM;



////////////////////////////////////////////////////////////////////////////////

// Smooth timed controller

namespace Lighting {
	constexpr unsigned int maxEntriesCount = 14;
	constexpr unsigned int entriesEEPROMOffset = 0x010;

	/// Entry for PWM values in certain timepoint.
	/// Assumption: Entries are sorted in EEPROM.
	struct Entry {
		byte hour; // TODO: "disabled" bit?
		byte minute; // TODO: week day flags by bits?
		byte redValue;
		byte greenValue;
		byte blueValue;
		byte whiteValue;
		byte _pad[2];

		Entry() {}

		Entry(byte hour, byte minute, 
				byte red, byte green, byte blue, byte white)
			: hour(hour), minute(minute), 
				redValue(red), greenValue(green), blueValue(blue), whiteValue(white)
		{}

		uint32_t getTimepointScheduledAfter(const DateTime& dateTime) const;
		uint32_t getTimepointScheduledBefore(const DateTime& dateTime) const;

		signed char compareHours(byte hour, byte minute, byte second);

		bool operator==(const Entry& other) const;

		/// Allocates C-style string representing entry data.
		/// Format: `[255, 255, 255, 255] @ 12:00`.
		std::unique_ptr<char[]> toCString() const;

		inline bool isValid() const;

		const static Entry invalid;
	};
	static_assert(sizeof(Entry) == 8, "Structures saved on EEPROM must have guaranted size!");

	extern Entry nextEntry;
	extern Entry previousEntry;
	extern uint32_t nextTime;
	extern uint32_t previousTime;
	extern unsigned short nextEntryIndex;
	extern bool disable;

	void reset();

	void setup();
	void update();

	void handleSetColors();
	void handleGetColors();
	void handleResetColors();
}
