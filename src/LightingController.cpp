#include "LightingController.hpp"

PWM<13> redPWM;
PWM<15> greenPWM;
PWM<12> bluePWM;
PWM<14> whitePWM;

// Smooth timed controller
namespace Lighting {
	uint32_t Entry::getTimepointScheduledAfter(const DateTime& dateTime) const {
		uint32_t timepoint = dateTime.unixtime();
		signed char hourDiff = static_cast<signed char>(this->hour) - dateTime.hour();
		signed char minuteDiff = static_cast<signed char>(this->minute) - dateTime.minute();
		if (hourDiff < 0 || (hourDiff == 0 && minuteDiff < 0)) {
			hourDiff += 24;
		}
		timepoint += (hourDiff * 60 + minuteDiff) * 60;
		return timepoint;
	}

	uint32_t Entry::getTimepointScheduledBefore(const DateTime& dateTime) const {
		uint32_t timepoint = dateTime.unixtime();
		signed char hourDiff = static_cast<signed char>(this->hour) - dateTime.hour();
		signed char minuteDiff = static_cast<signed char>(this->minute) - dateTime.minute();
		if (hourDiff > 0 || (hourDiff == 0 && minuteDiff > 0)) {
			hourDiff -= 24;
		}
		timepoint += (hourDiff * 60 + minuteDiff) * 60;
		return timepoint;
	}

	signed char Entry::compareHours(byte hour, byte minute, byte second) {
		if (this->hour > hour) return 1;
		if (this->hour < hour) return -1;
		if (this->minute > minute) return 1;
		if (this->minute < minute) return -1;
		if (0 < second) return -1;
		return 0;
	}

	bool Entry::operator==(const Entry& other) const {
		return (
			this->hour == other.hour &&
			this->minute == other.minute// &&
			// this->redValue == other.redValue &&
			// this->greenValue == other.greenValue &&
			// this->blueValue == other.blueValue &&
			// this->whiteValue == other.whiteValue
		);
	}

	/// Allocates C-style string representing entry data.
	/// Format: `[255, 255, 255, 255] @ 12:00`.
	std::unique_ptr<char[]> Entry::toCString() const {
		char* buf = new char[32]; 
		snprintf(
			buf, 32, 
			"[%3u, %3u, %3u, %3u] @ %02u:%02u", 
			redValue, greenValue,blueValue, whiteValue,
			hour, minute
		);
		return std::unique_ptr<char[]>(buf);
	}

	inline bool Entry::isValid() const {
		return hour < 24;
	}

	const Entry Entry::invalid {99, 99, 99, 99, 99, 99};

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
		if (CHECK_LOG_LEVEL(Lighting, LEVEL_INFO)) {
			LOG_INFO(Lighting, "Setuping... Entries:");
			for (unsigned int i = 0; i < maxEntriesCount; i++) {
				Entry entry;
				EEPROM.get(entriesEEPROMOffset + i * sizeof(Entry), entry);
				if (entry.isValid()) {
					LOG_INFO(Lighting, "%2u. %s", i, entry.toCString().get());
				}
				else {
					LOG_INFO(Lighting, "%2u. invalid", i);
				}
			}
		}

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
			LOG_WARN(Lighting, "No valid entries, disabling.");
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
			LOG_DEBUG(Lighting, "Only one valid entry (will result in static color).");
			previousEntry = nextEntry;
		}

		// Calculate timepoints
		nextTime = nextEntry.getTimepointScheduledAfter(now);
		previousTime = previousEntry.getTimepointScheduledBefore(now);

		LOG_DEBUG(Lighting, "Previous entry: %s (%us ago)", previousEntry.toCString().get(), previousTime);
		LOG_DEBUG(Lighting, "Next entry:     %s (in %us)",  previousEntry.toCString().get(), nextTime);
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
			nextTime = nextEntry.getTimepointScheduledAfter(now);

			LOG_DEBUG(Lighting, "Next entry: %s (in %us)",  previousEntry.toCString().get(), nextTime);
		}

		// Update values
		float ratio = static_cast<float>(currentTime - previousTime) / (nextTime - previousTime);

		LOG_TRACE(Lighting, "ratio: %5.3f = %u / %u", ratio, currentTime - previousTime, nextTime - previousTime);

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

	void handleSetColors() {
		const char* cstr = webServer.arg("plain").c_str();
		unsigned short length = strlen(cstr);
		unsigned short offset = 0;

		LOG_DEBUG(Lighting, "handleSetColors() length: %u, content: `%s`", length, cstr);

		if (length < 14) {
			webServer.send(500, WEB_CONTENT_TYPE_TEXT_PLAIN, F("Not enough data sent"));
			return;
		}

		// Format should be like: "#R,G,B,W@HH:mm" (repeated up to 14 times),
		// where R,G,B,W are colors values (0-255) and HH:mm is timepoint (hours & minutes).
		// Note: There is no check is it sorted, but this is assumption!
		unsigned short entriesCount = 0;
		do {
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

			EEPROM.put(entriesEEPROMOffset + entriesCount * sizeof(Entry), entry);
			entriesCount++;

			LOG_DEBUG(Lighting, "handleSetColors() Entry[%2u] -> %s", entriesCount, entry.toCString().get());

			if (offset >= length) {
				LOG_TRACE(Lighting, "handleSetColors() offset >= length (%u >= %u)", offset, length);
				break;
			}
		}
		while (entriesCount < maxEntriesCount);

		// Fill rest with invalid 
		for (unsigned int i = entriesCount; i < maxEntriesCount; i++) {
			EEPROM.put(entriesEEPROMOffset + i * sizeof(Entry), Entry::invalid);
		}

		if (entriesCount == maxEntriesCount && offset < length - 4) {
			LOG_DEBUG(Lighting, "handleSetColors() Too much entries sent, discarding");
		}

		// Run setup to update after change
		setup();

		webServer.send(200);
	}

	void handleGetColors() {
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
				webServer.send(500, WEB_CONTENT_TYPE_TEXT_PLAIN, F("Response buffer exceeded"));
				return;
			}
			offset += ret;

			response[offset++] = ',';
		}
		if (entriesCount > 0) {
			response[offset - 1] = ']'; // Overwrite last `,`
		}
		else {
			response[offset++] = ']';
		}
		response[offset++] = 0;
		webServer.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, response);
	}

	void handleResetColors() {
		reset();
		setup();
		webServer.send(200);
	}
}
