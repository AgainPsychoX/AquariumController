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

	signed char Entry::compareHours(byte hour, byte minute, byte second) const {
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
			// this->red == other.red &&
			// this->green == other.green &&
			// this->blue == other.blue &&
			// this->white == other.white
		);
	}

	/// Allocates C-style string representing entry data.
	/// Format: `[255, 255, 255, 255] @ 12:00`.
	std::unique_ptr<char[]> Entry::toCString() const {
		char* buf = new char[32]; 
		snprintf(
			buf, 32, 
			"[%3u, %3u, %3u, %3u] @ %02u:%02u", 
			red, green,blue, white,
			hour, minute
		);
		return std::unique_ptr<char[]>(buf);
	}

	const Entry* nextEntry;
	const Entry* previousEntry;
	uint32_t nextTime;
	uint32_t previousTime;
	uint8_t nextEntryIndex;
	bool disable = false;

	void resetToDefaultSettings() {
		// 0% about midnight
		settings->dayCycleEntries[0] = {
			.hour = 0, .minute = 0,
			.red = 0, .green = 0, .blue = 0, .white = 0,
		};
		// 100% about noon
		settings->dayCycleEntries[1] = {
			.hour = 12, .minute = 0,
			.red = 255, .green = 255, .blue = 255, .white = 255,
		};
		// Rest as invalid
		for (uint8_t i = 2; i < maxEntriesCount; i++) {
			settings->dayCycleEntries[i] = { .hour = 255 };
		}
	}

	void setup() {
		if (CHECK_LOG_LEVEL(Lighting, LEVEL_INFO)) {
			LOG_INFO(Lighting, "Setuping... Entries:");
			for (uint8_t i = 0; i < maxEntriesCount; i++) {
				const Entry& entry = getEntryFromSettings(i);
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
		for (uint8_t i = 0; i < maxEntriesCount; i++) {
			const Entry& entry = getEntryFromSettings(i);
			if (!entry.isValid()) {
				continue;
			}
			if (entry.compareHours(now.hour(), now.minute(), now.second()) > 0) {
				nextFound = true;
				nextEntry = &entry;
				nextEntryIndex = i;
				break;
			}
			previousFound = true;
			previousEntry = &entry;
		}

		// TODO: next found but after day change; use first valid

		if (!nextFound) {
			// If not found, set next entry as first entry of cycle. 
			// In such case, last entry should be the last entry of cycle.
			for (uint8_t i = 0; i < maxEntriesCount; i++) {
				const Entry& entry = getEntryFromSettings(i);
				if (!entry.isValid()) {
					continue;
				}
				if (!nextFound) {
					nextFound = true;
					nextEntry = &entry;
					nextEntryIndex = i;
				}
				previousFound = true;
				previousEntry = &entry;
			}
		}

		if (!nextFound) {
			LOG_WARN(Lighting, "No valid entries, disabling.");
			disable = true;
			return;
		}

		// Find entry before selected next entry, to be previous entry.
		if (!previousFound) {
			uint8_t index = nextEntryIndex;
			while (true) {
				const Entry& entry = getEntryFromSettings((++index) % maxEntriesCount);
				if (!entry.isValid()) {
					continue;
				}
				if (*nextEntry == entry) {
					break;
				}
				previousFound = true;
				previousEntry = &entry;
			}
		}

		// If there is only one entry...
		if (!previousFound) {
			LOG_DEBUG(Lighting, "Only one valid entry (will result in static color).");
			previousEntry = nextEntry;
		}

		// Calculate timepoints
		nextTime = nextEntry->getTimepointScheduledAfter(now);
		previousTime = previousEntry->getTimepointScheduledBefore(now);

		LOG_DEBUG(Lighting, "Previous entry: %s (%us ago)", previousEntry->toCString().get(), previousTime);
		LOG_DEBUG(Lighting, "Next entry:     %s (in %us)",  previousEntry->toCString().get(), nextTime);
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
				nextEntry = &getEntryFromSettings((++nextEntryIndex) % maxEntriesCount);
			}
			while (!nextEntry->isValid());
			nextTime = nextEntry->getTimepointScheduledAfter(now);

			LOG_DEBUG(Lighting, "Next entry: %s (in %us)",  previousEntry->toCString().get(), nextTime);
		}

		// Update values
		float ratio = static_cast<float>(currentTime - previousTime) / (nextTime - previousTime);

		LOG_TRACE(Lighting, "ratio: %5.3f = %u / %u", ratio, currentTime - previousTime, nextTime - previousTime);

		if (previousEntry->red == nextEntry->red) {
			redPWM.set(previousEntry->red);
		}
		else {
			redPWM.set(round(previousEntry->red * (1 - ratio) + nextEntry->red * (ratio)));
		}
		if (previousEntry->green == nextEntry->green) {
			greenPWM.set(round(previousEntry->green));
		}
		else {
			greenPWM.set(round(previousEntry->green * (1 - ratio) + nextEntry->green * (ratio)));
		}
		if (previousEntry->blue == nextEntry->blue) {
			bluePWM.set(round(previousEntry->blue));
		}
		else {
			bluePWM.set(round(previousEntry->blue * (1 - ratio) + nextEntry->blue * (ratio)));
		}
		if (previousEntry->white == nextEntry->white) {
			whitePWM.set(round(previousEntry->white));
		}
		else {
			whitePWM.set(round(previousEntry->white * (1 - ratio) + nextEntry->white * (ratio)));
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

		// Format should be like: "#R,G,B,W@HH:mm" (repeated up to 16 times),
		// where R,G,B,W are colors values (0-255) and HH:mm is timepoint (hours & minutes).
		// Note: There is no check is it sorted, but this is assumption!
		unsigned short entriesCount = 0;
		do {
			Entry& entry = getEntryFromSettingsMutable(entriesCount++);
			offset += 1; // #
			entry.red = atoi(cstr + offset);
			if (entry.red > 99) offset += 4; // 123,
			else if (entry.red > 9) offset += 3; // 12,
			else offset += 2; // 1,
			entry.green = atoi(cstr + offset);
			if (entry.green > 99) offset += 4; // 123,
			else if (entry.green > 9) offset += 3; // 12,
			else offset += 2; // 1,
			entry.blue = atoi(cstr + offset);
			if (entry.blue > 99) offset += 4; // 123,
			else if (entry.blue > 9) offset += 3; // 12,
			else offset += 2; // 1,
			entry.white = atoi(cstr + offset);
			if (entry.white > 99) offset += 4; // 123@
			else if (entry.white > 9) offset += 3; // 12@
			else offset += 2; // 1@
			entry.hour = atoi(cstr + offset);
			offset += 3; // 12: 
			entry.minute = atoi(cstr + offset);
			offset += 2; // 12

			LOG_DEBUG(Lighting, "handleSetColors() Entry[%2u] -> %s", entriesCount, entry.toCString().get());

			if (offset >= length) {
				LOG_TRACE(Lighting, "handleSetColors() offset >= length (%u >= %u)", offset, length);
				break;
			}
		}
		while (entriesCount < maxEntriesCount);

		// Fill rest with invalid 
		for (uint8_t i = entriesCount; i < maxEntriesCount; i++) {
			getEntryFromSettingsMutable(i).invalidate();
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
		uint8_t entriesCount = 0;
		for (uint8_t i = 0; i < maxEntriesCount; i++) {
			const Entry& entry = getEntryFromSettings(i);
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
				entry.red,
				entry.green,
				entry.blue,
				entry.white,
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
		resetToDefaultSettings();
		setup();
		webServer.send(200);
	}
}
