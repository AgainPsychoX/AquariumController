
# Aquarium controller

##### Features

+ Summarized status display on 2x20 LED.
+ Portal website hosted over Wi-Fi with status and configuration.
+ Lights controls with day schedule configuration and smooth transitions.
+ Water conditioner, with heating/cooling controller by thermometer.
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
| D0          | 16          | ?       | Digital | Water cooling                 | Wake/sleep unusable anymore. |
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
| 0   | Output  | Water heating                      | `LOW` if heating.       |
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
	|HH:mm:ss @@   TT.T^C| <-- Time, WiFi status and water temperature or water pH level.
	|Message here...    G| <-- Custom message (like pumping in progress) and 'G' set or hidden whenever heating is on.
	'--------------------'
	```

	```
	.--------------------.
	|HH:mm:ss AP   pH 7.1|
	|Message here...    G|
	'--------------------'
	```





## Software

[Platform IO](https://platformio.org/platformio-ide) is used with Arduino framework for development.



### Modules

* Web server,
* Network code (connect to configured network, incl. IP configuration; or host AP)
* Lights controller,
* Water temperature controller (heating/cooling),
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
	
	"heatingStatus": 2,
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

waterTemperatures.minimal=22.50
waterTemperatures.optimal=24.00
waterTemperatures.maximal=25.50

circulatorActiveTime=10
circulatorPauseTime=5

mineralsPumps.ca.time=570
mineralsPumps.ca.mL=10
mineralsPumps.ca.c=412.000
mineralsPumps.mg.time=570
mineralsPumps.mg.mL=10
mineralsPumps.mg.c=420.310
mineralsPumps.kh.time=570
mineralsPumps.kh.mL=10
mineralsPumps.kh.c=397.016

phMeter.points.0.adc=712
phMeter.points.0.pH=9.20
phMeter.points.1.adc=851
phMeter.points.1.pH=6.88
phMeter.points.2.adc=1008
phMeter.points.2.pH=4.03

network.mode=2
ssid=Hotspot
psk=12345678
static=1
ip=192.168.1.123
mask=24
gateway=192.168.1.1
dns1=1.1.1.1
dns2=1.0.0.1

cloudLoggingInterval=10
```

Returns:
```json
{
	"waterTemperatures": {
		"minimal": 22.50,
		"optimal": 24.00,
		"maximal": 25.50
	},
	
	"circulatorActiveTime": 10,
	"circulatorPauseTime":  10,

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
		"adcVoltage": 3.2
	},

	"network": {
		"mode": 2,
		"ssid": "Hotspot",
		"psk": "12345678",
		"static": 1,
		"ip": "192.168.55.249",
		"mask": 24,
		"gateway": "192.168.55.1",
		"dns1": "192.168.55.1",
		"dns2": "1.1.1.1"
	},

	"cloudLoggingInterval": 10,
}
```

#### I2C addresses

* `0x26` - Digital IO expander (PCF8574)
* `0x27` - LCD
* `0x57` - DS3231 Memory?
* `0x68` - DS3231 RTC

#### EEPROM structure

Size: `0x1000` (4kB), used `0x200` (512b).

* `0x000` - `0x020` - Empty header & checksum.

	* `0x01C` - `0x020` - CRC32 checksum.

* `0x020` - `0x030` - Water temperature.

	* `0x020` - `0x024` (`float`) - Minimum temperature (lowers starts heating).
	* `0x024` - `0x028` (`float`) - Optimal temperature (stops heating or fans).
	* `0x028` - `0x02C` (`float`) - Maximal temperature (above starts fans).

* `0x030` - `0x040` - Water circulation.

	* 1 byte - Circulator active time start (hour number).
	* 1 byte - Circulator active time start (minute number).
	* 1 byte - Circulator active hours end.
	* 1 byte - Circulator active hours end.
	* 4 bytes - Circulator active period in milliseconds.
	* 4 bytes - Circulator pause period in milliseconds. 

* `0x040` - `0x060` - pH meter settings (calibration).

	* `0x040`, `0x044`, `0x048`, `0x04C`, `0x050` (5x `float`) - Defined pH levels.
	* `0x054`, `0x056`, `0x058`, `0x05A`, `0x05C` (5x `u16`) - ADC readings for defined levels.
	* `0x05E` (`u8`) - Number of points.
	* `0x05F` (`u8`) - Interpolation method selection enum.

* `0x060` - `0x080` - Minerals pumps settings.

	Format:
	* 4 bytes - calibration float.
	* 1 byte - hour number (pump disabled if outside of 0-24).
	* 1 byte - minute number.
	* 1 byte - millimeters dose (so max dose is 250-255mL).
	* 1 byte padding (to round up at 8 bytes).

	Three pumps, total of 24 bytes.

