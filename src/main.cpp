
////////////////////////////////////////////////////////////////////////////////

#include "common.hpp"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> 
#include <DS3231.h>
#include <DallasTemperature.h> // for DS18B20 thermometer
#include <PCF8574.hpp>
#include "Network.hpp"
#include "LightingController.hpp"
#include "WaterLevelController.hpp"
#include "HeatingController.hpp"
#include "CirculatorController.hpp"
#include "CloudLogger.hpp"
#include "MineralsPumpsController.hpp"
#include "phMeter.hpp"
#include "webEncoded/WebStaticContent.hpp"

Settings* settings;

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
const uint8_t PROGMEM lcd_customChar_WiFi_best[] { 0x1E, 0x01, 0x1C, 0x02, 0x19, 0x05, 0x15, 0x00 };
const uint8_t PROGMEM lcd_customChar_WiFi_good[] { 0x00, 0x00, 0x1C, 0x02, 0x19, 0x05, 0x15, 0x00 };
const uint8_t PROGMEM lcd_customChar_WiFi_okay[] { 0x00, 0x00, 0x00, 0x00, 0x18, 0x04, 0x14, 0x00 };
const uint8_t PROGMEM lcd_customChar_dot_x0y7[] { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 };
const uint8_t PROGMEM lcd_customChar_vline_x0y4[] { 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00 };

ESP8266WebServer webServer(80);

float waterTemperature = 0; // avg of last and current read (simplest noise reduction)

bool showIP = true;
constexpr unsigned int showIPtimeout = 20000;

////////////////////////////////////////////////////////////////////////////////

// Parse boolean string.
bool parseBoolean(const char* str) {
	//return str[0] == '1' || str[0] == 't' || str[0] == 'T' || str[0] == 'y' || str[0] == 'Y';
	return !(str[0] == '0' || str[0] == 'f' || str[0] == 'F' || str[0] == 'n' || str[0] == 'N');
}

////////////////////////////////////////////////////////////////////////////////

