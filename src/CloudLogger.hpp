#pragma once

#include "common.hpp"
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

namespace CloudLogger {
	constexpr unsigned int EEPROMOffset = 0x008;
	constexpr unsigned int maxTimeout = 3000;
	constexpr bool discardResponse = true;

	unsigned long int interval = 0;
	unsigned long int parseIntervalFromSetting(uint8_t setting) {
		return static_cast<unsigned long int>((setting & 0b10000000) ? 60 : 1) * (setting & 0b01111111) * 1000;
	}
	uint8_t parseIntervalToSetting(unsigned long int interval) {
		unsigned short int seconds = interval / 1000;
		return seconds <= 127 ? seconds : (0b10000000 & (seconds / 60));
	}

	inline bool isEnabled() {
		return interval != 0;
	}

#if !CLOUDLOGGER_INSECURE
	const char fingerprint[] PROGMEM = "8E:4A:B0:91:50:92:3E:08:A2:CB:65:FD:C1:DF:57:4F:F5:ED:90:BD";
#endif

	struct Entry {
		float waterTemperature;
		float rtcTemperature;
		float phLevel;
	};

	void push(Entry entry) {
		[[maybe_unused]]
		unsigned long int startTime = millis();

		// Prepare SSL client
		WiFiClientSecure sslClient;
		sslClient.setTimeout(maxTimeout);
#if CLOUDLOGGER_INSECURE
		sslClient.setInsecure();
#else
		sslClient.setFingerprint(fingerprint);
#endif

		// Prepare HTTPS client
		HTTPClient httpClient;
		if (!httpClient.begin(sslClient, F("https://script.google.com/macros/s/AKfycbzNjb9VDXi9RayxOderzEnBSd-2OY8O6I-WqUD0YJTDE5wRQWC0/exec"))) {
			LOG_ERROR(CloudLogger, "Unable to connect to cloud server");
			return;
		}
		httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

		// Prepare request body
		DateTime now = rtc.now();
		constexpr unsigned int bufferLength = 128;
		char buffer[bufferLength];
		int writtenLength = snprintf(
			buffer, bufferLength,
			"{"
				"\"rtcTemperature\":%.2f,"
				"\"waterTemperature\":%.2f,"
				"\"phLevel\":%.2f,"
				"\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\""
			"}",
			entry.rtcTemperature,
			entry.waterTemperature,
			entry.phLevel,
			// Format should be like: "2004-02-12T15:19:21" (without time zones)
			now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second()
		);
		if (writtenLength < 0 || static_cast<unsigned int>(writtenLength) >= bufferLength) {
			LOG_ERROR(CloudLogger, "snprintf failed");
			return;
		}

		LOG_DEBUG(CloudLogger, "Logging sample to cloud: %s", buffer);

		// Send POST request
		httpClient.addHeader(F("Content-Type"), WEB_CONTENT_TYPE_APPLICATION_JSON);
		int statusCode = httpClient.POST(reinterpret_cast<uint8_t*>(buffer), writtenLength);
		if (!statusCode || statusCode >= 400) {
			LOG_ERROR(CloudLogger, "Invalid status code: %i", statusCode);
			return;
		}

		if (!discardResponse) {
			// Read response
			const String& payload = httpClient.getString();
			LOG_TRACE(CloudLogger, "Response body: %s", payload.c_str());

			int offset = payload.indexOf(F("samplesCount\":"));
			if (offset == -1) {
				LOG_WARN(CloudLogger, "No or invalid response");
			}
			else {
				int samplesCount = atoi(payload.c_str() + offset);
				LOG_INFO(CloudLogger, "Total %i samples collected", samplesCount);
			}
		}

		LOG_DEBUG(CloudLogger, "Time used %lums", millis() - startTime);

		// Disconnect
		httpClient.end();
		sslClient.stop();
	}


	void readSettings() {
		uint8_t intervalSetting = 0;
		EEPROM.get(EEPROMOffset, intervalSetting);
		interval = parseIntervalFromSetting(intervalSetting);

		if (interval < 10000) {
			LOG_WARN(CloudLogger, "Interval too short, using 10 seconds.");
			interval = 10000;
		}
	}
	void saveSettings() {
		uint8_t intervalSetting = parseIntervalToSetting(interval);
		EEPROM.put(EEPROMOffset, intervalSetting);
	}

	void setup() {
		readSettings();
	}
}
