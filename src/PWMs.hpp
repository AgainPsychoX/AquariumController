#pragma once // Note: Included directly in certain place.

#include <Arduino.h>
#include <EEPROM.h>

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
PWM<13> redPWM;
PWM<15> greenPWM;
PWM<12> bluePWM;
PWM<14> whitePWM;



////////////////////////////////////////////////////////////////////////////////

// Smooth timed controller
namespace SmoothTimedPWM {
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

		uint32_t getTimepointSchleudedAfter(const DateTime& dateTime) const {
			uint32_t timepoint = dateTime.unixtime();
			signed char hourDiff = static_cast<signed char>(this->hour) - dateTime.hour();
			signed char minuteDiff = static_cast<signed char>(this->minute) - dateTime.minute();
			if (hourDiff < 0 || (hourDiff == 0 && minuteDiff < 0)) {
				hourDiff += 24;
			}
			timepoint += (hourDiff * 60 + minuteDiff) * 60;
			return timepoint;
		}

		uint32_t getTimepointSchleudedBefore(const DateTime& dateTime) const {
			uint32_t timepoint = dateTime.unixtime();
			signed char hourDiff = static_cast<signed char>(this->hour) - dateTime.hour();
			signed char minuteDiff = static_cast<signed char>(this->minute) - dateTime.minute();
			if (hourDiff > 0 || (hourDiff == 0 && minuteDiff > 0)) {
				hourDiff -= 24;
			}
			timepoint += (hourDiff * 60 + minuteDiff) * 60;
			return timepoint;
		}

		signed char compareHours(byte hour, byte minute, byte second) {
			if (this->hour > hour) return 1;
			if (this->hour < hour) return -1;
			if (this->minute > minute) return 1;
			if (this->minute < minute) return -1;
			if (0 < second) return -1;
			return 0;
		}

		bool operator==(const Entry& other) const {
			return (
				this->hour == other.hour &&
				this->minute == other.minute// &&
				// this->redValue == other.redValue &&
				// this->greenValue == other.greenValue &&
				// this->blueValue == other.blueValue &&
				// this->whiteValue == other.whiteValue
			);
		}

#if DEBUG_PWM_SMOOTH_TIMED_CONTROLLER >= 1
		void printOut() const {
			Serial.print("[");
			if (this->redValue <= 99) Serial.print(' ');
			if (this->redValue <= 9)  Serial.print(' ');
			Serial.print(this->redValue);
			Serial.print(", ");
			if (this->greenValue <= 99) Serial.print(' ');
			if (this->greenValue <= 9)  Serial.print(' ');
			Serial.print(this->greenValue);
			Serial.print(", ");
			if (this->blueValue <= 99) Serial.print(' ');
			if (this->blueValue <= 9)  Serial.print(' ');
			Serial.print(this->blueValue);
			Serial.print(", ");
			if (this->whiteValue <= 99) Serial.print(' ');
			if (this->whiteValue <= 9)  Serial.print(' ');
			Serial.print(this->whiteValue);
			Serial.print("] @ ");
			if (this->hour <= 9) Serial.print('0');
			Serial.print(this->hour);
			Serial.print(':');
			if (this->minute <= 9) Serial.print('0');
			Serial.print(this->minute);
		}
#endif

		inline bool isValid() const {
			return hour < 24;
		}

