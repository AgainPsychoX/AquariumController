
////////////////////////////////////////////////////////////////////////////////

#include "common.hpp"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> 
#include <DS3231.h>
#include <DallasTemperature.h> // for DS18B20 thermometer
#include <PCF8574.hpp>

PCF8574 ioExpander {0x26};
constexpr byte waterLevelDetectorPin = 7; // on the extender
constexpr byte waterLevelRefillerPin = 1; // on the extender
constexpr byte waterLevelBackupTankDetectorPin = 6; // on the extender
bool waterLevelRefillingRequired = false;
bool waterLevelBackupTankLow = false;
OneWire oneWire(2);
struct {
	inline void on()  { digitalWrite(D4, LOW); }
	inline void off() { digitalWrite(D4, HIGH); }
	inline void setup() { pinMode(D4, OUTPUT); }
} buildInLed;
ESP8266WebServer server(80);
LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01);
DS3231 ds3231;
RTClib rtc;
DallasTemperature oneWireThermometers(&oneWire);
float waterTemperature = 0; // Noise reduction

#include "webEncoded/AllStaticContent.hpp"
#include "PWMs.hpp"
#include "HeatingController.hpp"
#include "CirculatorController.hpp"
#include "CloudLogger.hpp"
#include "MineralsPumpsController.hpp"

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

	// Register server handlers
	server.on("/", []() {
		WEB_USE_CACHE_STATIC(server);
		WEB_USE_GZIP_STATIC(server);
		server.send(200, WEB_CONTENT_TYPE_TEXT_HTML, WEB_index_html, sizeof(WEB_index_html));
	});
	WEB_REGISTER_ALL_STATIC(server);

	server.on("/status", []() {
		constexpr unsigned int bufferLength = 256;
		char buffer[bufferLength];
		DateTime now = rtc.now();
		int ret = snprintf(
			buffer, bufferLength,
			"{"
				"\"waterTemperature\":%.2f,"
				"\"rtcTemperature\":%.2f,"
				"\"phLevel\":%.2f,"
				"\"red\":%d,\"green\":%d,\"blue\":%d,\"white\":%d,"
				"\"isHeating\":%d,"
				"\"isRefilling\":%d,"
				"\"isRefillTankLow\":%d,"
				"\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\","
				"\"rssi\":%d"
			"}",
			waterTemperature,
			ds3231.getTemperature(),
			7.11f,
			redPWM.get(), greenPWM.get(), bluePWM.get(), whitePWM.get(),
			HeatingController::isHeating(),
			waterLevelRefillingRequired,
			waterLevelBackupTankLow,
			// Format should be like: "2004-02-12T15:19:21" (without time zones)
			now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
			WiFi.RSSI()
		);
		if (ret < 0 || static_cast<unsigned int>(ret) >= bufferLength) {
			server.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
		}
		else {
			server.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
		}
	});
	server.on("/config", []() {
		// Handle config options
		if (const String& str = server.arg("timestamp"); !str.isEmpty()) {
			// Format should be like: "2004-02-12T15:19:21" (without time zones)
			const char* cstr = str.c_str();
			ds3231.setYear(  atoi(cstr + 0) % 100);
			ds3231.setMonth( atoi(cstr + 5));
			ds3231.setDate(  atoi(cstr + 8));
			ds3231.setHour(  atoi(cstr + 11));
			ds3231.setMinute(atoi(cstr + 14));
			ds3231.setSecond(atoi(cstr + 17));
		}

		if (const String& str = server.arg("red");      !str.isEmpty())   redPWM.set(atoi(str.c_str()));
		if (const String& str = server.arg("green");    !str.isEmpty()) greenPWM.set(atoi(str.c_str()));
		if (const String& str = server.arg("blue");     !str.isEmpty())  bluePWM.set(atoi(str.c_str()));
		if (const String& str = server.arg("white");    !str.isEmpty()) whitePWM.set(atoi(str.c_str()));
		if (const String& str = server.arg("forceColors"); !str.isEmpty()) SmoothTimedPWM::disable = server.arg("forceColors").equals("true");

		// Handle heating config options
		{
			bool changes = false;
			if (server.hasArg("heatingMinTemperature")) {
				HeatingController::minTemperature = atof(server.arg("heatingMinTemperature").c_str());
				changes = true;
			}
			if (server.hasArg("heatingMaxTemperature")) {
				HeatingController::maxTemperature = atof(server.arg("heatingMaxTemperature").c_str());
				changes = true;
			}
			if (changes) {
				HeatingController::saveSettings();
			}
		}

		// Handle circulator config options
		{
			bool changes = false;
			if (server.hasArg("circulatorActiveTime")) {
				CirculatorController::activeSeconds = atoi(server.arg("circulatorActiveTime").c_str()) % 65536;
				changes = true;
			}
			if (server.hasArg("circulatorPauseTime")) {
				CirculatorController::pauseSeconds = atoi(server.arg("circulatorPauseTime").c_str()) % 65536;
				changes = true;
			}
			if (changes) {
				CirculatorController::nextUpdateTime = millis() + 1000;
				CirculatorController::saveSettings();
			}
		}

		// Handle minerals pumps config
		{
			using namespace MineralsPumpsController;
			bool changes = false;

			String arg;
			arg.reserve(24);
			arg = F("mineralsPumps.ca.time");
			constexpr uint8_t keyOffset = 14;

			for (uint8_t i = 0; i < Mineral::Count; i++) {
				const char* key = pumpsKeys[i];
				arg[keyOffset + 0] = key[0];
				arg[keyOffset + 1] = key[1];
				const String& value = server.arg(arg);
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
				const String& value = server.arg(arg);
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
				const String& value = server.arg(arg);
				if (!value.isEmpty()) {
					const float c = atof(value.c_str());
					pumps[i].setCalibration(c);
					changes = true;
				}
			}

			if (changes) {
				MineralsPumpsController::saveSettings();
				MineralsPumpsController::printSettings();
			}
		}

		if (const String& str = server.arg("cloudLoggingInterval"); !str.isEmpty()) {
			CloudLogger::interval = atoi(str.c_str()) * 1000;
			CloudLogger::saveSettings();
		}

		// Response with current config
		const auto& pump_ca = MineralsPumpsController::pumps[MineralsPumpsController::Mineral::Ca];
		const auto& pump_mg = MineralsPumpsController::pumps[MineralsPumpsController::Mineral::Mg];
		const auto& pump_kh = MineralsPumpsController::pumps[MineralsPumpsController::Mineral::KH];
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
			HeatingController::minTemperature,
			HeatingController::maxTemperature,
			CirculatorController::activeSeconds,
			CirculatorController::pauseSeconds,
			(pump_ca.settings.hour * 60) + pump_ca.settings.minute, pump_ca.getMilliliters(),
			(pump_mg.settings.hour * 60) + pump_mg.settings.minute, pump_mg.getMilliliters(),
			(pump_kh.settings.hour * 60) + pump_kh.settings.minute, pump_kh.getMilliliters(),
			static_cast<unsigned int>(CloudLogger::interval / 1000)
		);
		if (ret < 0 || static_cast<unsigned int>(ret) >= bufferLength) {
			server.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
		}
		else {
			server.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
		}

		// As somebody connected, remove flag
		if (showIP) {
			lcd.clear();
		}
		showIP = false;
	});
	// Built-in LED should not be activated outside early setup, as OneWire uses the same pin.
	// server.on("/led", []() {
	//     if (server.args() > 0) {
	//         buildInLed.on();
	//     }
	//     else {
	//         buildInLed.off();
	//     }
	//     server.send(200);
	// });
	server.on("/test", []() {
		constexpr unsigned int bufferLength = 256;
		char buffer[bufferLength];

		// Minerals pumps testing
		{
			using namespace MineralsPumpsController;
			const String& pump = server.arg("pump");
			if (!pump.isEmpty()) {
				const String& action = server.arg("action");
				const bool on     = action.equals("on");
				const bool dose   = action.equals("dose");
				const bool active = on || dose;
				const bool set    = active || action.equals("off");
				const bool manual = on && !dose;
				if (set) {
					Mineral which = Mineral::Count;
					/**/ if (pump.equals("ca")) which = Mineral::Ca;
					else if (pump.equals("mg")) which = Mineral::Mg;
					else if (pump.equals("kh")) which = Mineral::KH;
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
					server.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
				}
				else {
					server.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
				}
				return;
			}
		}

		server.send(200);
	});
	server.on("/saveEEPROM", []() {
		EEPROM.commit();
		server.send(200);
	});
	server.on("/getColorsCycle", SmoothTimedPWM::getColorsHandler);
	server.on("/setColorsCycle", SmoothTimedPWM::setColorsHandler);
	server.on("/resetColorsCycle", SmoothTimedPWM::resetColorsHandler);
	server.onNotFound([]() {
		server.send(404, WEB_CONTENT_TYPE_TEXT_PLAIN, "Not found\n\n");
	});
	server.begin();

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

	// Initialize smooth timed PWM controller
	SmoothTimedPWM::setup();

	// Initialize water heating controller
	HeatingController::setup();

	// Initialize water heating controller
	CirculatorController::setup();

	// Initialize minerals pumps controller
	MineralsPumpsController::setup();

	// Initialize cloud logger service
	CloudLogger::setup();
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

	MineralsPumpsController::update();

	server.handleClient();

	if (CloudLogger::isEnabled() && !MineralsPumpsController::pumping) {
		UPDATE_EVERY(CloudLogger::interval) {
			CloudLogger::push({
				.waterTemperature = waterTemperature,
				.rtcTemperature = ds3231.getTemperature(),
				.phLevel = 7.11f
			});
		}
	}

	UPDATE_EVERY(500) {
		SmoothTimedPWM::update();

		CirculatorController::update();

		// Water level refiller
		{
			waterLevelRefillingRequired = ioExpander.digitalRead(waterLevelDetectorPin) == LOW;
			ioExpander.digitalWrite(waterLevelRefillerPin, waterLevelRefillingRequired ? LOW : HIGH);

			waterLevelBackupTankLow = ioExpander.digitalRead(waterLevelBackupTankDetectorPin) == LOW;
		}
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

		HeatingController::update(waterTemperature);

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
			using namespace MineralsPumpsController;
			if (MineralsPumpsController::pumping) {
				lcd.print(F("Dozowanie "));
				switch (MineralsPumpsController::which) {
					case Mineral::Ca:	lcd.print(F("Ca... "));	break;
					case Mineral::Mg:	lcd.print(F("Mg... "));	break;
					case Mineral::KH:	lcd.print(F("KH... "));	break;
					default: break;
				}
			}
			else if (waterLevelBackupTankLow) {
				lcd.print(F("Niski poziom RO!"));
			}
			else if (waterLevelRefillingRequired) {
				lcd.print(F("Dolewanie RO... "));
			}
			else {
				lcd.print(F("                "));
			}

			//lcd.setCursor(20 - 1, 1);
			lcd.print("   ");

			// Show heating status
			lcd.write(HeatingController::isHeating() ? 'G' : ' '); // G - Grzałka
		}
	}
}