void setup() {
	// Initialize I2C Wire protocol
	Wire.begin();

	// Initialize I/O extender, making sure everything is off on start.
	ioExpander.state = 0b00111111;
	ioExpander.write();

	// Perform some early setup
	Heating::earlySetup();

	// Initialize Serial console
	Serial.begin(115200);
	Serial.println();

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
	lcd.createChar(1, lcd_customChar_dot_x0y7);
	lcd.createChar(2, lcd_customChar_WiFi_okay);
	lcd.createChar(3, lcd_customChar_WiFi_good);
	lcd.createChar(4, lcd_customChar_WiFi_best);
	lcd.createChar(5, lcd_customChar_vline_x0y4);

	// Initialize EEPROM
	{
		EEPROM.begin(sizeof(Settings));
		delay(100);
		settings = reinterpret_cast<Settings*>(EEPROM.getDataPtr());

		if (settings->calculateChecksum() != settings->checksum) {
			LOG_INFO(EEPROM, "Checksum miss-match, reseting settings to default.");
			settings->resetToDefault();
			Network::resetConfig();
			Lighting::resetToDefaultSettings();

			EEPROM.commit();
			lcd.setCursor(0, 0);
			lcd.print(F("Reseting EEPROM..."));
			delay(3000);
			ESP.restart();
		}
	}

	// Setup network service (might take a while, incl. LCD animation)
	Network::setup();
	if (settings->network.mode == Settings::Network::DISABLED) {
		showIP = false;
	}
	lcd.clear();

	// Initialize water termometer
	oneWireThermometers.begin();
	oneWireThermometers.requestTemperatures();
	float t = oneWireThermometers.getTempCByIndex(0);
	if (t != DEVICE_DISCONNECTED_C) {
		waterTemperature = t;
	}

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
				"\"phLevel\":%.6f,\"phRaw\":%u,"
				"\"red\":%d,\"green\":%d,\"blue\":%d,\"white\":%d,"
				"\"heatingStatus\":%u,"
				"\"isRefilling\":%d,"
				"\"isRefillTankLow\":%d,"
				"\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\","
				"\"rssi\":%d"
			"}",
			waterTemperature,
			ds3231.getTemperature(),
			phMeter::getAverage(), phMeter::getRawAverage(),
			redPWM.get(), greenPWM.get(), bluePWM.get(), whitePWM.get(),
			Heating::getStatus(),
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

		// Handle lighting immediate config (not preserved, exposed for testing)
		if (const String& str = webServer.arg("red");      !str.isEmpty())   redPWM.set(atoi(str.c_str()));
		if (const String& str = webServer.arg("green");    !str.isEmpty()) greenPWM.set(atoi(str.c_str()));
		if (const String& str = webServer.arg("blue");     !str.isEmpty())  bluePWM.set(atoi(str.c_str()));
		if (const String& str = webServer.arg("white");    !str.isEmpty()) whitePWM.set(atoi(str.c_str()));
		if (const String& str = webServer.arg("forceColors"); !str.isEmpty()) Lighting::disable = parseBoolean(str.c_str());

		// Handle heating config options
		if (const String& str = webServer.arg("waterTemperatures.minimal"); !str.isEmpty()) settings->temperatures.minimal = atof(str.c_str());
		if (const String& str = webServer.arg("waterTemperatures.optimal"); !str.isEmpty()) settings->temperatures.optimal = atof(str.c_str());
		if (const String& str = webServer.arg("waterTemperatures.maximal"); !str.isEmpty()) settings->temperatures.maximal = atof(str.c_str());

		// Handle circulator config options
		if (const String& str = webServer.arg("circulatorActiveTime"); !str.isEmpty()) settings->circulator.activeTime = atoi(str.c_str()) * 1000;
		if (const String& str = webServer.arg("circulatorPauseTime");  !str.isEmpty()) settings->circulator.pauseTime  = atoi(str.c_str()) * 1000;

		// Handle minerals pumps config
		{
			using namespace MineralsPumps;
			bool changes = false;

			String arg;
			arg.reserve(24);
			arg = F("mineralsPumps.ca.time");
			constexpr uint8_t keyOffset = 14;

			for (uint8_t i = 0; i < Mineral::Count; i++) {
				arg[keyOffset + 0] = pumpsKeys[i][0];
				arg[keyOffset + 1] = pumpsKeys[i][1];
				if (const String& str = webServer.arg(arg); !str.isEmpty()) {
					const unsigned short time = atoi(str.c_str());
					pumps[i].settings->hour   = time / 60;
					pumps[i].settings->minute = time % 60;
					changes = true;
				}
			}

			arg.replace(F(".time"), F(".mL"));

			for (uint8_t i = 0; i < Mineral::Count; i++) {
				arg[keyOffset + 0] = pumpsKeys[i][0];
				arg[keyOffset + 1] = pumpsKeys[i][1];
				if (const String& str = webServer.arg(arg); !str.isEmpty()) {
					const uint8_t mL = atoi(str.c_str());
					pumps[i].setMilliliters(mL);
					changes = true;
				}
			}

			arg.replace(F(".mL"), F(".c"));

			for (uint8_t i = 0; i < Mineral::Count; i++) {
				arg[keyOffset + 0] = pumpsKeys[i][0];
				arg[keyOffset + 1] = pumpsKeys[i][1];
				if (const String& str = webServer.arg(arg); !str.isEmpty()) {
					const float c = atof(str.c_str());
					pumps[i].setCalibration(c);
					changes = true;
				}
			}

			if (changes) {
				MineralsPumps::setup();
			}
		}

		// Handle pH meter config
		{
			bool changes = false;

			String arg;
			arg.reserve(24);
			arg = F("phMeter.points.0.adc");
			constexpr uint8_t indexOffset = 15;

			for (uint8_t i = 0; i < 3; i++) {
				arg[indexOffset] = '0' + i;
				if (const String& value = webServer.arg(arg); !value.isEmpty()) {
					settings->phMeter.adc[i] = atoi(value.c_str());
					changes = true;
				}
			}

			arg.replace(F("adc"), F("pH"));

			for (uint8_t i = 0; i < 3; i++) {
				arg[indexOffset] = '0' + i;
				if (const String& value = webServer.arg(arg); !value.isEmpty()) {
					settings->phMeter.pH[i] = atof(value.c_str());
					changes = true;
				}
			}

			if (changes) {
				phMeter::setup();
			}
		}

		if (const String& str = webServer.arg("cloudLoggingInterval"); !str.isEmpty()) {
			CloudLogger::setInterval(atoi(str.c_str()) * 1000);
		}

		// Handle network config
		Network::handleConfigArgs();

		// Response with current config
		const auto& pump_ca = MineralsPumps::pumps[MineralsPumps::Mineral::Ca];
		const auto& pump_mg = MineralsPumps::pumps[MineralsPumps::Mineral::Mg];
		const auto& pump_kh = MineralsPumps::pumps[MineralsPumps::Mineral::KH];
		constexpr unsigned int bufferLength = 640;
		char buffer[bufferLength];
		int ret = snprintf_P(
			buffer, bufferLength,
			PSTR("{"
				"\"waterTemperatures\":{"
					"\"minimal\":%.2f,"
					"\"optimal\":%.2f,"
					"\"maximal\":%.2f"
				"},"
				"\"circulatorActiveTime\":%u,"
				"\"circulatorPauseTime\":%u,"
				"\"mineralsPumps\":{"
					"\"ca\":{\"time\":%u,\"mL\":%u},"
					"\"mg\":{\"time\":%u,\"mL\":%u},"
					"\"kh\":{\"time\":%u,\"mL\":%u}"
				"},"
				"\"phMeter\":{"
					"\"points\":["
						"{\"adc\":%u,\"pH\":%.3f},"
						"{\"adc\":%u,\"pH\":%.3f},"
						"{\"adc\":%u,\"pH\":%.3f}"
					"],"
					"\"adcMax\":%u,"
					"\"adcVoltage\":%.2f"
				"},"
				"\"network\":%s,"
				"\"cloudLoggingInterval\":%u"
			"}"),
			settings->temperatures.minimal,
			settings->temperatures.optimal,
			settings->temperatures.maximal,
			static_cast<unsigned int>(settings->circulator.activeTime / 1000),
			static_cast<unsigned int>(settings->circulator.pauseTime / 1000),
			(pump_ca.settings->hour * 60) + pump_ca.settings->minute, pump_ca.getMilliliters(),
			(pump_mg.settings->hour * 60) + pump_mg.settings->minute, pump_mg.getMilliliters(),
			(pump_kh.settings->hour * 60) + pump_kh.settings->minute, pump_kh.getMilliliters(),
			settings->phMeter.adc[0], settings->phMeter.pH[0],
			settings->phMeter.adc[1], settings->phMeter.pH[1],
			settings->phMeter.adc[2], settings->phMeter.pH[2],
			phMeter::analogMax, phMeter::analogMaxVoltage,
			Network::getConfigJSON().get(),
			static_cast<unsigned int>(CloudLogger::getInterval() / 1000)
		);
		if (ret < 0 || static_cast<unsigned int>(ret) >= bufferLength) {
			webServer.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
		}
		else {
			webServer.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
		}

		// As somebody connected, remove flag
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
		if (settings->prepareForSave()) {
			EEPROM.commit();
		}
		webServer.send(200);
	});

	webServer.on(F("/getColorsCycle"), Lighting::handleGetColors);
	webServer.on(F("/setColorsCycle"), Lighting::handleSetColors);
	webServer.on(F("/resetColorsCycle"), Lighting::handleResetColors);

	webServer.on(F("/mineralsPumps"), MineralsPumps::handleWebEndpoint);

	if constexpr (debugLevel >= LEVEL_DEBUG) {
		// Hidden API for testing propuses
		webServer.on(F("/test"), []() {
			webServer.send(204);
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

	MineralsPumps::updateForStop();

	UPDATE_EVERY(60000) {
		MineralsPumps::updateForStart();
	}

	webServer.handleClient();

	if (CloudLogger::isEnabled() && !MineralsPumps::pumping) {
		UPDATE_EVERY(CloudLogger::getInterval()) {
			CloudLogger::push({
				.waterTemperature = waterTemperature,
				.rtcTemperature = ds3231.getTemperature(),
				.phLevel = phMeter::getAverage(),
			});
		}
	}

	Circulator::update(currentMillis);

	UPDATE_EVERY(500) {
		Lighting::update();

		WaterLevel::update();

		phMeter::update();
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

		// Show WiFi status
		{
			lcd.print(' ');
			static bool blink;
			blink = !blink;
			switch (WiFi.getMode()) {
				case WIFI_STA: {
					if (WiFi.status() == WL_CONNECTED) {
						int8_t rssi = WiFi.RSSI();
						if (rssi > -65) {
							lcd.write(4);
							lcd.write(5);
							break;
						}
						else if (rssi > -70) {
							if (blink) {
								lcd.write(4);
								lcd.write(5);
							}
							else {
								lcd.write(3);
								lcd.write(' ');
							}
							break;
						}
						else if (rssi > -75) lcd.write(3);
						else if (rssi > -80) lcd.write(blink ? 3 : 2);
						else if (rssi > -85) lcd.write(2);
						else if (rssi > -95) lcd.write(blink ? 2 : 1);
						else /*           */ lcd.write(1);
						lcd.write(' ');
					}
					else {
						lcd.write(blink ? 3 : 'x');
						lcd.write(' ');
					}
					break;
				}
				case WIFI_AP: {
					lcd.write('A');
					lcd.write('P');
					break;
				}
				default:
					break;
			}
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

		// Show water termometer or pH level on LCD
		lcd.setCursor(20 - 6, 0);
		if (currentMillis % 32768 >= 16384) {
			char buf[8];
			sprintf(buf, "%.1f%cC", waterTemperature, 0b11011111);
			lcd.print(buf);
		}
		else {
			char buf[8];
			sprintf(buf, "pH %.2f", phMeter::getAverage());
			lcd.print(buf);
		}

		lcd.setCursor(0, 1);
		if (showIP) {
			// Show only IP in second row
			lcd.setCursor(0, 1);
			lcd.print(F("IP: "));
			char buf[16];
			ip_info info;
			Network::getIPInfo(info);
			sprintf(buf, "%u.%u.%u.%u", ip4_addr_printf_unpack(&info.ip));
			lcd.print(buf);
			if (millis() > showIPtimeout) {
				showIP = false;
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

			lcd.setCursor(20 - 4, 1);
			lcd.print(F("   "));

			// Show heating status
			char c = ' ';
			switch (Heating::getStatus()) {
				case Heating::Status::HEATING: c = 'G'; break; // G - Grzałka
				case Heating::Status::COOLING: c = 'W'; break; // W - Wiatrak
				default: break;
			}
			lcd.write(c); 
		}
	}
}
