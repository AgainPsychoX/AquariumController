#pragma once // Note: Included directly in certain place.

#include <Arduino.h>
#include <EEPROM.h>

namespace MineralsPumpsController {
	enum Mineral : uint8_t {
		Ca = 0,
		Mg = 1,
		KH = 2,
		Count,
	};

	/// Settings for mineral pump (EEPROM structure).
	struct MineralPumpSettings {
		/// Calibration value is defined by time (in miliseconds) required to pump one milliliter.
		float calibration;

		uint8_t hour;
		uint8_t minute;

		uint8_t milliliters;
		uint8_t _pad;
	};
	static_assert(sizeof(MineralPumpSettings) == 8, "Structures saved on EEPROM must have guaranted size!");

	struct MineralPump {
		MineralPumpSettings settings;
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

		float getMillilitersForDuration(unsigned long duration) {
			return (float) duration / settings.calibration;
		}

		MineralPump(uint8_t pin) : pin(pin) {}

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
	MineralPump pumps[Mineral::Count] = {
		{ 2 },
		{ 5 },
		{ 4 },
	};

	constexpr unsigned int EEPROMOffset = 0x200;

	inline void saveSettings() {
		for (uint8_t i = 0; i < Mineral::Count; i++) {
			EEPROM.put(EEPROMOffset + i * sizeof(MineralPumpSettings), pumps[i].settings);
		}
	}
	inline void readSettings() {
		for (uint8_t i = 0; i < Mineral::Count; i++) {
			EEPROM.get(EEPROMOffset + i * sizeof(MineralPumpSettings), pumps[i].settings);
			pumps[i].settings.calibration = 412.0f;
			pumps[i].setMilliliters(pumps[i].settings.milliliters);
		}
	}
#if DEBUG_MINERALS_PUMPS_CONTROLLER >= 1
	void printOut() {
		Serial.println(F("MineralsPumpController settings:"));
		for (uint8_t i = 0; i < Mineral::Count; i++) {
			Serial.print(pumps[i].getMilliliters());
			Serial.print("mL\t@ ");
			Serial.print(pumps[i].settings.hour);
			Serial.print(':');
			Serial.print(pumps[i].settings.minute);
			Serial.print("\t\tc: ");
			Serial.println(pumps[i].settings.calibration);
		}
	}
#endif

	/// Flag for whenever any of the pumps is pumping. Used to avoid heavy tasks while pumping.
	bool pumping = false;

	Mineral which = Mineral::Count;

	inline void update() {
		unsigned long currentMillis = millis();

		// Once every minute check if any pump should be started
		static unsigned long previousEnabling = 0;
		if (currentMillis - previousEnabling > 59000) {
			currentMillis = previousEnabling = millis();

#if DEBUG_MINERALS_PUMPS_CONTROLLER >= 1
			Serial.println(F("MineralsPumpController update() "));
#endif

			// Mark pumps to be queued
			DateTime now = rtc.now();
			for (uint8_t i = 0; i < Mineral::Count; i++) {
				if (pumps[i].shouldStart(now)) {
					pumps[i].queued = true;
#if DEBUG_MINERALS_PUMPS_CONTROLLER >= 1
					Serial.print(F("Pump "));
					Serial.print(i);
					Serial.println(F(" queued"));
#endif
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
#if DEBUG_MINERALS_PUMPS_CONTROLLER >= 1
						Serial.print(F("Pump "));
						Serial.print(i);
						Serial.println(F(" started"));
#endif
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
#if DEBUG_MINERALS_PUMPS_CONTROLLER >= 2
				Serial.print(F("Pump "));
				Serial.print(i);
				Serial.println(F(" stopped"));
#endif
			}
			anyPumping = anyPumping || pumps[i].active;
		}
		pumping = anyPumping;
	}
	inline void setup() {
		readSettings();
#if DEBUG_MINERALS_PUMPS_CONTROLLER >= 1
		printOut();
#endif
	}
};
