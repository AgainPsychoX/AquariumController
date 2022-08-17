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
		
		refillingRequired = mainTankDetector.isContinuouslyDissatisfied();
		doRefilling(refillingRequired);

		backupTankLow = backupTankDetector.isContinuouslyDissatisfied();
	}
}
