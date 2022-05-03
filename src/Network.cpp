#include "Network.hpp"

#include <lwip/dns.h>
#include <user_interface.h>
#include <LwipDhcpServer.h>
#include <LiquidCrystal_I2C.h> 

extern LiquidCrystal_I2C lcd; // from main

#ifndef DEFAULT_SSID
#define DEFAULT_SSID "AquariumController"
#endif
#ifndef DEFAULT_PASSWORD
#define DEFAULT_PASSWORD ""
#endif

namespace Network {
	bool validateIP(const ip4_addr_t& ip) {
		return ip.addr != IPADDR_NONE && ip.addr != IPADDR_ANY;
	}
	
	bool setIPAddresses(ip_info& info) {
		if (!validateIP(info.ip) || !validateIP(info.gw) || !validateIP(info.netmask)) {
			return false;
		}
		// Note: Random IP could be taken if invalid, based on gateway and netmask.

		if (!wifi_set_ip_info(STATION_IF, &info)) {
			return false;
		}

		return true;
	}

	void getIPInfo(ip_info& info) {
		switch (WiFi.getMode()) {
			case WIFI_STA:
				wifi_get_ip_info(STATION_IF, &info);
				break;
			case WIFI_AP:
				wifi_get_ip_info(SOFTAP_IF, &info);
				break;
			default:
				LOG_TRACE(Network, "Invalid WiFi mode! WiFi.getMode() == %u", static_cast<uint8_t>(WiFi.getMode()));
				break;
		}
	}

	int8_t getRSSI() {
		return wifi_station_get_rssi();
	}

	std::unique_ptr<softap_config> loadSoftAPConfig() {
		auto conf = std::make_unique<softap_config>();
		wifi_softap_get_config_default(conf.get());
		return conf;
	}
	void saveSoftAPConfig(softap_config& conf) {
		WiFi.mode(WIFI_AP);
		ETS_UART_INTR_DISABLE();
		wifi_softap_set_config(&conf);
		ETS_UART_INTR_ENABLE();
	}

	std::unique_ptr<station_config> loadStationConfig() {
		auto conf = std::make_unique<station_config>();
		wifi_station_get_config_default(conf.get());
		return conf;
	}
	void saveStationConfig(station_config& conf) {
		WiFi.mode(WIFI_STA);
		ETS_UART_INTR_DISABLE();
		wifi_station_set_config(&conf);
		ETS_UART_INTR_ENABLE();
	}

	void resetConfig() {
		LOG_DEBUG(Network, "Resetting config to defaults");
		{
			struct station_config conf = {
				.bssid_set = 0,
				.threshold = {
					.rssi = -127,
					.authmode = sizeof(DEFAULT_PASSWORD) > 1 ? AUTH_WPA_WPA2_PSK : AUTH_OPEN,
				},
			};
			strncpy_P(reinterpret_cast<char*>(conf.ssid), PSTR(DEFAULT_SSID), sizeof(conf.ssid));
			strncpy_P(reinterpret_cast<char*>(conf.password), PSTR(DEFAULT_PASSWORD), sizeof(conf.password));
			saveStationConfig(conf);
		}
		{
			struct softap_config conf = {
				.ssid_len = sizeof(DEFAULT_SSID),
				.channel = 1,
				.authmode = sizeof(DEFAULT_PASSWORD) > 1 ? AUTH_WPA_WPA2_PSK : AUTH_OPEN,
				.ssid_hidden = 0,
				.max_connection = 4,
				.beacon_interval = 100,
			};
			strncpy_P(reinterpret_cast<char*>(conf.ssid), PSTR(DEFAULT_SSID), sizeof(conf.ssid));
			strncpy_P(reinterpret_cast<char*>(conf.password), PSTR(DEFAULT_PASSWORD), sizeof(conf.password));
			saveSoftAPConfig(conf);
		}
		debugPrint();
	}