* `0x080` - `0x100` - Smooth light controller entries.

	Entry format: 
	* 1st byte - hour
	* 2nd byte - minute
	* 3rd byte - red light PWM value
	* 4th byte - green light PWM value
	* 5th byte - blue light PWM value
	* 6th byte - while light PWM value
	* 2 bytes padding (round up to 8 bytes).

	It means there are `16` customizable entries.

* `0x100` - `0x160` - Some network settings.

	Some SSID, PSK and some mode specific settings are saved internally by SDK, therefore not included here. 
	IPv4 fields: 192.168.4.1 == 0x0104A8C0.

	* 1 byte - mode/flags (disabled/always ap/station/fallback ap; whenever to use static ip or not).
	* 4 bytes - IPv4 for device.
	* 4 bytes - Network mask.
	* 4 bytes - Gateway IP.
	* 4 bytes - IPv4 for DNS 1.
	* 4 bytes - IPv4 for DNS 2, or 0 if none.
	* 8 bytes of padding.
	* 64 bytes - basic authorization token, encoded (base64 of `login:password`).

* `0x160` - `0x200` - Cloud settings.

	* 16 bytes - secret, to authorize and/or identify the controller (needs to end with \0).
	* 4 bytes - interval for cloud logger (in milliseconds).
	* 12 bytes padding.
	* `0x180` - `0x200` - cloud endpoint URL.

#### Cloud logger

There is HTTPS client running every configured interval sending the request with readings sample to endpoint. As the endpoint, [Google Apps Script with `POST` handler](https://script.google.com/d/1MDprbKPWUi1Kno0x6o2YVEOm3dEMGe_TI3PfwGwiD1rW21l4PcxbYVoA/edit?usp=sharing) is used. Data is stored into [Google Spreadsheet document](https://docs.google.com/spreadsheets/d/1OeXW_dhXnBcgqe8lflZT3rbdBoCy9qz3bURGQ95YH9o/edit?usp=sharing#gid=581325308). 





## TODO

+ WiFi reconnecting.
+ WiFi settings graying out if DHCP is used and show mask as IP.
+ Make `/config` endpoint use proper GET/POST to avoid lag of printing so much when only setting.
+ Make `/colorsCycle` endpoint instead separate `set/get`. RESTful somewhat, GET/POST.
+ RESTful API (see https://arduinojson.org/ or https://github.com/cesanta/mjson)
+ Write rest of API docs.
+ Add favicon for web (https://icons8.com/icons/set/aquarium-favicon)
+ Optional color slider transformation (illusion of control, first parts of slider affect lesser part of raw value).
+ Add circulator night mode, with settable hour:minute (already in EEPROM structure, but unused).
+ Add feeding mode - circulator disable (and maybe special lighting?)
+ Try soft-PWM on io expander (rapid switching? https://www.youtube.com/watch?v=Fsb7kxDxYGw http://sim.okawa-denshi.jp/en/PWMtool.php).
+ Add security token to cloud communication.
+ Store cloud logger settings in EEPROM (incl. URL and secret) (already in EEPROM structure, but unused).
+ Include cloud endpoint code (Google App Scripts) here.
+ Circulator force run button.
+ Extend RTC with milliseconds (sync on start and use `millis`); Use extended RTC for smoother LEDs transitions.
+ Add fancy animations on demand for LEDs.
+ Show total running time (from last power-on), under current time at web portal.
+ Try overclock ESP into 160 MHz.
+ Use better web server library (https://github.com/me-no-dev/ESPAsyncWebServer).
+ Use HTTPS for web server.
+ Use some authorization for web panel config setting.
+ Reuse light cycle 2 bytes instead of hour/minute start/stop bytes for things like pumps or circulator.
+ Add polynomial/Newton interpolation for calculating ph level.
+ Temperature compensation for ph calculation.
+ Chart link/button for last hour/day should show error if no data to show.
+ Add loading/error information for line charts.
+ Cloud logger buffer (configurable; in such case charts could get data from status too?).
+ Don't remove unchanged files at `prepareWebArduino` (support directory tree).
+ Add `--watch` to `prepareWebArduino`.
+ Exportable/importable colour cycle configuration (as JSON/encoded?) .
+ Saving/reading colour cycle presets from browser storage.
+ Refactor DS3231 library (lighter one, use full DS3231 instead pieces).
+ Use `mimetable.cpp` for MIME types? No. Not until ESP8266 core impl fixes 3-4 times copying. Also, `getContentType` is copying even more XD.
+ Standard Arduino/ESP core libraries are bad... I want to do better, but shitty foundations make it futile or too hard.

	- Stream using int instead char?
	- Lack or invalid comments.
	- Inconsistent namings.
	- Formatting even worse than C++ STD.
	- Paying for unused features.

+ Use `snprintf_P` in place of `snprintf`? SRAM is faster than PROGMEM tho.


