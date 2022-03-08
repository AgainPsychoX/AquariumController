
# Aquarium controller

##### Features

+ Summarized status display on 2x20 LED.
+ Portal website hosted over Wi-Fi with status and configuration.
+ LED controls with day schedule configuration and smooth transitions.
+ Water conditioner, with heating controller by thermometer.
+ Water level detection and refilling.
+ Simple circulator controller (periodical)
+ Water pH (acidity/basicity) meter.
+ Cloud logging of various parameters.





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
| 2   | ?       | ?                                  | Free.                   |
| 3   | Output  | Water circulator                   | `LOW` when active.      |
| 4   | ?       | ?                                  | Free.                   |
| 5   | ?       | ?                                  | Free.                   |
| 6   | Input   | Water level detection              | `LOW` if low level.     |
| 7   | Input   | Backup water tank level detection  | `LOW` if low level.     |





## Software

[Platform IO](https://platformio.org/platformio-ide) is used with Arduino framework for development.



### Notes

#### Web content

Web content (`web` directory) is prepared by `prepareWebArduino.js` script into `src/webEncoded` as compressed, allowing easy embedding in web server code with simple macros.

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

#### Cloud logger

There is HTTPS client running every configured interval sending the request with readings sample to endpoint. As the endpoint, [Google Apps Script with `POST` handler](https://script.google.com/d/1MDprbKPWUi1Kno0x6o2YVEOm3dEMGe_TI3PfwGwiD1rW21l4PcxbYVoA/edit?usp=sharing) is used. Data is stored into [Google Spreadsheet document](https://docs.google.com/spreadsheets/d/1OeXW_dhXnBcgqe8lflZT3rbdBoCy9qz3bURGQ95YH9o/edit?usp=sharing#gid=581325308). 





## TODO

+ Use SSL sessions (to avoid partially lag of cloud logger).
+ Store Wi-Fi settings (SSID/password) in EEPROM. 
+ Store cloud logger settings in EEPROM.
+ Cloud logger buffer (configurable; in such case charts could get data from status too?).
+ Configurable cloud log interval (and maybe timeout too?).
+ Sync chart refresh with cloud logger.
+ Add loading/error information for line charts.
+ Add security token to cloud communication.
+ Include electronic board schema in docs.
+ Water wind controller (nie ma PWM, ale może krótkie pulsy?).
+ Exportable/importable colour cycle configuration (as JSON?) .
+ Saving/reading colour cycle presets from browser storage.
+ Update SSL certs fingerprints.
+ Extend RTC with milliseconds (sync on start and use `millis`); Use extended RTC for better `PWM`.
+ Use `snprintf_P` in place of `snprintf`.
+ Use some macro for logging instead of multiple `#if`?.
+ Add recessiveness to `prepareWebArduino`.
+ Add `--watch` to `prepareWebArduino`.
+ Try overclock ESP into 160 MHz.
+ Proper seeding code, incl. WiFi.
+ Working pH meter.
