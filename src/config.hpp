#pragma once

////////////////////////////////////////////////////////////////////////////////
// Main configuration

#define DEBUG LEVEL_DEBUG
constexpr auto debugLevel = DEBUG;

USE_LOG_LEVEL_DEFAULT(DEBUG);
USE_LOG_LEVEL(Network,          LEVEL_INFO);
USE_LOG_LEVEL(EEPROM,           LEVEL_INFO);
USE_LOG_LEVEL(Web,              LEVEL_INFO);
USE_LOG_LEVEL(Lighting,         LEVEL_INFO);
USE_LOG_LEVEL(MineralsPumps,    LEVEL_INFO);
USE_LOG_LEVEL(CloudLogger,      LEVEL_INFO);
USE_LOG_LEVEL(Circulator,       LEVEL_INFO);
USE_LOG_LEVEL(Heating,          LEVEL_INFO);
USE_LOG_LEVEL(phMeter,          LEVEL_INFO);

#define CLOUDLOGGER_SECURE 0

/// Timeouts for networking
constexpr unsigned long timeoutForConnectingWiFi = 5000; // ms
constexpr uint16_t timeoutForCloudLogger = 2000; // ms

#define WIFI_ROUND_RSSI_CHARS 0

////////////////////////////////////////////////////////////////////////////////
// Settings structure (persistet in EEPROM)

/// Day cycle entry at given time point.
struct DayCycleEntryData {
	uint8_t hour;
	uint8_t minute;

	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t white;

	uint8_t _pad[2];
};
static_assert(sizeof(DayCycleEntryData) == 8);

/// Interpolation mode (for pH meter).
enum InterpolationMode : uint8_t {
	Linear,
	// TODO: Other interpolation modes
	// Cosine,
	// Polynomial,
	Count,
};

/// Pump settings
struct PumpSettings {
	/// Calibration value is defined by time (in miliseconds) required to pump one milliliter.
	float calibration;

	uint8_t hour;
	uint8_t minute;

	uint8_t milliliters;
	uint8_t _pad;
};
static_assert(sizeof(PumpSettings) == 8);

// Settings structure (in EEPROM)
struct Settings {
	////////////////////////////////////////
	// 0x000 - 0x020: Checksum

	uint8_t _emptyBeginPad[28];
	uint32_t checksum;

	uint32_t calculateChecksum() {
		constexpr uint16_t prefixLength = offsetof(Settings, checksum) + sizeof(checksum);
		return crc32(reinterpret_cast<uint8_t*>(this) + prefixLength, sizeof(Settings) - prefixLength);
	}

	bool prepareForSave() {
		uint32_t calculatedChecksum = calculateChecksum();
		bool changed = checksum != calculatedChecksum;
		checksum = calculatedChecksum;
		return changed;
	}

	////////////////////////////////////////
	// 0x020 - 0x030: Water temperature.

	struct {
		/// Minimal temperature for water, lower starts heating.
		float minimal = 22.5f;

		/// Optimal temperature for water, heating or fans stop.
		float optimal = 24.0f;

		/// Maximal temperature for water, above starts fans.
		float maximal = 25.5f;

		uint8_t _pad[4];
	} temperatures;

	////////////////////////////////////////
	// 0x030 - 0x040: Water circulation.
	
	struct {
		/// Circulator active time start
		uint8_t hourStart = 8;
		/// Circulator active time start
		uint8_t minuteStart = 30;
		
		/// Circulator active time end
		uint8_t hourEnd = 18;
		/// Circulator active time end
		uint8_t minuteEnd = 30;

		/// Circulator active (in miliseconds)
		uint32_t activeTime = 10000;
		/// Circulator pause (in miliseconds).
		uint32_t pauseTime = 20000;

		uint8_t _pad[4];
	} circulator;

	////////////////////////////////////////
	// 0x040 - 0x060: Water pH meter calibration.

	struct {
		/// Defined pH levels for calibration points.
		float pH[5] = { 9.20f, 6.88f, 4.03f, 0.0f, 0.0f };
		/// Readings from ADC for calibration points.
		uint16_t adc[5] = { 712, 851, 1008, 0, 0 };

		/// Number of calibration points.
		uint8_t pointsCount = 3;

		/// Interpolation mode (method used to calculate value from reading using given points).
		InterpolationMode mode = InterpolationMode::Linear;
	} phMeter;

	////////////////////////////////////////
	// 0x060 - 0x080: Minerals pumps.

	PumpSettings pumps[3] = {
		{ .calibration = 412.000f, .hour = 9, .minute = 30, .milliliters = 10 },
		{ .calibration = 412.310f, .hour = 9, .minute = 40, .milliliters = 10 },
		{ .calibration = 397.016f, .hour = 9, .minute = 50, .milliliters = 20 },
	};
	uint8_t _padPumps[8];

	////////////////////////////////////////
	// 0x080 - 0x100: Day cycle entries (smooth lighting).

	DayCycleEntryData dayCycleEntries[16];

	////////////////////////////////////////
	// 0x100 - 0x160: Some network and cloud settings.

	// Some network settings are persistet by internal SDK.
	struct Network {
		enum Mode : uint8_t {
			DISABLED = 0b00,
			STATION  = 0b01,
			AP       = 0b10,
			FALLBACK = 0b11,
		};

		union {
			struct {
				Mode mode          : 2;
				bool staticIP      : 1;
			};
			uint8_t flags;
		};
		ip_info ipInfo;
		ip4_addr_t dns1;
		ip4_addr_t dns2;

		char _pad[8];
		char basicAuthEncoded[64] = "";

		inline bool usesStation() { return mode & 0b01; }
	} network;
	static_assert(0x04 == offsetof(Network, ipInfo));
	static_assert(0x0C == sizeof(ip_info));
	static_assert(0x10 == offsetof(Network, dns1));
	static_assert(0x14 == offsetof(Network, dns2));
	static_assert(0x20 == offsetof(Network, basicAuthEncoded));
	static_assert(sizeof(network) == 0x60);

	////////////////////////////////////////
	// 0x160 - 0x200: Cloud settings.

	struct {
		char secret[16] = "";
		uint32_t interval = 60000;
		char _pad[12];
		char endpointURL[128];
	} cloud;
	static_assert(sizeof(cloud) == 0x0A0);





	////////////////////////////////////////

	void resetToDefault() {
		new (this) Settings(); // will apply all defaults
		network.mode = Network::Mode::AP;
	}
};
static_assert(0x020 == offsetof(Settings, temperatures));
static_assert(0x030 == offsetof(Settings, circulator));
static_assert(0x040 == offsetof(Settings, phMeter));
static_assert(0x060 == offsetof(Settings, pumps));
static_assert(0x080 == offsetof(Settings, dayCycleEntries));
static_assert(0x100 == offsetof(Settings, network));
static_assert(0x160 == offsetof(Settings, cloud));
static_assert(sizeof(Settings) == 0x200);
static_assert(0x000 + 28 == offsetof(Settings, checksum));
