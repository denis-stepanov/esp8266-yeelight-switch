// DS System customization file
// (c) DNS 2021

#ifndef _DS_SYSTEM_H_

#define DS_CAP_APP_ID            // Enable application identification
#define DS_CAP_APP_LOG           // Enable application log
#define DS_CAP_SYS_LED           // Enable builtin LED
#define DS_CAP_SYS_LOG_HW        // Enable syslog on hardware serial line
#define DS_CAP_SYS_TIME          // Enable system time
#define DS_CAP_SYS_FS            // Enable file system
#define DS_CAP_SYS_NETWORK       // Enable networking
#define DS_CAP_WIFIMANAGER       // Enable Wi-Fi configuration at runtime
#define DS_CAP_MDNS              // Enable mDNS
#define DS_CAP_WEBSERVER         // Enable web server
#define DS_CAP_BUTTON            // Enable button
#define DS_CAP_TIMERS_ABS        // Enable timers from absolute time
#define DS_CAP_TIMERS_SOLAR      // Enable timers from solar events
#define DS_CAP_TIMERS_COUNT_ABS  // Enable countdown timers via absolute time
#define DS_CAP_WEB_TIMERS        // Enable timer configuration via web

#define DS_TIMEZONE TZ_Europe_Paris       // Timezone. Pick yours from https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
#define DS_LATITUDE 51.483611             // Your latitude  (needed for solar-based timers)
#define DS_LONGITUDE -0.005833            // Your longitude (needed for solar-based timers)

// Different button wiring on various boards
//// For Witty Cloud, use board "LOLIN(WEMOS) D1 R2 & mini"
#ifdef ARDUINO_ESP8266_WEMOS_D1MINI
#define BUTTON_BUILTIN D2
#endif // ARDUINO_ESP8266_WEMOS_D1MINI
//// For NodeMCU, use board "NodeMCU 1.0 (ESP-12E Module)". Defaults are fine
//// For ESP-01(S), use board "Generic ESP8266 Module"; Flash Size "1MB (FS:256KB)"; Builtin LED: 1 for ESP-01, 2 for ESP-01S. Connect a push button between GPIO0 and GND

// #define BUTTON_BUILTIN 0               // Define if your button is connected to a GPIO other than 0

#include "src/System.h"         // DS-System global definitions

#endif // _DS_SYSTEM_H_
