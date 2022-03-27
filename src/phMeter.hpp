#pragma once

#include "common.hpp"
#include "Averager.hpp"

namespace phMeter {
	constexpr uint8_t pin = A0;
	constexpr uint16_t analogMax = 1024;

	constexpr unsigned long sampleInterval = 33; // ms
	constexpr uint8_t samplesCount = 16;

	using Averager_t = Averager<uint16_t, samplesCount, uint32_t>;
	Averager_t averager(analogMax / 2);

	enum InterpolationMode : uint8_t {
		Linear,
		// TODO: Other interpolation modes
		// Cosine,
		// Polynomial,
		Count,
	};

	struct CalibrationPoint {
		float pH;
		uint16_t adc;
	};

	struct Settings {
		CalibrationPoint points[3];
		InterpolationMode mode;
	};

	Settings settings;

	constexpr unsigned int EEPROMOffset = 0x220;

	void saveSettings() {
		EEPROM.put(EEPROMOffset, settings);
	}
	void readSettings() {
		EEPROM.get(EEPROMOffset, settings);
	}
	void printSettings() {
		LOG_INFO(phMeter, "Settings:");
		for (uint8_t i = 0; i < 3; i++) {
			LOG_INFO(phMeter, "%2.4f @ %u", settings.points[i].pH, settings.points[i].adc);
		}
	}

	inline uint16_t readRaw() {
		return analogRead(phMeter::pin);
	}

	inline uint16_t readRawAverage() {
		return averager.getAverage();
	}

	float calculate(uint16 raw) {
		switch (settings.mode) {
			case Linear:
				if (raw <= settings.points[1].adc) {
					const auto [y0, x0] = settings.points[0];
					const auto [y1, x1] = settings.points[1];
					const float a = (y0 - y1) / (x0 - x1);
					const float b = y0 - a * x0;
					return a * raw + b;
				}
				else {
					const auto [y0, x0] = settings.points[1];
					const auto [y1, x1] = settings.points[2];
					const float a = (y0 - y1) / (x0 - x1);
					const float b = y0 - a * x0;
					return a * raw + b;
				}
			default: 
				return -1.0f;
		}
	}

	float read() {
		return calculate(readRaw());
	}

	float readAverage() {
		return calculate(readRawAverage());
	}

	// TODO: Cache some calculations

	void update() {
		averager.pushSample(readRaw());
	}

	void setup() {
		// TODO: settings, calibration via web panel with saving to EEPROM
		// readSettings();
		// Static settings that works for us
		settings.mode = InterpolationMode::Linear;
		settings.points[0] = { .pH = 9.18f + 0.02f, .adc = 712 };
		settings.points[1] = { .pH = 6.86f + 0.02f, .adc = 851 };
		settings.points[2] = { .pH = 4.01f + 0.02f, .adc = 1008 };
		printSettings();
	}
}
