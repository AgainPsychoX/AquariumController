
#include "WaterLevelController.hpp"

namespace WaterLevel {
	Detector<7> mainTankDetector;
	Detector<6> backupTankDetector;

	bool refillingRequired = false;
	bool backupTankLow = false;

	void doRefilling(bool state) {
		ioExpander.digitalWrite(refillerPin, state ? LOW : HIGH);
	}

	void setup() {
		mainTankDetector.setup();
		backupTankDetector.setup();
	}

	void update() {
		mainTankDetector.checkIsSatisfiedNow();
		backupTankDetector.checkIsSatisfiedNow();
		
		if (CHECK_LOG_LEVEL(WaterLevel, LEVEL_DEBUG)) {
			static uint32_t lastTime = 0;
			bool refillingChange = refillingRequired;
			refillingRequired = mainTankDetector.isContinuouslyDissatisfied();
			refillingChange ^= refillingRequired;
			if (refillingChange) {
				LOG_DEBUG(WaterLevel, "Refilling %s after %lums.", refillingRequired ? "started" : "stopped", millis() - lastTime)
			}
			doRefilling(refillingRequired);
		}
		else {
			refillingRequired = mainTankDetector.isContinuouslyDissatisfied();
			doRefilling(refillingRequired);
		}

		backupTankLow = backupTankDetector.isContinuouslyDissatisfied();

		LOG_TRACE(WaterLevel, "main: %s | backup: %s | refilling: %s | showBackupLow: %s", 
			mainTankDetector.wasSatisfiedNow()   ? "UP  " : "DOWN",
			backupTankDetector.wasSatisfiedNow() ? "UP  " : "DOWN",
			refillingRequired ? "YES (LOW)" : "NO (OK)  ",
			backupTankLow ? "LOW" : "OK"
		);
	}
}
