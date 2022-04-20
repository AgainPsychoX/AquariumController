#pragma once

#include "common.hpp"
#include <user_interface.h>

#define ip4_addr_printf_unpack(ip) ip4_addr_get_byte(ip, 0), ip4_addr_get_byte(ip, 1), ip4_addr_get_byte(ip, 2), ip4_addr_get_byte(ip, 3)

namespace Network {
	bool validateIP(const ip4_addr_t& ip);

	bool setIPAddresses(ip_info& info);
	void getIPInfo(ip_info& info);

	std::unique_ptr<softap_config> loadSoftAPConfig();
	void saveSoftAPConfig(softap_config& conf);

	std::unique_ptr<station_config> loadStationConfig();
	void saveStationConfig(station_config& conf);

	void resetConfig();

	std::unique_ptr<char[]> getConfigJSON();

	void handleConfigArgs();
	
	void setup();

	void debugPrint();
}