	/// Returns network config parsed into JSON.
	std::unique_ptr<char[]> getConfigJSON() {
		ip_info info;
		union {
			station_config sta_conf;
			softap_config ap_conf;
			struct {
				char ssid[32];
				char password[64];
			};
		} u;
		switch (WiFi.getMode()) {
			case WIFI_STA:
				wifi_get_ip_info(STATION_IF, &info);
				wifi_station_get_config(&u.sta_conf);
				break;
			case WIFI_AP:
				wifi_get_ip_info(SOFTAP_IF, &info);
				wifi_softap_get_config(&u.ap_conf);
				break;
			default:
				LOG_TRACE(Network, "Invalid WiFi mode! WiFi.getMode() == %u", static_cast<uint8_t>(WiFi.getMode()));
				break;
		}

		constexpr unsigned int bufferLength = 200;
		char* buffer = new char[bufferLength];
		snprintf_P(
			buffer, bufferLength,
			PSTR("{"
				"\"mode\":%c,"
				"\"ssid\":\"%.32s\","
				"\"psk\":\"%.64s\","
				"\"static\":%c,"
				"\"ip\":\"%u.%u.%u.%u\","
				// "\"mask\":\"%u.%u.%u.%u\","
				"\"mask\":%u,"
				"\"gateway\":\"%u.%u.%u.%u\","
				"\"dns1\":\"%u.%u.%u.%u\","
				"\"dns2\":\"%u.%u.%u.%u\""
			"}"),
			'0' + settings->network.mode,
			u.ssid,
			u.password,
			'0' + settings->network.staticIP,
			ip4_addr_printf_unpack(&info.ip),
			// ip4_addr_printf_unpack(&info.netmask),
			numberOfSetBits(info.netmask.addr),
			ip4_addr_printf_unpack(&info.gw),
			ip4_addr_printf_unpack(&settings->network.dns1),
			ip4_addr_printf_unpack(&settings->network.dns2)
		);
		return std::unique_ptr<char[]>(buffer);
	}

	void handleConfigArgs() {
		bool changes = false;

		if (const String& str = webServer.arg("network.reset"); !str.isEmpty()) {
			resetConfig();
			changes = true;
		}

		if (const String& str = webServer.arg("network.mode"); !str.isEmpty()) {
			settings->network.mode = static_cast<Settings::Network::Mode>(atoi(str.c_str()) % 4);
			changes = true;
		}

		union {
			station_config conf;
			struct {
				char ssid[32];
				char password[64];
			};
		} sta_u;
		union {
			softap_config conf;
			struct {
				char ssid[32];
				char password[64];
			};
		} ap_u;
		wifi_station_get_config(&sta_u.conf);
		wifi_softap_get_config(&ap_u.conf);

		if (const String& str = webServer.arg("network.ssid"); !str.isEmpty()) {
			strncpy(sta_u.ssid, str.c_str(), sizeof(sta_u.ssid));
			const String& psk = webServer.arg("network.psk");
			strncpy(sta_u.password, psk.c_str(), sizeof(sta_u.password));
			changes = true;
		}

		if (const String& str = webServer.arg("network.ap.ssid"); !str.isEmpty()) {
			strncpy(ap_u.ssid, str.c_str(), sizeof(ap_u.ssid));
			ap_u.conf.ssid_len = std::max<uint8>(str.length(), sizeof(ap_u.ssid));
			const String& psk = webServer.arg("network.ap.psk");
			if (psk.length() > 0) {
				ap_u.conf.authmode = AUTH_WPA_WPA2_PSK;
				strncpy(ap_u.password, psk.c_str(), sizeof(ap_u.password));
			}
			else {
				ap_u.conf.authmode = AUTH_OPEN;
				memset(ap_u.password, 0, sizeof(ap_u.password));
			}
			changes = true;
		}

		if (const String& str = webServer.arg("network.ap.channel"); !str.isEmpty()) {
			uint8_t channel = atoi(str.c_str()) % 14;
			if (channel != 0) {
				ap_u.conf.channel = channel;
			}
			changes = true;
		}

		if (const String& str = webServer.arg("network.static"); !str.isEmpty()) {
			settings->network.staticIP = parseBoolean(str.c_str());
			changes = true;
		}

		if (const String& str = webServer.arg("network.ip"); !str.isEmpty()) {
			IPAddress address; address.fromString(str);
			settings->network.ipInfo.ip.addr = address.v4();
			changes = true;
		}
		if (const String& str = webServer.arg("network.mask"); !str.isEmpty()) {
			if (str.indexOf('.') > -1) {
				IPAddress address; address.fromString(str);
				settings->network.ipInfo.netmask.addr = address.v4();
			}
			else {
				const uint8_t maskLength = atoi(str.c_str());
				uint8_t i = maskLength - 1;
				settings->network.ipInfo.netmask.addr = 1;
				while (i--) {
					settings->network.ipInfo.netmask.addr <<= 1;
					settings->network.ipInfo.netmask.addr |= 1;
				}
				LOG_TRACE(
					Network, "Setting mask as length %u. Resulting addres: %u.%u.%u.%u", 
					maskLength, ip4_addr_printf_unpack(&settings->network.ipInfo.netmask)
				);
			}
			changes = true;
		}
		if (const String& str = webServer.arg("network.gateway"); !str.isEmpty()) {
			IPAddress address; address.fromString(str);
			settings->network.ipInfo.gw.addr = address.v4();
			changes = true;
		}
		if (const String& str = webServer.arg("network.dns1"); !str.isEmpty()) {
			IPAddress address; address.fromString(str);
			settings->network.dns1 = address;
			changes = true;
		}
		if (const String& str = webServer.arg("network.dns2"); !str.isEmpty()) {
			IPAddress address; address.fromString(str);
			settings->network.dns2 = address;
			changes = true;
		}

		if (changes) {
			constexpr unsigned int bufferLength = 80;
			char buffer[bufferLength];
			int ret = snprintf_P(
				buffer, bufferLength,
				PSTR("{"
					"\"newIP\":\"%u.%u.%u.%u\""
				"}"),
				ip4_addr_printf_unpack(&settings->network.ipInfo.ip)
			);
			if (ret < 0 || static_cast<unsigned int>(ret) >= bufferLength) {
				webServer.send(500, WEB_CONTENT_TYPE_TEXT_HTML, F("Response buffer exceeded"));
			}
			else {
				webServer.send(200, WEB_CONTENT_TYPE_APPLICATION_JSON, buffer);
			}
			webServer.handleClient();
			delay(50);

			saveStationConfig(sta_u.conf);
			saveSoftAPConfig(ap_u.conf);
			settings->prepareForSave();
			EEPROM.commit();
			delay(50);
			ESP.restart();
		}
	}

