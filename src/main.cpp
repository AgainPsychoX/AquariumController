
////////////////////////////////////////////////////////////////////////////////

#include "common.hpp"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> 
#include <DS3231.h>
#include <DallasTemperature.h> // for DS18B20 thermometer
#include <PCF8574.hpp>
#include "LightingController.hpp"
#include "WaterLevelController.hpp"
#include "HeatingController.hpp"
#include "CirculatorController.hpp"
#include "CloudLogger.hpp"
#include "MineralsPumpsController.hpp"
#include "phMeter.hpp"

PCF8574 ioExpander {0x26};

DS3231 ds3231;
RTClib rtc;

OneWire oneWire(2);
struct {
	inline void on()  { digitalWrite(D4, LOW); }
	inline void off() { digitalWrite(D4, HIGH); }
	inline void setup() { pinMode(D4, OUTPUT); }
} buildInLed;
DallasTemperature oneWireThermometers(&oneWire);

LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01);

ESP8266WebServer webServer(80);

float waterTemperature = 0; // avg of last and current read (simplest noise reduction)

bool showIP = true;
constexpr unsigned int showIPtimeout = 15000;

////////////////////////////////////////////////////////////////////////////////

void setup() {
	// Initialize I2C Wire protocol
	Wire.begin();

	// Initialize I/O extender, making sure everything is off on start.
	ioExpander.state = 0b00111111;
	ioExpander.write();

	// Initialize Serial console
	Serial.begin(115200);

	// Initialize RTC
	{
		delay(100);
		DateTime now = rtc.now();

		// Check for error
		if (now.hour() > 24 || now.minute() > 60) {
			buildInLed.setup();
			while (true) {
				LOG_ERROR(RTC, "Failed, please reset its memory!");
				buildInLed.on();
				delay(500);
				buildInLed.off();
				delay(500);
				buildInLed.on();
				delay(500);
				buildInLed.off();
				delay(2500);
			}
		}

		// Print time to serial
		LOG_INFO(RTC, "Time is %02u:%02u:%02u", now.hour(), now.minute(), now.second());
	}

	// Initialize display
	lcd.begin(20, 2);
	lcd.clear();

	// Connect to WiFi
	{
		Serial.print(F("Connecting to WiFi"));

		// Connecting info on LCD
		lcd.setCursor(0, 0);
		if (strlen(ssid) < 14) {
			lcd.print(F("SSID: "));
		}
		lcd.print(ssid);

		WiFi.begin(ssid, password);
		while (WiFi.status() != WL_CONNECTED) {
			// buildInLed.on();
			lcd.setCursor(0, 1);
			lcd.print(F("IP: - * - * - * - "));
			delay(333);

			// buildInLed.off();
			lcd.setCursor(0, 1);
			lcd.print(F("IP: * - * - * - * "));
			delay(333);

			Serial.print(F("."));
		}

		Serial.println(F(" connected!"));
		Serial.print(F("IP address: "));
		Serial.println(WiFi.localIP());
		lcd.clear();
	}

	// Initialize water termometer
	oneWireThermometers.begin();
	oneWireThermometers.requestTemperatures();
	float t = oneWireThermometers.getTempCByIndex(0);
	if (t != DEVICE_DISCONNECTED_C){
		waterTemperature = t;
	}

	// Initialize EEPROM
	// See http://arduino.esp8266.com/Arduino/versions/2.0.0/doc/libraries.html#eeprom
	EEPROM.begin(0x1000);
	delay(100);
	// TODO: seeding/factory settings

	// Setup subcontrollers and services
	Lighting::setup();
	Heating::setup();
	Circulator::setup();
	MineralsPumps::setup();
	CloudLogger::setup();
	phMeter::setup();

	// Register server handlers
	webServer.on(F("/"), []() {
		WEB_USE_CACHE_STATIC(webServer);
		WEB_USE_GZIP_STATIC(webServer);
		webServer.send(200, WEB_CONTENT_TYPE_TEXT_HTML, WEB_index_html_CONTENT, WEB_index_html_CONTENT_LENGTH);
	});
	WEB_REGISTER_ALL_STATIC(webServer);

	webServer.on(F("/status"), []() {
		constexpr unsigned int bufferLength = 256;
		char buffer[bufferLength];
		DateTime now = rtc.now();
		int ret = snprintf(
			buffer, bufferLength,
			"{"
				"\"waterTemperature\":%.2f,"
				"\"rtcTemperature\":%.2f,"
				"\"phLevel\":%.6f,"
				"\"red\":%d,\"green\":%d,\"blue\":%d,\"white\":%d,"
				"\"isHeating\":%d,"
				"\"isRefilling\":%d,"
				"\"isRefillTankLow\":%d,"
				"\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\","
				"\"rssi\":%d"
			"}",
			waterTemperature,
			ds3231.getTemperature(),
			phMeter::readAverage(),
			redPWM.get(), greenPWM.get(), bluePWM.get(), whitePWM.get(),
			Heating::isHeating(),
			WaterLevel::refillingRequired,
			WaterLevel::backupTankLow,
			// Format should be like: "2004-02-12T15:19:21" (without time zones)
			now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
			WiFi.RSSI()
		);
		if (ret < 0 || static_cast<unsigned int>(ret) >= bufferLength) {
			webServer.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
		}
		else {
			webServer.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
		}
	});

	webServer.on(F("/config"), []() {
		// Handle config options
		if (const String& str = webServer.arg("timestamp"); !str.isEmpty()) {
			// Format should be like: "2004-02-12T15:19:21" (without time zones)
			const char* cstr = str.c_str();
			ds3231.setYear(  atoi(cstr + 0) % 100);
			ds3231.setMonth( atoi(cstr + 5));
			ds3231.setDate(  atoi(cstr + 8));
			ds3231.setHour(  atoi(cstr + 11));
			ds3231.setMinute(atoi(cstr + 14));
			ds3231.setSecond(atoi(cstr + 17));
		}

		if (const String& str = webServer.arg("red");      !str.isEmpty())   redPWM.set(atoi(str.c_str()));
		if (const String& str = webServer.arg("green");    !str.isEmpty()) greenPWM.set(atoi(str.c_str()));
		if (const String& str = webServer.arg("blue");     !str.isEmpty())  bluePWM.set(atoi(str.c_str()));
		if (const String& str = webServer.arg("white");    !str.isEmpty()) whitePWM.set(atoi(str.c_str()));
		if (const String& str = webServer.arg("forceColors"); !str.isEmpty()) Lighting::disable = webServer.arg("forceColors").equals("true");

		// Handle heating config options
		{
			bool changes = false;
			if (const String& str = webServer.arg("heatingMinTemperature"); !str.isEmpty()) { Heating::minTemperature = atof(str.c_str()); changes = true; }
			if (const String& str = webServer.arg("heatingMaxTemperature"); !str.isEmpty()) { Heating::maxTemperature = atof(str.c_str()); changes = true; }
			if (changes) Heating::saveSettings();
		}

		// Handle circulator config options
		{
			bool changes = false;
			if (const String& str = webServer.arg("circulatorActiveTime"); !str.isEmpty()) { Circulator::activeSeconds = atoi(str.c_str()); changes = true; }
			if (const String& str = webServer.arg("circulatorPauseTime");  !str.isEmpty()) { Circulator::pauseSeconds  = atoi(str.c_str()); changes = true; }
			if (changes) {
				Circulator::nextUpdateTime = millis() + 1000;
				Circulator::saveSettings();
			}
		}

		// Handle minerals pumps config
		{
			using namespace MineralsPumps;
			bool changes = false;

			String arg;
			arg.reserve(24);
			arg = F("mineralsPumps.ca.time");
			constexpr uint8_t keyOffset = 14;

			for (uint8_t i = 0; i < Mineral::Count; i++) {
				const char* key = pumpsKeys[i];
				arg[keyOffset + 0] = key[0];
				arg[keyOffset + 1] = key[1];
				const String& value = webServer.arg(arg);
				if (!value.isEmpty()) {
					const unsigned short time = atoi(value.c_str());
					pumps[i].settings.hour   = time / 60;
					pumps[i].settings.minute = time % 60;
					changes = true;
				}
			}

			arg.replace(F(".time"), F(".mL"));

			for (uint8_t i = 0; i < Mineral::Count; i++) {
				const char* key = pumpsKeys[i];
				arg[keyOffset + 0] = key[0];
				arg[keyOffset + 1] = key[1];
				const String& value = webServer.arg(arg);
				if (!value.isEmpty()) {
					const uint8_t mL = atoi(value.c_str());
					pumps[i].setMilliliters(mL);
					changes = true;
				}
			}

			arg.replace(F(".mL"), F(".c"));

			for (uint8_t i = 0; i < Mineral::Count; i++) {
				const char* key = pumpsKeys[i];
				arg[keyOffset + 0] = key[0];
				arg[keyOffset + 1] = key[1];
				const String& value = webServer.arg(arg);
				if (!value.isEmpty()) {
					const float c = atof(value.c_str());
					pumps[i].setCalibration(c);
					changes = true;
				}
			}

			if (changes) {
				MineralsPumps::saveSettings();
				MineralsPumps::printSettings();
			}
		}

		if (const String& str = webServer.arg("cloudLoggingInterval"); !str.isEmpty()) {
			CloudLogger::interval = atoi(str.c_str()) * 1000;
			CloudLogger::saveSettings();
		}

		// Response with current config
		const auto& pump_ca = MineralsPumps::pumps[MineralsPumps::Mineral::Ca];
		const auto& pump_mg = MineralsPumps::pumps[MineralsPumps::Mineral::Mg];
		const auto& pump_kh = MineralsPumps::pumps[MineralsPumps::Mineral::KH];
		constexpr unsigned int bufferLength = 256;
		char buffer[bufferLength];
		int ret = snprintf_P(
			buffer, bufferLength,
			"{"
				"\"heatingMinTemperature\":%.2f,"
				"\"heatingMaxTemperature\":%.2f,"
				"\"circulatorActiveTime\":%u,"
				"\"circulatorPauseTime\":%u,"
				"\"mineralsPumps\":{"
					"\"ca\":{\"time\":%u,\"mL\":%u},"
					"\"mg\":{\"time\":%u,\"mL\":%u},"
					"\"kh\":{\"time\":%u,\"mL\":%u}"
				"},"
				"\"cloudLoggingInterval\":%u"
			"}",
			Heating::minTemperature,
			Heating::maxTemperature,
			Circulator::activeSeconds,
			Circulator::pauseSeconds,
			(pump_ca.settings.hour * 60) + pump_ca.settings.minute, pump_ca.getMilliliters(),
			(pump_mg.settings.hour * 60) + pump_mg.settings.minute, pump_mg.getMilliliters(),
			(pump_kh.settings.hour * 60) + pump_kh.settings.minute, pump_kh.getMilliliters(),
			static_cast<unsigned int>(CloudLogger::interval / 1000)
		);
		if (ret < 0 || static_cast<unsigned int>(ret) >= bufferLength) {
			webServer.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
		}
		else {
			webServer.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
		}

		// As somebody connected, remove flag
		if (showIP) {
			lcd.clear();
		}
		showIP = false;
	});

	// Built-in LED should not be activated outside early setup, as OneWire uses the same pin.
	// webServer.on(F("/led"), []() {
	//     if (webServer.args() > 0) {
	//         buildInLed.on();
	//     }
	//     else {
	//         buildInLed.off();
	//     }
	//     webServer.send(200);
	// });

	webServer.on(F("/saveEEPROM"), []() {
		EEPROM.commit();
		webServer.send(200);
	});

	webServer.on(F("/getColorsCycle"), Lighting::handleGetColors);
	webServer.on(F("/setColorsCycle"), Lighting::handleSetColors);
	webServer.on(F("/resetColorsCycle"), Lighting::handleResetColors);

	webServer.on(F("/mineralsPumps"), MineralsPumps::handleWebEndpoint);

	if constexpr (debugLevel >= LEVEL_DEBUG) {
		// Hidden API for testing propuses
		webServer.on(F("/test"), []() {
			// const int adcValue = analogRead(phMeter::pin);
			// const float voltage = (float)adcValue / 1024 * 3.3f;
			// const float pH = 7.f - (2.5f - voltage) * phMeter::calibration;

			// constexpr unsigned int bufferLength = 128;
			// char buffer[bufferLength];
			// int ret = snprintf(
			// 	buffer, bufferLength,
			// 	"{"
			// 		"\"adc\":%i,"
			// 		"\"voltage\":%.4f,"
			// 		"\"pH\":%.4f,"
			// 		"\"currentTime\":%lu"
			// 	"}",
			// 	adcValue,
			// 	voltage,
			// 	pH,
			// 	millis()
			// );
			// if (ret < 0 || static_cast<unsigned int>(ret) >= bufferLength) {
			// 	webServer.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
			// }
			// else {
			// 	webServer.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
			// }
		});
	}

	webServer.onNotFound([]() {
		webServer.send(404, WEB_CONTENT_TYPE_TEXT_PLAIN, PSTR("Not found\n\n"));
	});
	webServer.begin();
}

