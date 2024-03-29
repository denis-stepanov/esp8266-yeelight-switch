// DS System customization file
// (c) DNS 2021

#ifndef _DS_SYSTEM_H_

// === User configuration
//// Geographical coordinates do not have to match your location precisely. One decimal digit after a comma (NN.N) (few km precision) is good enough
#define DS_TIMEZONE  TZ_Europe_Paris      // Your timezone (needed for clock timers). Pick from the list https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
#define DS_LATITUDE  48.8584              // Your latitude (needed for timers triggered on sunrise / sunset)
#define DS_LONGITUDE 2.2945               // Your longitude (needed for timers triggered on sunrise / sunset)
#define DS_HOSTNAME  "ybutton1"           // <hostname>.local in the local network. Also, SSID of the temporary network for Wi-Fi configuration

//// Different button wiring on various boards. Normally OK as it is
////// For Witty Cloud, use board "LOLIN(WEMOS) D1 R2 & mini"
#ifdef ARDUINO_ESP8266_WEMOS_D1MINI
#define BUTTON_BUILTIN D2
#endif // ARDUINO_ESP8266_WEMOS_D1MINI
////// For NodeMCU, use board "NodeMCU 1.0 (ESP-12E Module)". Defaults are fine
////// For ESP-01(S), use board "Generic ESP8266 Module"; Flash Size "1MB (FS:256KB)"; Builtin LED: 1 for ESP-01, 2 for ESP-01S. Connect a push button between GPIO0 and GND

// #define BUTTON_BUILTIN 0               // Uncomment and define if your button is connected to a GPIO other than 0
// === End of user configuration

// Changing below this line will require code adaptations elsewhere in the app
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

#include "src/System.h"          // DS-System global definitions

#endif // _DS_SYSTEM_H_
