
////////////////////////////////////////////////////////////////////////////////
// Kontroler akwarium na układzie ESP8266 (NodeMCUv3)
//

// Ustawienia WiFi:
const char* ssid = "TP-LINK_Jacek";
const char* password = "kamildanielpatryk";
#define CLOUDLOGGER_INSECURE 0

// Debugowanie
#define DEBUG 1
#define DEBUG_PWM_SMOOTH_TIMED_CONTROLLER 2
#define DEBUG_HEATING_CONTROLLER 2
#define DEBUG_CIRCULATOR_CONTROLLER 2
#define DEBUG_CLOUD_LOGGER 2

////////////////////////////////////////////////////////////////////////////////
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

bool showIP = true;
constexpr unsigned int showIPtimeout = 15000;

////////////////////////////////////////////////////////////////////////////////

void setup() {
	// Initialize Serial console
	Serial.begin(115200);

	// Initialize I2C Wire protocol
	Wire.begin();

	// Initialize I/O extender
	ioExpander.state = 0b00001111;
	ioExpander.write();

	// Initialize RTC
	{
		delay(100);
		DateTime now = rtc.now();

		// Check for error
		if (now.hour() > 24 || now.minute() > 60) {
			buildInLed.setup();
			while (true) {
				Serial.println(F("RTC failed, please reset its memory!"));
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
		Serial.print(F("Time is "));
		if (now.hour() < 10) {
			Serial.print('0');
		}
		Serial.print(now.hour());
		Serial.print(':');
		if (now.minute() < 10) {
			Serial.print('0');
		}
		Serial.print(now.minute());
		Serial.print(':');
		if (now.second() < 10) {
			Serial.print('0');
		}
		Serial.print(now.second());
		Serial.println(" ");
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

		Serial.println(F("connected!"));
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
		constexpr unsigned int buffLen = 256;
		char response[buffLen];
		DateTime now = rtc.now();
		int ret = snprintf(
			response, buffLen,
			"{"
				"\"waterTemperature\":%.2f,"
				"\"airTemperature\":%.2f,"
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
		if (ret < 0 || static_cast<unsigned int>(ret) >= buffLen) {
			server.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
		}
		server.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, response);
	});
	server.on("/config", []() {
		// Handle config options
		if (server.hasArg("timestamp")) {
			// Format should be like: "2004-02-12T15:19:21" (without time zones)
			const char* cstr = server.arg("timestamp").c_str();
			ds3231.setYear(  atoi(cstr + 0) % 100);
			ds3231.setMonth( atoi(cstr + 5));
			ds3231.setDate(  atoi(cstr + 8));
			ds3231.setHour(  atoi(cstr + 11));
			ds3231.setMinute(atoi(cstr + 14));
			ds3231.setSecond(atoi(cstr + 17));
		}
		if (server.hasArg("red")) {
			byte value = atoi(server.arg("red").c_str()) % 256;
			redPWM.set(value);
		}
		if (server.hasArg("green")) {
			byte value = atoi(server.arg("green").c_str()) % 256;
			greenPWM.set(value);
		}
		if (server.hasArg("blue")) {
			byte value = atoi(server.arg("blue").c_str()) % 256;
			bluePWM.set(value);
		}
		if (server.hasArg("white")) {
			byte value = atoi(server.arg("white").c_str()) % 256;
			whitePWM.set(value);
		}
		if (server.hasArg("forceColors")) {
			SmoothTimedPWM::disable = server.arg("forceColors").equals("true");
		}
		if (server.hasArg("cloudLoggingInterval")) {
			CloudLogger::interval = atoi(server.arg("cloudLoggingInterval").c_str()) * 1000;
			CloudLogger::saveSettings();
		}

		// Handle heating config options
		{
			bool heatingConfigChanged = false;
			if (server.hasArg("heatingMinTemperature")) {
				HeatingController::minTemperature = atof(server.arg("heatingMinTemperature").c_str());
#if DEBUG_HEATING_CONTROLLER >= 1
			Serial.print(F("HTTP /config? () HeatingController minTemperature: "));
			Serial.println(HeatingController::minTemperature);
#endif
				heatingConfigChanged = true;
			}
			if (server.hasArg("heatingMaxTemperature")) {
				HeatingController::maxTemperature = atof(server.arg("heatingMaxTemperature").c_str());
#if DEBUG_HEATING_CONTROLLER >= 1
			Serial.print(F("HTTP /config? () HeatingController maxTemperature: "));
			Serial.println(HeatingController::maxTemperature);
#endif
				heatingConfigChanged = true;
			}
			if (heatingConfigChanged) {
				HeatingController::saveSettings();
			}
		}

		// Handle circulator config options
		{
			bool configChanged = false;
			if (server.hasArg("circulatorActiveTime")) {
				CirculatorController::activeSeconds = atoi(server.arg("circulatorActiveTime").c_str()) % 65536;
#if DEBUG_CIRCULATOR_CONTROLLER >= 1
			Serial.print(F("HTTP /config? () CirculatorController activeSeconds: "));
			Serial.println(CirculatorController::activeSeconds);
#endif
				configChanged = true;
			}
			if (server.hasArg("circulatorPauseTime")) {
				CirculatorController::pauseSeconds = atoi(server.arg("circulatorPauseTime").c_str()) % 65536;
#if DEBUG_CIRCULATOR_CONTROLLER >= 1
			Serial.print(F("HTTP /config? () CirculatorController pauseSeconds: "));
			Serial.println(CirculatorController::pauseSeconds);
#endif
				configChanged = true;
			}
			if (configChanged) {
				CirculatorController::nextUpdateTime = millis() + 1000;
				CirculatorController::saveSettings();
			}
		}

		// Response with current config
		constexpr unsigned int buffLen = 256;
		char response[buffLen];
		int ret = snprintf(
			response, buffLen,
			"{"
				"\"heatingMinTemperature\":%.2f,"
				"\"heatingMaxTemperature\":%.2f,"
				"\"circulatorActiveTime\":%u,"
				"\"circulatorPauseTime\":%u,"
				"\"cloudLoggingInterval\":%u"
			"}",
			HeatingController::minTemperature,
			HeatingController::maxTemperature,
			CirculatorController::activeSeconds,
			CirculatorController::pauseSeconds,
			static_cast<unsigned int>(CloudLogger::interval / 1000)
		);
		if (ret < 0 || static_cast<unsigned int>(ret) >= buffLen) {
			server.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
		}
		server.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, response);

		// As somebody connected, remove flag
		if (showIP) {
			lcd.clear();
		}
		showIP = false;
	});
	// server.on("/led", []() {
	//     if (server.args() > 0) {
	//         buildInLed.on();
	//     }
	//     else {
	//         buildInLed.off();
	//     }
	//     server.send(200);
	// });
	server.on("/saveEEPROM", []() {
		EEPROM.commit();
		server.send(200);
	});
	server.on("/getColorsCycle", SmoothTimedPWM::getColorsHandler);
	server.on("/pushColorsCycle", SmoothTimedPWM::pushColorsHandler);
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

	server.handleClient();

	if (CloudLogger::isEnabled()) {
		UPDATE_EVERY(CloudLogger::interval) {
			CloudLogger::push({
				.waterTemperature = waterTemperature,
				.airTemperature = ds3231.getTemperature(),
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

		// Show only IP in second row
		if (showIP) {
			lcd.setCursor(0, 1);
			lcd.print("IP: ");
			lcd.print(WiFi.localIP().toString().c_str());
			if (millis() > showIPtimeout) {
				showIP = false;
				lcd.clear();
			}
		}
		else {
			// Show status for refilling and heating
			lcd.setCursor(20 - 4, 1);
			if (waterLevelBackupTankLow && (millis() % 2000 > 1000)) {
				lcd.print("  ");
			}
			else {
				lcd.print(waterLevelRefillingRequired ? "RO" : "  ");
			}
			lcd.write(' ');
			lcd.write(HeatingController::isHeating() ? 'G' : ' '); // G - Grzałka
		}
	}
}
