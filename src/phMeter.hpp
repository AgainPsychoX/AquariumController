#pragma once

#include "common.hpp"
#include "Averager.hpp"

namespace phMeter {
	constexpr uint8_t pin = A0;
	constexpr uint16_t analogMax = 1024;
	constexpr float analogMaxVoltage = 3.2f;

	constexpr uint8_t samplesCount = 16;

	void printSettings() {
		LOG_INFO(phMeter, "Calibration points:");
		for (uint8_t i = 0; i < 3; i++) {
			LOG_INFO(phMeter, "%2.4f @ %4u (%.4fV)",
				settings->phMeter.pH[i],
				settings->phMeter.adc[i],
				analogMaxVoltage * settings->phMeter.adc[i] / analogMax
			);
		}
		switch (settings->phMeter.mode) {
			case Linear: LOG_INFO(phMeter, "Mode: Linear"); break;
			default:     LOG_INFO(phMeter, "Mode: Unknown"); break;
		}
	}

	using Averager_t = Averager<uint16_t, samplesCount, uint32_t>;
	Averager_t averager(800);

	uint16_t raw;

	inline uint16_t getRaw() { return raw; }
	inline uint16_t getRawAverage() { return averager.getAverage(); }

	/// Calculates raw value to pH scale value based on calibration.
	float calculate(uint16 raw) {
		switch (settings->phMeter.mode) {
			case Linear:
				if (raw <= settings->phMeter.adc[1]) {
					const auto x0 = settings->phMeter.adc[0];
					const auto y0 = settings->phMeter.pH[0];
					const auto x1 = settings->phMeter.adc[1];
					const auto y1 = settings->phMeter.pH[1];
					const float a = (y0 - y1) / (x0 - x1);
					const float b = y0 - a * x0;
					return a * raw + b;
				}
				else {
					const auto x0 = settings->phMeter.adc[1];
					const auto y0 = settings->phMeter.pH[1];
					const auto x1 = settings->phMeter.adc[2];
					const auto y1 = settings->phMeter.pH[2];
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
		printSettings();
	}
}