	void showSSIDOnLCD(const char* ssid) {
		// Connecting info on LCD
		lcd.setCursor(0, 0);
		if (strlen(ssid) <= 14) {
			lcd.print(F("SSID: "));
		}
		lcd.print(ssid);
	}

	void setup() {
		bool fallback = false;
		WiFi.persistent(false);

		// Try to connect to preconfigured WiFi network
		if (settings->network.usesStation()) {
			auto conf = loadStationConfig();
			LOG_DEBUG(Network, "Connecting to SSID: '%.32s', PASSWORD: '%.64s'", conf->ssid, conf->password);
			
			lcd.clear();
			showSSIDOnLCD(reinterpret_cast<const char*>(conf->ssid));

			WiFi.mode(WIFI_STA);

			wifi_station_dhcpc_stop();
			wifi_softap_dhcps_stop();

			bool hasAddress = false;
			if (settings->network.staticIP) {
				LOG_TRACE(Network, "Trying to use static IP config:");
				LOG_TRACE(Network, "IPv4: %u.%u.%u.%u", ip4_addr_printf_unpack(&settings->network.ipInfo.ip));
				LOG_TRACE(Network, "Mask: %u.%u.%u.%u", ip4_addr_printf_unpack(&settings->network.ipInfo.netmask));
				LOG_TRACE(Network, "Gate: %u.%u.%u.%u", ip4_addr_printf_unpack(&settings->network.ipInfo.gw));

				hasAddress = setIPAddresses(settings->network.ipInfo);
				if (!hasAddress) {
					LOG_WARN(Network, "Invalid IP settings, falling back to DHCP client.");
				}
			}
			if (!hasAddress) {
				wifi_station_dhcpc_start();
			}

			if (validateIP(settings->network.dns1)) {
				dns_setserver(0, &settings->network.dns1);
			}
			if (validateIP(settings->network.dns2)) {
				dns_setserver(1, &settings->network.dns2);
			}

			ETS_UART_INTR_DISABLE();
			wifi_station_set_config_current(conf.get());
			wifi_station_connect();
			ETS_UART_INTR_ENABLE();

			const unsigned long startTime = millis();
			while (millis() - startTime < timeoutForConnectingWiFi) {
				// buildInLed.on();
				lcd.setCursor(0, 1);
				lcd.print(F("IP: - * - * - * - "));
				delay(333);

				// buildInLed.off();
				lcd.setCursor(0, 1);
				lcd.print(F("IP: * - * - * - * "));
				delay(333);

				if (WiFi.status() == WL_CONNECTED) {
					LOG_INFO(Network, "Connected via WiFi to %.32s!", conf->ssid);
					break;
				}
			}

			if (WiFi.status() == WL_CONNECTED) {
				LOG_INFO(Network, "IP: %s", WiFi.localIP().toString().c_str());
			}
			else {
				LOG_WARN(Network, "Failed to connect to network SSID: '%.32s'", conf->ssid);

				if (settings->network.mode == Settings::Network::Mode::FALLBACK) {
					fallback = true;
					LOG_WARN(Network, "Falling back to hosting AP.", conf->ssid);
				}
			}
		}

		// Try to host AP (if always or fallback)
		if (settings->network.mode == Settings::Network::Mode::AP || fallback) {
			auto conf = loadSoftAPConfig();
			LOG_DEBUG(Network, "Hosting AP with SSID: '%.32s', PASSWORD: '%.64s'", conf->ssid, conf->password);

			lcd.clear();
			showSSIDOnLCD(reinterpret_cast<const char*>(conf->ssid));

			WiFi.mode(WIFI_AP);

			wifi_station_dhcpc_stop();
			wifi_softap_dhcps_stop();
			dhcpSoftAP.end();

			struct ip_info info = {
				.ip      = { 0x0104A8C0 },
				.netmask = { 0x00FFFFFF },
				.gw      = { 0x0104A8C0 },
			};
			setIPAddresses(info);

			struct dhcps_lease dhcps_lease = {
				.enable = true,
				.start_ip = { 0x6404A8C0 },
				.end_ip   = { 0xC804A8C0 },
			};
			dhcpSoftAP.set_dhcps_lease(&dhcps_lease);
			dhcpSoftAP.set_dhcps_lease_time(720);

			uint8_t mode = info.gw.addr ? 1 : 0;
			dhcpSoftAP.set_dhcps_offer_option(OFFER_ROUTER, &mode);

			ETS_UART_INTR_DISABLE();
			wifi_softap_set_config_current(conf.get());
			ETS_UART_INTR_ENABLE();

			wifi_softap_dhcps_start();
			dhcpSoftAP.begin(&info);

			// Print IP to logs
			{
				ip_info info;
				Network::getIPInfo(info);
				LOG_INFO(Network, "IP: %u.%u.%u.%u", ip4_addr_printf_unpack(&info.ip));
			}

			lcd.setCursor(0, 1);
			lcd.print(F("Hosting AP..."));
			delay(2000);
		}

		debugPrint();
	}

