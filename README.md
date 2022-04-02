
# Aquarium controller

##### Features

+ Summarized status display on 2x20 LED.
+ Portal website hosted over Wi-Fi with status and configuration.
+ Lights controls with day schedule configuration and smooth transitions.
+ Water conditioner, with heating controller by thermometer.
+ Water level detection and refilling.
+ Simple circulator controller (periodical)
+ Dosing minerals/micro-elements with pumps.
+ Water pH (acidity/basicity) meter (PH-4502C).
+ Cloud logging of various parameters.
+ Most of UI uses Polish language.





## Hardware

Hardware consists of:

+ [ESP8266 (NodeMCU ESP-12E) microcontroller](https://www.mischianti.org/2022/02/09/nodemcu-v3-high-resolution-pinout-and-specs/).
+ 2x20 LCD via I2C.
+ PCF8574 digital IO expander.
+ DS3231 clock.
+ DS18B20 thermometer (via OneWire).
+ More on-board specific electronics (incl. relays, transistors, passives).



### Schema

...



### Pins

#### Microcontroller

![NodeMCU V3 ESP-12E Pinout](https://www.mischianti.org/wp-content/uploads/2021/10/NodeMcu-V3-CH340-Lua-ESP8266-pinout-mischianti-low-resolution-1024x646.jpg)

| Pinout name | Pin in code | Mode    | Type    | Designation                   | Notes                        |
|-------------|-------------|---------|---------|-------------------------------|------------------------------|
| A0          | ADC0        | Input   | Analog  | Water ph meter                |                              |
| S3          | 10          | ?       | PWM     | Free                          | Free.                        |
| S2          | 9           | ?       | PWM     | Free                          | Free.                        |
| S1          | 8           | ?       | PWM     | Free                          | Free.                        |
| SC          | 11          | ?       | PWM     | Free                          | Free.                        |
| SO          | 7           | ?       | PWM     | Free                          | Free.                        |
| SK          | 6           | ?       | PWM     | Free                          | Free.                        |
| D0          | 16          | ?       | Digital | Free                          | Free, wake.                  |
| D1          | 5           | Special | Digital | SCL (I2C)                     |                              |
| D2          | 4           | Special | Digital | SDA (I2C)                     |                              |
| D3          | 0           | ?       | Digital | Free                          | Free, flash.                 |
| D4          | 2           | Special | Digital | OneWire / Built-in LED        | Used by DS18B20 thermometer. |
| D5          | 14          | Output  | PWM     | White LEDs                    |                              |
| D6          | 12          | Output  | PWM     | Blue LEDs                     |                              |
| D7          | 13          | Output  | PWM     | Red LEDs                      |                              |
| D8          | 15          | Output  | PWM     | Green LEDs                    |                              |
| RX          | 3           | Special | Digital | Serial read                   |                              |
| TX          | 1           | Special | Digital | Serial transmit               |                              |
|             |             |         |         |                               |                              |

#### IO expander

| Pin | Mode    | Designation                        | Notes                   |
|-----|---------|------------------------------------|-------------------------|
| 0   | Output  | Heating                            | `LOW` if heating.       |
| 1   | Output  | Water refilling                    | `LOW` if refilling.     |
| 2   | Output  | Minerals pump - Ca                 | `LOW` when pumping.     |
| 3   | Output  | Water circulator                   | `LOW` when active.      |
| 4   | Output  | Minerals pump - KH                 | `LOW` when pumping.     |
| 5   | Output  | Minerals pump - Mg                 | `LOW` when pumping.     |
| 6   | Input   | Water level detection              | `LOW` if low level.     |
| 7   | Input   | Backup water tank level detection  | `LOW` if low level.     |



### LCD

* While connecting to Wi-Fi:

	```
	.--------------------.
	|SSID: AAAAAAAAAAAAAA| <-- SSID being displayed.
	|IP: * - * - * - * - | <-- IP is not know yet, animation is displayed.
	'--------------------'
	```

* After connected, until IP showing timeout or anyone connected to web portal:

	```
	.--------------------.
	|HH:mm:ss      TT.T^C| <-- Time and water temperature.
	|IP:  AAA.BBB.CCC.DDD| <-- IP used by the controller.
	'--------------------'
	```

* Running:

	```
	.--------------------.
	|HH:mm:ss      TT.T^C| <-- Time and water temperature.
	|Message here...    G| <-- Custom message (like pumping in progress) and 'G' set or hidden whenever heating is on.
	'--------------------'
	```





## Software

[Platform IO](https://platformio.org/platformio-ide) is used with Arduino framework for development.



### Modules

* Web server,
* Lights controller,
* Heating controller,
* Circulator controller,
* Cloud logging controller,
* Minerals pumps controller,
* Water refilling controller,



### Web content

Web content (`web` directory) is prepared by `prepareWebArduino.js` script into `src/webEncoded` as compressed, allowing easy embedding in web server code with simple macros.

#### Web API

| Request                    | Description                                                                              |
|----------------------------|------------------------------------------------------------------------------------------|
| `/status`                  | Returns JSON with status object.                                                         |
| `/config` (`?=...`)        | Updates config values (from query params) and/or returns JSON with current config.       |
| `/saveEEPROM`              | Saves any config and persistent data (incl. colors cycles) to EEPROM.                    |
|                            |                                                                                          |
| `/getColorsCycle`          | Returns JSON with colors cycle object.                                                   |
| `/setColorsCycle` (POST)   | Updates colors cycle from encoded value send via POST (see sources for details).         |
| `/resetColorCycle`         | Resets colors cycle to defaults.                                                         |
|                            |                                                                                          |
| `/mineralsPumps` (`?=...`) | Allows for minerals pumps controlling (handy for calibration) and returns run time info. |
|                            |                                                                                          |

##### GET `/status`

Reports status.

Returns:
```json
{
	"waterTemperature": 22.01,
	"rtcTemperature": 22.25,

	"phLevel": 7.1234,
	"phRaw": 1024,
	
	"red": 0,
	"green": 127,
	"blue": 255,
	"white": 0,
	
	"isHeating": true,
	"isRefilling": false,
	"isRefillTankLow": false,
	
	"timestamp": "2022-03-13T16:01:17",
	"rssi": -67
}
```

##### GET `/config`

Allow changing configuration via passed arguments in query.

Arguments (should be joined with `&` to form querystring):
```
timestamp=2004-02-12T15:19:21

red=255
green=255
blue=255
white=255
forceColors=true

heatingMinTemperature=22.25
heatingMaxTemperature=23.75

circulatorActiveTime=10
circulatorPauseTime=10

mineralsPumps.ca.time=570
mineralsPumps.ca.mL=10
mineralsPumps.ca.c=4200.013
mineralsPumps.mg.time=570
mineralsPumps.mg.mL=10
mineralsPumps.mg.c=4200.013
mineralsPumps.kh.time=570
mineralsPumps.kh.mL=10
mineralsPumps.kh.c=4200.013

phMeter.points.0.adc
phMeter.points.0.pH
phMeter.points.1.adc
phMeter.points.1.pH
phMeter.points.2.adc
phMeter.points.2.pH

cloudLoggingInterval=10
```

Returns:
```json
{
	"heatingMinTemperature": 22.25,
	"heatingMaxTemperature": 23.75,
	
	"circulatorActiveTime": 10,
	"circulatorPauseTime":  10,

	"cloudLoggingInterval": 10,

	"mineralsPumps": {
		"ca": { "time": 570, "mL": 10 },
		"mg": { "time": 570, "mL": 10 },
		"kh": { "time": 570, "mL": 10 }
	},

	"phMeter": {
		"points": [
			{ "adc": 800, "pH": 7.11 },
			{ "adc": 800, "pH": 7.11 },
			{ "adc": 800, "pH": 7.11 }
		],
		"adcMax": 1024,
		"adcVoltage": 3.2,
	}
}
```

#### I2C addresses

* `0x26` - Digital IO expander (PCF8574)
* `0x27` - LCD
* `0x57` - DS3231 Memory?
* `0x68` - DS3231 RTC

#### EEPROM structure

Size: `0x1000` (4kB)

* `0x000` - `0x010`:

	* `0x000` - `0x007` bytes - nothing (8 bytes of nothing at position 0)
	* `0x008` byte - cloud logging settings (interval in seconds or seconds (MSB set for minutes), up to 127 or 0 if disabled)
	* `0x009` byte - unused for now
	* `0x00A` byte - maximum temperature* for heating controller
	* `0x00B` byte - minimal temperature* for heating controller 
	* `0x00C` & `0x00D` bytes - circulator active period in seconds
	* `0x00E` & `0x00F` bytes - circulator pause period in minutes 

\* - temperatures encoded as `abs(floor(temperature * 100)) / 25`, decoded as `value * 25 / 100`.

* `0x010` - `0x080` - smooth light controller entries

	Entry format: 
	* 1st byte - hour
	* 2nd byte - minute
	* 3rd byte - red light PWM value
	* 4th byte - green light PWM value
	* 5th byte - blue light PWM value
	* 6th byte - while light PWM value

	It means there are `14` customizable entries.

* `0x200` - `0x218` - minerals pumps settings:

	Format:
	* 4 bytes - calibration float.
	* 1 byte - hour number (pump disabled if outside of 0-24).
	* 1 byte - minute number.
	* 1 byte - millimeters dose (so max dose is 250-255mL).
	* 1 byte padding (to round up at 8 bytes).

	Three pumps, total of 24 bytes.

* `0x220` - `0x230` - pH meter settings (calibration).

	Format:
	* 3 calibration points, each 8 bytes:
		* 4 bytes - pH level (float).
		* 2 bytes - ADC raw value read for given pH.
		* 2 bytes padding.
	* 1 byte for interpolation method selection enum.
	* The rest is padding.

#### Cloud logger

There is HTTPS client running every configured interval sending the request with readings sample to endpoint. As the endpoint, [Google Apps Script with `POST` handler](https://script.google.com/d/1MDprbKPWUi1Kno0x6o2YVEOm3dEMGe_TI3PfwGwiD1rW21l4PcxbYVoA/edit?usp=sharing) is used. Data is stored into [Google Spreadsheet document](https://docs.google.com/spreadsheets/d/1OeXW_dhXnBcgqe8lflZT3rbdBoCy9qz3bURGQ95YH9o/edit?usp=sharing#gid=581325308). 





## TODO

+ Add favicon for web (https://icons8.com/icons/set/aquarium-favicon)
+ Write rest of API docs.
+ Make `/colorsCycle` endpoint instead separate `set/get`. RESTful somewhat, GET/POST.
+ Use SSL sessions (to avoid partially lag of cloud logger).
	
	See https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/bearssl-client-secure-class.html?highlight=WiFiClientSecure

+ Use X509 keys for SSL. Also maybe allow less secure, but faster SSL. 

	See https://github.com/esp8266/Arduino/blob/448486a4c9db1e74a60fa922c8388116c01c5f2b/libraries/ESP8266WiFi/examples/BearSSL_Validation/BearSSL_Validation.ino

+ Optional color slider transformation (illusion of control, first parts of slider affect lesser part of raw value).
+ Restructure EEPROM.
+ Store Wi-Fi settings (SSID/password) in EEPROM. 
+ Store cloud logger settings in EEPROM (incl. URL and fingerprint).
+ Password protection for web panel (store in EEPROM).
+ Add security token to cloud communication.
+ Include cloud endpoint code (Google App Scripts) here.
+ Fit colors cycle configuration on mobile nicely, somehow.
+ Circulator force run button.
+ Extend RTC with milliseconds (sync on start and use `millis`); Use extended RTC for smoother LEDs transitions.
+ Add fancy animations on demand for LEDs.
+ Auto-update SSL certs fingerprints?
+ Proper seeding code (detect with `#include <coredecls.h> // crc32` or https://pl.wikipedia.org/wiki/Adler-32), incl. WiFi (http://arduino.esp8266.com/Arduino/versions/2.0.0/doc/libraries.html#wifi-esp8266wifi-library)
+ RESTful API (see https://github.com/cesanta/mjson)
+ Show total running time (from last power-on), under current time at web portal.
+ Try overclock ESP into 160 MHz.
+ Add polynomial/Newton interpolation for calculating ph level.
+ Temperature compensation for ph calculation.
+ Chart link/button for last hour/day should show error if no data to show.
+ Add loading/error information for line charts.
+ Cloud logger buffer (configurable; in such case charts could get data from status too?).
+ Don't remove unchanged files at `prepareWebArduino` (support directory tree).
+ Add `--watch` to `prepareWebArduino`.
+ Water wind controller (nie ma PWM, ale może krótkie pulsy?).
+ Exportable/importable colour cycle configuration (as JSON/encoded?) .
+ Saving/reading colour cycle presets from browser storage.
+ Refactor DS3231 library (lighter one, use full DS3231 instead pieces).
+ Use `mimetable.cpp` for MIME types? No. Not until ESP8266 core impl fixes 3-4 times copying. Also, `getContentType` is copying even more XD.
+ Standard Arduino/ESP core libraries are bad... 

	- Stream using int instead char?
	- Lack or invalid comments.
	- Inconsistent namings.
	- Formatting even worse than C++ STD.
	- Paying for unused features.

+ Use `snprintf_P` in place of `snprintf`? SRAM is faster than PROGMEM tho.


