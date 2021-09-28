// DS System customization file
// (c) DNS 2021

#ifndef _DS_SYSTEM_H_

#define DS_CAP_APP_ID            // Enable application identification
// #define DS_CAP_APP_LOG           // Enable application log
// #define DS_CAP_SYS_LED           // Enable builtin LED
// #define DS_CAP_SYS_LOG           // Enable syslog
// #define DS_CAP_SYS_LOG_HW        // Enable syslog on hardware serial line
// #define DS_CAP_SYS_RESET         // Enable software reset interface
// #define DS_CAP_SYS_RTCMEM        // Enable RTC memory
// #define DS_CAP_SYS_TIME          // Enable system time
// #define DS_CAP_SYS_UPTIME        // Enable system uptime counter
// #define DS_CAP_SYS_FS            // Enable file system
// #define DS_CAP_SYS_NETWORK       // Enable networking
// #define DS_CAP_WIFIMANAGER       // Enable Wi-Fi configuration at runtime
// #define DS_CAP_MDNS              // Enable mDNS
// #define DS_CAP_WEBSERVER         // Enable web server
// #define DS_CAP_BUTTON            // Enable button
// #define DS_CAP_TIMERS            // Enable timers
// #define DS_CAP_TIMERS_ABS        // Enable timers from absolute time
// #define DS_CAP_TIMERS_SOLAR      // Enable timers from solar events
// #define DS_CAP_TIMERS_COUNT_ABS  // Enable countdown timers via absolute time
// #define DS_CAP_TIMERS_COUNT_TICK // Enable countdown timers via ticker
// #define DS_CAP_WEB_TIMERS        // Enable timer configuration via web


// #define DS_TIMEZONE TZ_Europe_Paris       // Timezone if system time is used
// #define DS_LATITUDE 51.483611             // Latitude if solar events are used
// #define DS_LONGITUDE -0.005833            // Longitude if solar events are used
// #define DS_FS_TYPE LittleFS               // "LittleFS" or "SPIFFS" (without quotes)
// #define DS_LED_VS_SERIAL_CHECKED_OK       // Define if both LED and Serial are used in your sketch in non-conflicting way
// #define DS_UNSTABLE_SERIAL                // Define to skip small delay after syslog initialization, used to get reliable printout after program upload
// #define BUTTON_BUILTIN 0                  // Define if your button is connected to GPIO other than 0

#include "System.h"         // DS-System global definitions

#endif // _DS_SYSTEM_H_