	void debugPrint() {
		if (CHECK_LOG_LEVEL(Network, LEVEL_TRACE)) {
			{
				struct ip_info info;
				wifi_get_ip_info(STATION_IF, &info);
				LOG_TRACE(Network, "STATION IPv4: %u.%u.%u.%u", ip4_addr_printf_unpack(&info.ip));
				LOG_TRACE(Network, "STATION Mask: %u.%u.%u.%u", ip4_addr_printf_unpack(&info.netmask));
				LOG_TRACE(Network, "STATION Gate: %u.%u.%u.%u", ip4_addr_printf_unpack(&info.gw));

				struct station_config conf;
				wifi_station_get_config(&conf);
				LOG_TRACE(Network, "STATION CONFIG SSID: '%.33s'", conf.ssid);
				LOG_TRACE(Network, "STATION CONFIG PASS: '%.65s'", conf.password);

				wifi_station_get_config_default(&conf);
				LOG_TRACE(Network, "STATION CONFIG DEFAULT SSID: '%s'", conf.ssid);
				LOG_TRACE(Network, "STATION CONFIG DEFAULT PASS: '%s'", conf.password);
			}
			{
				struct ip_info info;
				wifi_get_ip_info(SOFTAP_IF, &info);
				LOG_TRACE(Network, "SOFTAP IPv4: %u.%u.%u.%u", ip4_addr_printf_unpack(&info.ip));
				LOG_TRACE(Network, "SOFTAP Mask: %u.%u.%u.%u", ip4_addr_printf_unpack(&info.netmask));
				LOG_TRACE(Network, "SOFTAP Gate: %u.%u.%u.%u", ip4_addr_printf_unpack(&info.gw));

				struct softap_config conf;
				wifi_softap_get_config(&conf);
				LOG_TRACE(Network, "SOFTAP CONFIG SSID: '%.33s'", conf.ssid);
				LOG_TRACE(Network, "SOFTAP CONFIG PASS: '%.65s'", conf.password);
				
				wifi_softap_get_config_default(&conf);
				LOG_TRACE(Network, "SOFTAP CONFIG DEFAULT SSID: '%s'", conf.ssid);
				LOG_TRACE(Network, "SOFTAP CONFIG DEFAULT PASS: '%s'", conf.password);
			}
		}
	}
}