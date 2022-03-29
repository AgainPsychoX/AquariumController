#pragma once

#include "common.hpp"
#include "Averager.hpp"

namespace phMeter {
	constexpr uint8_t pin = A0;
	constexpr uint16_t analogMax = 1024;
	constexpr float analogMaxVoltage = 3.2f;

	constexpr uint8_t samplesCount = 16;

	////////////////////////////////////////
	// Settings

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
	static_assert(sizeof(CalibrationPoint) == 8, "Structures saved on EEPROM must have guaranted size!");

	struct Settings {
		CalibrationPoint points[3];
		InterpolationMode mode;
	};
	static_assert(sizeof(Settings) == 28, "Structures saved on EEPROM must have guaranted size!");

	Settings settings;

	constexpr unsigned int EEPROMOffset = 0x220;

	void saveSettings() {
		EEPROM.put(EEPROMOffset, settings);
	}
	void readSettings() {
		EEPROM.get(EEPROMOffset, settings);
		settings.mode = InterpolationMode::Linear;
	}
	void printSettings() {
		LOG_INFO(phMeter, "Calibration points:");
		for (uint8_t i = 0; i < 3; i++) {
			LOG_INFO(phMeter, "%2.4f @ %4u (%.4fV)",
				settings.points[i].pH, 
				settings.points[i].adc, 
				analogMaxVoltage * settings.points[i].adc / analogMax
			);
		}
		switch (settings.mode) {
			case Linear: LOG_INFO(phMeter, "Mode: Linear"); break;
			default:     LOG_INFO(phMeter, "Mode: Unknown"); break;
		}
	}

	////////////////////////////////////////
	// Sampling

	using Averager_t = Averager<uint16_t, samplesCount, uint32_t>;
	Averager_t averager(800);

	uint16_t raw;

	inline uint16_t getRaw() { return raw; }
	inline uint16_t getRawAverage() { return averager.getAverage(); }

	/// Calculates raw value to pH scale value based on calibration.
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

	float getCurrent() { return calculate(getRaw()); }
	float getAverage() { return calculate(getRawAverage()); }

	// TODO: Cache some calculations

	void update() {
		raw = analogRead(phMeter::pin);
		averager.pushSample(raw);
	}

	void setup() {
		readSettings();
		printSettings();
	}
}
