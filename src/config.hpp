#pragma once

#define DEBUG LEVEL_DEBUG
constexpr auto debugLevel = DEBUG;

USE_LOG_LEVEL_DEFAULT(DEBUG);
USE_LOG_LEVEL(Lighting,         LEVEL_INFO);
USE_LOG_LEVEL(MineralsPumps,    LEVEL_INFO);
USE_LOG_LEVEL(CloudLogger,      LEVEL_INFO);
USE_LOG_LEVEL(Circulator,       LEVEL_INFO);
USE_LOG_LEVEL(Heating,          LEVEL_INFO);
USE_LOG_LEVEL(phMeter,          LEVEL_INFO);

// WiFi settings
const char ssid[] = "TP-LINK_Jacek";
const char password[] = "kamildanielpatryk";

#define CLOUDLOGGER_SECURE 0
