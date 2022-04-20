#pragma once

#include "common.hpp"
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
	constexpr unsigned int maxEntriesCount = 16;
	static_assert(maxEntriesCount == sizeof(settings->dayCycleEntries) / sizeof(settings->dayCycleEntries[0]));

	/// Entry for PWM values in certain timepoint.
	/// Assumption: Entries are sorted in EEPROM.
	struct Entry : DayCycleEntryData {
		uint32_t getTimepointScheduledAfter(const DateTime& dateTime) const;
		uint32_t getTimepointScheduledBefore(const DateTime& dateTime) const;

		signed char compareHours(byte hour, byte minute, byte second) const;

		bool operator==(const Entry& other) const;

		/// Allocates C-style string representing entry data.
		/// Format: `[255, 255, 255, 255] @ 12:00`.
		std::unique_ptr<char[]> toCString() const;

		inline void invalidate() {
			hour |= 0b10000000; // += 127;
		}
		inline bool isValid() const {
			return hour < 24;
		}
	};
	static_assert(sizeof(Entry) == sizeof(DayCycleEntryData));

	inline const Entry& getEntryFromSettings(uint8_t index) {
		return reinterpret_cast<const Entry&>(settings->dayCycleEntries[index]);
	}
	inline Entry& getEntryFromSettingsMutable(uint8_t index) {
		return reinterpret_cast<Entry&>(settings->dayCycleEntries[index]);
	}

	extern const Entry* nextEntry;
	extern const Entry* previousEntry;
	extern uint32_t nextTime;
	extern uint32_t previousTime;
	extern uint8_t nextEntryIndex;
	extern bool disable;

	void resetToDefaultSettings();

	void setup();
	void update();

	void handleSetColors();
	void handleGetColors();
	void handleResetColors();
}
