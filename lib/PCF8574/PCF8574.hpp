#pragma once

#include <Arduino.h>
#include <Wire.h>

struct PCF8574 {
	uint8_t address;
	uint8_t state;

	PCF8574(uint8_t address) : address(address) {}

	void read() {
		Wire.requestFrom(address, (uint8_t)1); 
		delayMicroseconds(50);
		if (Wire.available()) {
			state = Wire.read();
		}
	}
	void write() {
		Wire.beginTransmission(address);
		Wire.write(state);
		Wire.endTransmission();
	}

	bool digitalRead(byte pin) {
		read();
		return (state >> pin) & 1;
	}
	void digitalWrite(byte pin, bool value) {
		if (value) {
			state |= 1 << pin;
		}
		else {
			state &= ~(1 << pin);
		}
		write();
	}
};