////////////////////////////////////////////////////////////////////////////////

#define UPDATE_EVERY(interval)          \
	for(                                \
		static unsigned long prev = 0;  \
		currentMillis - prev > interval;\
		currentMillis = prev = millis() \
	)

void loop() {
	unsigned long currentMillis = millis();

	MineralsPumps::update();

	webServer.handleClient();

	if (CloudLogger::isEnabled() && !MineralsPumps::pumping) {
		UPDATE_EVERY(CloudLogger::interval) {
			CloudLogger::push({
				.waterTemperature = waterTemperature,
				.rtcTemperature = ds3231.getTemperature(),
				.phLevel = phMeter::readAverage(),
			});
		}
	}

	UPDATE_EVERY(500) {
		Lighting::update();

		Circulator::update();

		WaterLevel::update();

		phMeter::update();

		// For debug & calibration
		// LOG_TRACE(phMeter, "Raw: %4u Avg: %4u", phMeter::readRaw(), phMeter::readRawAverage());
	}

	UPDATE_EVERY(1000) {
		// Show time on LCD
		{
			DateTime now = rtc.now();
			lcd.setCursor(0, 0);
			if (now.hour() < 10) {
				lcd.print('0');
			}
			lcd.print(now.hour());
			lcd.print(':');
			if (now.minute() < 10) {
				lcd.print('0');
			}
			lcd.print(now.minute());
			lcd.print(':');
			if (now.second() < 10) {
				lcd.print('0');
			}
			lcd.print(now.second());
		}

		// Update themometer read
		{
			oneWireThermometers.requestTemperatures();
			float t = oneWireThermometers.getTempCByIndex(0);
			if (t != DEVICE_DISCONNECTED_C){
				waterTemperature = (waterTemperature + t) / 2;
			}
		}

		Heating::update(waterTemperature);

		// Show water termometer on LCD
		{
			char buf[5];
			snprintf(buf, 5, "%.1f", waterTemperature);
			buf[4] = 0;
			lcd.setCursor(20 - 6, 0);
			lcd.print(buf);
			lcd.write(0b11011111);
			lcd.write('C');
		}

		lcd.setCursor(0, 1);
		if (showIP) {
			// Show only IP in second row
			lcd.setCursor(0, 1);
			lcd.print(F("IP: "));
			lcd.print(WiFi.localIP().toString().c_str());
			if (millis() > showIPtimeout) {
				showIP = false;
				lcd.clear();
			}
		}
		else {
			// Show message
			using namespace MineralsPumps;
			if (MineralsPumps::pumping) {
				lcd.print(F("Dozowanie "));
				switch (MineralsPumps::which) {
					case Mineral::Ca:	lcd.print(F("Ca... "));	break;
					case Mineral::Mg:	lcd.print(F("Mg... "));	break;
					case Mineral::KH:	lcd.print(F("KH... "));	break;
					default: break;
				}
			}
			else if (WaterLevel::backupTankLow) {
				lcd.print(F("Niski poziom RO!"));
			}
			else if (WaterLevel::refillingRequired) {
				lcd.print(F("Dolewanie RO... "));
			}
			else {
				lcd.print(F("                "));
			}

			//lcd.setCursor(20 - 1, 1);
			lcd.print("   ");

			// Show heating status
			lcd.write(Heating::isHeating() ? 'G' : ' '); // G - Grzałka
		}
	}
}
