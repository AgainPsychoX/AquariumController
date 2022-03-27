#pragma once

#define DEBUG LEVEL_INFO
constexpr auto debugLevel = DEBUG;

USE_LOG_LEVEL(Lighting,         LEVEL_INFO);
USE_LOG_LEVEL(MineralsPumps,    LEVEL_INFO);
USE_LOG_LEVEL(CloudLogger,      LEVEL_INFO);
USE_LOG_LEVEL(Circulator,       LEVEL_INFO);
USE_LOG_LEVEL(Heating,          LEVEL_INFO);

// WiFi settings
const char ssid[] = "TP-LINK_Jacek";
const char password[] = "kamildanielpatryk";

#define CLOUDLOGGER_INSECURE 0