		const static Entry invalid;
	};
	const Entry Entry::invalid {99, 99, 99, 99, 99, 99};
	static_assert(sizeof(Entry) == 8, "Structures saved on EEPROM must have guaranted size!");

	Entry nextEntry;
	Entry previousEntry;
	uint32_t nextTime;
	uint32_t previousTime;
	unsigned short nextEntryIndex;
	bool disable = false;

	void reset() {
		// 0% about midnight
		EEPROM.put(entriesEEPROMOffset + 0 * sizeof(Entry), Entry{0, 0, 0, 0, 0, 0});
		// 100% about noon
		EEPROM.put(entriesEEPROMOffset + 1 * sizeof(Entry), Entry{12, 0, 255, 255, 255, 255});
		// Rest as invalid
		for (unsigned int i = 2; i < maxEntriesCount; i++) {
			EEPROM.put(entriesEEPROMOffset + i * sizeof(Entry), Entry::invalid);
		}
	}

	void setup() {
#if DEBUG_PWM_SMOOTH_TIMED_CONTROLLER >= 1
		Serial.println(F("SmoothTimedPWM::setup() entries list: "));
		for (unsigned int i = 0; i < maxEntriesCount; i++) {
			Serial.print(i);
			Serial.print('\t');
			Entry entry;
			EEPROM.get(entriesEEPROMOffset + i * sizeof(Entry), entry);
			if (!entry.isValid()) {
				Serial.println(F("invalid"));
				continue;
			}
			entry.printOut();
			Serial.println();
		}
#endif
		DateTime now = rtc.now();
		bool nextFound = false;
		bool previousFound = false;

		// Find next entry after current time
		for (unsigned int i = 0; i < maxEntriesCount; i++) {
			Entry entry;
			EEPROM.get(entriesEEPROMOffset + i * sizeof(Entry), entry);
			if (!entry.isValid()) {
				continue;
			}
			if (entry.compareHours(now.hour(), now.minute(), now.second()) > 0) {
				nextFound = true;
				nextEntry = entry;
				nextEntryIndex = i;
				break;
			}
			previousFound = true;
			previousEntry = entry;
		}

		// TODO: next found but after day change; use first valid

		if (!nextFound) {
			// If not found, set next entry as first entry of cycle. 
			// In such case, last entry should be the last entry of cycle.
			for (unsigned int i = 0; i < maxEntriesCount; i++) {
				Entry entry;
				EEPROM.get(entriesEEPROMOffset + i * sizeof(Entry), entry);
				if (!entry.isValid()) {
					continue;
				}
				if (!nextFound) {
					nextFound = true;
					nextEntry = entry;
					nextEntryIndex = i;
				}
				previousFound = true;
				previousEntry = entry;
			}
		}

		if (!nextFound) {
			Serial.print(F("SmoothTimedPWM::setup() No valid entries, disabling. You might want use reset!"));
			disable = true;
			return;
		}

		// Find entry before selected next entry, to be previous entry.
		if (!previousFound) {
			unsigned short index = nextEntryIndex;
			while (true) {
				Entry entry;
				EEPROM.get(entriesEEPROMOffset + ((++index) % maxEntriesCount) * sizeof(Entry), entry);
				if (!entry.isValid()) {
					continue;
				}
				if (entry == nextEntry) {
					break;
				}
				previousFound = true;
				previousEntry = entry;
			}
		}

		// If there is only one entry...
		if (!previousFound) {
			Serial.print(F("SmoothTimedPWM::setup() Only one valid entry...? Okay"));
			previousEntry = nextEntry;
		}

		// Calculate timepoints
		nextTime = nextEntry.getTimepointSchleudedAfter(now);
		previousTime = previousEntry.getTimepointSchleudedBefore(now);

#if DEBUG_PWM_SMOOTH_TIMED_CONTROLLER >= 1
		Serial.print(F("SmoothTimedPWM::setup() previous entry: "));
		previousEntry.printOut();
		Serial.print(" (in ");
		Serial.print(previousTime);
		Serial.println("s)");
		Serial.print(F("SmoothTimedPWM::setup() next entry: "));
		nextEntry.printOut();
		Serial.print(" (in ");
		Serial.print(nextTime);
		Serial.println("s)");
#endif
	}

	void update() {
		if (disable) {
			return;
		}

		DateTime now = rtc.now();
		uint32_t currentTime = now.unixtime();

		if (nextTime < currentTime) {
			previousEntry = nextEntry;
			previousTime = nextTime;
			do {
				EEPROM.get(entriesEEPROMOffset + ((++nextEntryIndex) % maxEntriesCount) * sizeof(Entry), nextEntry);
			}
			while (!nextEntry.isValid());
			nextTime = nextEntry.getTimepointSchleudedAfter(now);

#if DEBUG_PWM_SMOOTH_TIMED_CONTROLLER >= 2
			Serial.print(F("SmoothTimedPWM::update() next entry: "));
			nextEntry.printOut();
			Serial.print(" (in ");
			Serial.print(nextTime);
			Serial.println("s)");
#endif
		}

		// Update values
		float ratio = static_cast<float>(currentTime - previousTime) / (nextTime - previousTime);
#if DEBUG_PWM_SMOOTH_TIMED_CONTROLLER >= 3
		Serial.print(F("SmoothTimedPWM::update() ratio: "));
		Serial.print(ratio);
		Serial.print(" = ");
		Serial.print(currentTime - previousTime);
		Serial.print(" / ");
		Serial.println(nextTime - previousTime);
#endif
		if (previousEntry.redValue == nextEntry.redValue) {
			redPWM.set(previousEntry.redValue);
		}
		else {
			redPWM.set(round(previousEntry.redValue * (1 - ratio) + nextEntry.redValue * (ratio)));
		}
		if (previousEntry.greenValue == nextEntry.greenValue) {
			greenPWM.set(round(previousEntry.greenValue));
		}
		else {
			greenPWM.set(round(previousEntry.greenValue * (1 - ratio) + nextEntry.greenValue * (ratio)));
		}
		if (previousEntry.blueValue == nextEntry.blueValue) {
			bluePWM.set(round(previousEntry.blueValue));
		}
		else {
			bluePWM.set(round(previousEntry.blueValue * (1 - ratio) + nextEntry.blueValue * (ratio)));
		}
		if (previousEntry.whiteValue == nextEntry.whiteValue) {
			whitePWM.set(round(previousEntry.whiteValue));
		}
		else {
			whitePWM.set(round(previousEntry.whiteValue * (1 - ratio) + nextEntry.whiteValue * (ratio)));
		}
	}

	void pushColorsHandler() {
		const char* cstr = server.arg("plain").c_str();
		unsigned short length = strlen(cstr);
		unsigned short offset = 0;
#if DEBUG_PWM_SMOOTH_TIMED_CONTROLLER >= 2
		Serial.print(F("SmoothTimedPWM::pushColorsHandler() length: "));
		Serial.println(length);
		Serial.print(F("SmoothTimedPWM::pushColorsHandler() content: "));
		Serial.println(cstr);
#endif
		if (length < 14) {
			server.send(500, WEB_CONTENT_TYPE_TEXT_PLAIN, F("Not enough data sent"));
			return;
		}
		// Format should be like: "#R,G,B,W@HH:mm" (repeated up to 14 times),
		// where R,G,B,W are colors values (0-255) and HH:mm is timepoint (hours & minutes).
		// Note: There is no check is it sorted, but this is assumption!
		unsigned short entriesCount = 0;
		for (unsigned int i = 0; i < maxEntriesCount; i++) {
			Entry entry;
			offset += 1; // #
			entry.redValue = atoi(cstr + offset);
			if (entry.redValue > 99) offset += 4; // 123,
			else if (entry.redValue > 9) offset += 3; // 12,
			else offset += 2; // 1,
			entry.greenValue = atoi(cstr + offset);
			if (entry.greenValue > 99) offset += 4; // 123,
			else if (entry.greenValue > 9) offset += 3; // 12,
			else offset += 2; // 1,
			entry.blueValue = atoi(cstr + offset);
			if (entry.blueValue > 99) offset += 4; // 123,
			else if (entry.blueValue > 9) offset += 3; // 12,
			else offset += 2; // 1,
			entry.whiteValue = atoi(cstr + offset);
			if (entry.whiteValue > 99) offset += 4; // 123@
			else if (entry.whiteValue > 9) offset += 3; // 12@
			else offset += 2; // 1@
			entry.hour = atoi(cstr + offset);
			offset += 3; // 12: 
			entry.minute = atoi(cstr + offset);
			offset += 2; // 12
			EEPROM.put(entriesEEPROMOffset + i * sizeof(Entry), entry);
			entriesCount++;
#if DEBUG_PWM_SMOOTH_TIMED_CONTROLLER >= 2
			Serial.print(F("SmoothTimedPWM::pushColorsHandler() Entry["));
			Serial.print(i);
			Serial.print(F("] = "));
			entry.printOut();
			Serial.println();
#endif
			if (offset >= length) {
#if DEBUG_PWM_SMOOTH_TIMED_CONTROLLER >= 2
				Serial.println(F("SmoothTimedPWM::pushColorsHandler() offset >= length ("));
				Serial.print(offset);
				Serial.print(" >= ");
				Serial.println(length);
#endif
				break;
			}
		}

#if DEBUG_PWM_SMOOTH_TIMED_CONTROLLER >= 2
		Serial.print(F("SmoothTimedPWM::pushColorsHandler() entries count: "));
		Serial.println(entriesCount);
#endif

		// Fill rest with invalid 
		for (unsigned int i = entriesCount; i < maxEntriesCount; i++) {
			EEPROM.put(entriesEEPROMOffset + i * sizeof(Entry), Entry::invalid);
		}

		if (entriesCount == maxEntriesCount && offset < length - 4) {
			Serial.println(F("SmoothTimedPWM::pushColorsHandler() Too much entries sent, discarding"));
		}

		// Run setup to update after change
		setup();

		server.send(200);
	}
	void getColorsHandler() {
		constexpr unsigned int buffLen = 1024;
		char response[buffLen];
		unsigned short offset = 0;
		response[offset++] = '[';
		unsigned short entriesCount = 0;
		for (unsigned int i = 0; i < maxEntriesCount; i++) {
			Entry entry;
			EEPROM.get(entriesEEPROMOffset + i * sizeof(Entry), entry);
			if (!entry.isValid()) {
				continue;
			}
			unsigned int max = buffLen - offset - 3; // 3 for `,`, `]` and 0.
			int ret = snprintf(
				response + offset, max,
				"{"
					"\"red\":%d,"
					"\"green\":%d,"
					"\"blue\":%d,"
					"\"white\":%d,"
					"\"time\":\"%02d:%02d\""
				"}",
				entry.redValue,
				entry.greenValue,
				entry.blueValue,
				entry.whiteValue,
				entry.hour, entry.minute
			);
			entriesCount++;
			if (ret < 0 || static_cast<unsigned int>(ret) >= max) {
				server.send(500, WEB_CONTENT_TYPE_TEXT_PLAIN, F("Response buffer exceeded"));
				return;
			}
			offset += ret;

			response[offset++] = ',';
		}
		if (entriesCount > 0) {
			response[offset - 1] = ']'; // Overwrite last `,`
		}
		else {
			// TODO: Add fake entry for proper display
			response[offset++] = ']';
		}
		response[offset++] = 0;
		server.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, response);
	}
	void resetColorsHandler() {
		reset();
		setup();
		server.send(200);
	}
}
