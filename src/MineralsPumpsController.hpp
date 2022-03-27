#pragma once
#include "common.hpp"

namespace MineralsPumps {
	enum Mineral : uint8_t {
		Ca = 0,
		Mg = 1,
		KH = 2,
		Count,
	};

	/// Settings for mineral pump (EEPROM structure).
	struct PumpSettings {
		/// Calibration value is defined by time (in miliseconds) required to pump one milliliter.
		float calibration;

		uint8_t hour;
		uint8_t minute;

		uint8_t milliliters;
		uint8_t _pad;

		/// Allocates C-style string representing entry data.
		/// Format: `255mL @ 00:00 c: 123.456`.
		std::unique_ptr<char[]> toCString(int pad = 0) const {
			char* buf = new char[32]; 
			snprintf(buf, 32, "%*umL @ %02u:%02u c: %f", pad, milliliters, hour, minute, calibration);
			return std::unique_ptr<char[]>(buf);
		}
	};
	static_assert(sizeof(PumpSettings) == 8, "Structures saved on EEPROM must have guaranted size!");

	struct Pump {
		PumpSettings settings;
		unsigned long duration = 0;
		unsigned long lastStartTime = 0;
		uint8_t pin;
		union {
			struct {
				bool active : 1;
				bool queued : 1;
				bool manual : 1;
			};
			uint8_t flags = 0;
		};

		inline uint8_t getMilliliters() const {
			return settings.milliliters;
		}
		void setMilliliters(uint8_t mL) {
			settings.milliliters = mL;
			duration = static_cast<unsigned long>(settings.calibration * mL + 0.5f);
		}

		inline float getCalibration() const {
			return settings.calibration;
		}
		void setCalibration(float value) {
			settings.calibration = value;
			duration = static_cast<unsigned long>(value * settings.milliliters + 0.5f);
		}

		float getMillilitersForDuration(unsigned long duration) {
			return (float) duration / settings.calibration;
		}

		inline unsigned long getStopTime() {
			return lastStartTime + duration;
		}

		Pump(uint8_t pin) : pin(pin) {}

		void set(bool active) {
			this->active = active;
			if (active) {
				lastStartTime = millis();
			}
			ioExpander.digitalWrite(pin, !active);
		}

		bool shouldStart(DateTime now) {
			return (
				!active &&
				settings.milliliters > 0 &&
				now.hour()   == settings.hour && 
				now.minute() == settings.minute &&
				millis() - lastStartTime > 60000
			);
		}

		bool shouldStop() {
			return active && millis() - lastStartTime >= duration && !manual;
		}
	};

	// Pins using IO expander
	Pump pumps[Mineral::Count] = {
		{ 2 },
		{ 5 },
		{ 4 },
	};

	const char* pumpsKeys[Mineral::Count] = { "ca", "mg", "kh" };

	constexpr unsigned int EEPROMOffset = 0x200;

	void saveSettings() {
		for (uint8_t i = 0; i < Mineral::Count; i++) {
			EEPROM.put(EEPROMOffset + i * sizeof(PumpSettings), pumps[i].settings);
		}
	}
	void readSettings() {
		for (uint8_t i = 0; i < Mineral::Count; i++) {
			EEPROM.get(EEPROMOffset + i * sizeof(PumpSettings), pumps[i].settings);
			pumps[i].setMilliliters(pumps[i].settings.milliliters);
		}
	}
	void printSettings() {
		LOG_INFO(MineralsPumps, "Settings:");
		for (uint8_t i = 0; i < Mineral::Count; i++) {
			LOG_INFO(MineralsPumps, "%s: %s", pumpsKeys[i], pumps[i].settings.toCString(3));
		}
	}

	/// Flag for whenever any of the pumps is pumping. Used to avoid heavy tasks while pumping.
	bool pumping = false;

	Mineral which = Mineral::Count;

	void update() {
		unsigned long currentMillis = millis();

		// Once every minute check if any pump should be started
		static unsigned long previousEnabling = 0;
		if (currentMillis - previousEnabling > 59000) {
			currentMillis = previousEnabling = millis();

			LOG_TRACE(MineralsPumps, "update()");

			// Mark pumps to be queued
			DateTime now = rtc.now();
			for (uint8_t i = 0; i < Mineral::Count; i++) {
				if (pumps[i].shouldStart(now)) {
					pumps[i].queued = true;
					LOG_DEBUG(MineralsPumps, "Pump #%u (%s) queued", i, pumpsKeys[i]);
				}
			}

			// If not pumping, start next queued pump
			if (!pumping) {
				for (uint8_t i = 0; i < Mineral::Count; i++) {
					if (pumps[i].queued) {
						pumps[i].queued = false;
						pumps[i].set(true);
						pumping = true;
						which = static_cast<Mineral>(i);
						LOG_DEBUG(MineralsPumps, "Pump #%u (%s) started", i, pumpsKeys[i]);
						break;
					}
				}
			}
		}

		// Always check if any pump is finishing, for more precise timings
		bool anyPumping = false;
		for (uint8_t i = 0; i < Mineral::Count; i++) {
			if (pumps[i].shouldStop()) {
				pumps[i].set(false);
				LOG_DEBUG(MineralsPumps, "Pump #%u (%s) stopped", i, pumpsKeys[i]);
			}
			anyPumping = anyPumping || pumps[i].active;
		}
		pumping = anyPumping;
	}
	void setup() {
		readSettings();
		printSettings();
	}

	void handleWebEndpoint() {
		constexpr unsigned int bufferLength = 256;
		char buffer[bufferLength];

		const String& key = webServer.arg("key");
		if (!key.isEmpty()) {
			const String& action = webServer.arg("action");
			const bool on     = action.equals("on");
			const bool dose   = action.equals("dose");
			const bool active = on || dose;
			const bool set    = active || action.equals("off");
			const bool manual = on && !dose;
			if (set) {
				Mineral which = Mineral::Count;
				/**/ if (key.equals("ca")) which = Mineral::Ca;
				else if (key.equals("mg")) which = Mineral::Mg;
				else if (key.equals("kh")) which = Mineral::KH;
				if (which != Mineral::Count) {
					pumps[which].manual = manual;
					pumps[which].set(active);
				}
			}
			int ret = snprintf(
				buffer, bufferLength,
				"{"
					"\"mineralsPumps\":{"
						"\"ca\":{\"lastStartTime\":%lu},"
						"\"mg\":{\"lastStartTime\":%lu},"
						"\"kh\":{\"lastStartTime\":%lu}"
					"},"
					"\"currentTime\":%lu"
				"}",
				pumps[Mineral::Ca].lastStartTime,
				pumps[Mineral::Mg].lastStartTime,
				pumps[Mineral::KH].lastStartTime,
				millis()
			);
			if (ret < 0 || static_cast<unsigned int>(ret) >= bufferLength) {
				webServer.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
			}
			else {
				webServer.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
			}
			return;
		}

		webServer.send(200);
	}
};
