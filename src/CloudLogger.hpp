#pragma once // Note: Included directly in certain place.

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

namespace CloudLogger {
	constexpr unsigned int EEPROMOffset = 0x008;
	constexpr unsigned int maxTimeout = 3000;
	constexpr bool discardResponse = true;

	unsigned long int interval = 0;
	inline unsigned long int parseIntervalFromSetting(uint8_t setting) {
		return static_cast<unsigned long int>(setting & 0b10000000 ? 60 : 1) * (setting & 0b01111111);
	}
	inline uint8_t parseIntervalToSetting(unsigned long int interval) {
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
		float airTemperature;
		float phLevel;
	};

	const char SerialLogPrefix_push[] PROGMEM = "CloudLogger::push() ";

	void push(Entry entry) {
#if DEBUG_CLOUD_LOGGER >= 2
		unsigned long int startTime = millis();
#endif

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
			Serial.print(SerialLogPrefix_push);
			Serial.println(F("Unable to connect to cloud server"));
			return;
		}
		httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

		// Prepare request body
		DateTime now = rtc.now();
		constexpr unsigned int bufferLength = 128;
		char dataBuffer[bufferLength];
		int writtenLength = snprintf(
			dataBuffer, bufferLength,
			"{"
				"\"airTemperature\":%.2f,"
				"\"waterTemperature\":%.2f,"
				"\"phLevel\":%.2f,"
				"\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\""
			"}",
			entry.airTemperature,
			entry.waterTemperature,
			entry.phLevel,
			// Format should be like: "2004-02-12T15:19:21" (without time zones)
			now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second()
		);
		if (writtenLength < 0 || static_cast<unsigned int>(writtenLength) >= bufferLength) {
			Serial.print(SerialLogPrefix_push);
			Serial.println(F("Request buffer exceeded"));
			return;
		}

#if DEBUG_CLOUD_LOGGER >= 1
		Serial.print(SerialLogPrefix_push);
		Serial.print(F("Logging sample to cloud: "));
		Serial.println(dataBuffer);
#endif

		// Send POST request
		httpClient.addHeader(F("Content-Type"), WEB_CONTENT_TYPE_APPLICATION_JSON);
		int statusCode = httpClient.POST(reinterpret_cast<uint8_t*>(dataBuffer), writtenLength);
		if (!statusCode || statusCode >= 400) {
			Serial.print(SerialLogPrefix_push);
			Serial.print(F("Failed, invalid status code: "));
			Serial.println(statusCode);
			return;
		}

		if (!discardResponse) {
			// Read response
			auto stream = httpClient.getStream();
			long samplesCount = stream.parseInt(); // TODO: fix here :C
			Serial.print(SerialLogPrefix_push);
			Serial.print(F("Already logged "));
			Serial.print(samplesCount);
			Serial.print(F(" samples.\r\n"));
		}

#if DEBUG_CLOUD_LOGGER >= 2
		unsigned long int endTime = millis();
		Serial.print(SerialLogPrefix_push);
		Serial.print(F("time used: "));
		Serial.print(endTime - startTime);
		Serial.println(F("ms"));

#if DEBUG_CLOUD_LOGGER >= 3
		Serial.println(F("Response body: "));
		Serial.println(httpClient.getString());
#endif
#endif

		// Disconnect
		httpClient.end();
		sslClient.stop();
	}


	inline void readSettings() {
		uint8_t intervalSetting = 0;
		EEPROM.get(EEPROMOffset, intervalSetting);
		interval = parseIntervalFromSetting(intervalSetting);
#if DEBUG_CLOUD_LOGGER >= 1
		if (interval < 10000) {
			Serial.println(F("Cloud logging interval too short, using 10 seconds."));
			interval = 10000;
		}
#endif
	}
	inline void saveSettings() {
		uint8_t intervalSetting = parseIntervalToSetting(interval);
		EEPROM.put(EEPROMOffset, intervalSetting);
	}

	inline void setup() {
		readSettings();
	}
}
