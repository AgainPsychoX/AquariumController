#pragma once

#define DEBUG LEVEL_INFO
constexpr auto debugLevel = DEBUG;

USE_LOG_LEVEL(SmoothTimedPWM,           LEVEL_INFO);
USE_LOG_LEVEL(MineralsPumpsController,  LEVEL_INFO);
USE_LOG_LEVEL(CloudLogger,              LEVEL_INFO);
USE_LOG_LEVEL(CirculatorController,     LEVEL_INFO);
USE_LOG_LEVEL(HeatingController,        LEVEL_INFO);

// WiFi settings
const char ssid[] = "TP-LINK_Jacek";
const char password[] = "kamildanielpatryk";

#define CLOUDLOGGER_INSECURE 0
