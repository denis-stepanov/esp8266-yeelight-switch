/* DS System definition
 * * Use capability macros to enable/disable available system features
 * (c) DNS 2020-2021
 */

#ifndef _DS_SYSTEM_H_
#define _DS_SYSTEM_H_

// System capabilities. Define them in MySystem.h before including this header
// DS_CAP_APP_ID            - enable application identification
// DS_CAP_APP_LOG           - enable application log
// DS_CAP_SYS_LED           - enable builtin LED
// DS_CAP_SYS_LOG           - enable syslog
// DS_CAP_SYS_LOG_HW        - enable syslog on hardware serial line (UART 0 or 1)
// DS_CAP_SYS_RESET         - enable software reset interface
// DS_CAP_SYS_RTCMEM        - enable RTC memory
// DS_CAP_SYS_TIME          - enable system time
// DS_CAP_SYS_UPTIME        - enable system uptime counter
// DS_CAP_SYS_FS            - enable file system
// DS_CAP_SYS_NETWORK       - enable networking
// DS_CAP_WIFIMANAGER       - enable Wi-Fi configuration at runtime
// DS_CAP_MDNS              - enable mDNS
// DS_CAP_WEBSERVER         - enable web server
// DS_CAP_BUTTON            - enable button
// DS_CAP_TIMERS            - enable timers
// DS_CAP_TIMERS_ABS        - enable timers from absolute time
// DS_CAP_TIMERS_SOLAR      - enable timers from solar events
// DS_CAP_TIMERS_COUNT_ABS  - enable countdown timers via absolute time
// DS_CAP_TIMERS_COUNT_TICK - enable countdown timers via ticker
// DS_CAP_WEB_TIMERS        - enable timer configuration via web

// System version
// Format is x.xx.xx (major.minor.maintenance). E.g., 20001 means 2.0.1
#define DS_SYSTEM_VERSION 10103U   // 1.1.3

// Consistency checks. Policy: whenever one capability requires another, issue a warning and enable. Whenever one capability extends another, enable without a warning
#if defined(DS_CAP_SYS_LOG_HW) && !defined(DS_CAP_SYS_LOG)
#define DS_CAP_SYS_LOG
#endif // DS_CAP_SYS_LOG_HW && !DS_CAP_SYS_LOG

#if defined(DS_CAP_APP_LOG) && !defined(DS_CAP_SYS_FS)
#warning "Capability DS_CAP_APP_LOG requires DS_CAP_SYS_FS; enabling"
#define DS_CAP_SYS_FS
#endif // DS_CAP_APP_LOG && !DS_CAP_SYS_FS

#if defined(DS_CAP_WEB_TIMERS) && !defined(DS_CAP_WEBSERVER)
#warning "Capability DS_CAP_WEB_TIMERS requires DS_CAP_WEBSERVER; enabling"
#define DS_CAP_WEBSERVER
#endif // DS_CAP_WEB_TIMERS && !DS_CAP_WEBSERVER

#if defined(DS_CAP_MDNS) && !defined(DS_CAP_SYS_NETWORK)
#warning "Capability DS_CAP_MDNS requires DS_CAP_SYS_NETWORK; enabling"
#define DS_CAP_SYS_NETWORK
#endif // DS_CAP_MDNS && !DS_CAP_SYS_NETWORK

#if defined(DS_CAP_WEBSERVER) && !defined(DS_CAP_SYS_NETWORK)
#warning "Capability DS_CAP_WEBSERVER requires DS_CAP_SYS_NETWORK; enabling"
#define DS_CAP_SYS_NETWORK
#endif // DS_CAP_WEBSERVER && !DS_CAP_SYS_NETWORK

#if defined(DS_CAP_WEB_TIMERS) && !defined(DS_CAP_TIMERS_ABS)
#warning "Capability DS_CAP_WEB_TIMERS requires DS_CAP_TIMERS_ABS; enabling"
#define DS_CAP_TIMERS_ABS
#endif // DS_CAP_WEB_TIMERS && !DS_CAP_TIMERS_ABS

#if defined(DS_CAP_WEB_TIMERS) && !defined(DS_CAP_TIMERS_SOLAR)
#warning "Capability DS_CAP_WEB_TIMERS requires DS_CAP_TIMERS_SOLAR; enabling"
#define DS_CAP_TIMERS_SOLAR
#endif // DS_CAP_WEB_TIMERS && !DS_CAP_TIMERS_SOLAR

#if defined(DS_CAP_WEB_TIMERS) && !defined(DS_CAP_TIMERS_COUNT_ABS)
#warning "Capability DS_CAP_WEB_TIMERS requires DS_CAP_TIMERS_COUNT_ABS; enabling"
#define DS_CAP_TIMERS_COUNT_ABS
#endif // DS_CAP_WEB_TIMERS && !DS_CAP_TIMERS_COUNT_ABS

#if (defined(DS_CAP_TIMERS_SOLAR) || defined(DS_CAP_TIMERS_COUNT_ABS)) && !defined(DS_CAP_TIMERS_ABS)
#define DS_CAP_TIMERS_ABS
#endif // (DS_CAP_TIMERS_SOLAR || DS_CAP_TIMERS_COUNT_ABS) && !DS_CAP_TIMERS_ABS

#if (defined(DS_CAP_TIMERS_ABS) || defined(DS_CAP_TIMERS_COUNT_TICK)) && !defined(DS_CAP_TIMERS)
#define DS_CAP_TIMERS
#endif // (DS_CAP_TIMERS_ABS || DS_CAP_TIMERS_COUNT_TICK) && !DS_CAP_TIMERS

#if defined(DS_CAP_TIMERS_ABS) && !defined(DS_CAP_SYS_TIME)
#warning "Selected timer functionality requires time; enabling"
#define DS_CAP_SYS_TIME
#endif // DS_CAP_TIMERS && !DS_CAP_SYS_TIME


// External libraries
#include <Arduino.h>                // String

#ifdef DS_CAP_SYS_LED
#include <jled.h>                   // LED, https://github.com/jandelgado/jled
#endif // DS_CAP_SYS_LED

#ifdef DS_CAP_SYS_LOG
// Log message prefixed with time (to be used with log.printf())
#define TIMED(MSG) "%010lu: " MSG, millis()
#endif // DS_CAP_SYS_LOG

#ifdef DS_CAP_SYS_RESET
#include <user_interface.h>         // ESP interface
#endif // DS_CAP_SYS_RESET

#ifdef DS_CAP_SYS_FS
#include <FS.h>                     // File system
#endif // DS_CAP_SYS_FS

#ifdef DS_CAP_WEBSERVER
#include <ESP8266WebServer.h>       // Web server
#endif // DS_CAP_WEBSERVER

#ifdef DS_CAP_BUTTON
#include <AceButton.h>              // Button, https://github.com/bxparks/AceButton
#endif // DS_CAP_BUTTON

#if defined(DS_CAP_TIMERS_ABS) || defined(DS_CAP_WEB_TIMERS)
#include <forward_list>             // Timer or action list
#endif // DS_CAP_TIMERS_ABS || DS_CAP_WEB_TIMERS

#ifdef DS_CAP_TIMERS_COUNT_TICK
#include <Ticker.h>                 // Periodic events
#endif // DS_CAP_TIMERS_COUNT_TICK

namespace ds {

#ifdef DS_CAP_SYS_TIME
  typedef enum {
    TIME_SYNC_NONE,                                   // Time was never synchronized
    TIME_SYNC_OK,                                     // Time is synchronized
    TIME_SYNC_DEGRADED                                // Time was synchronized but not anymore
  } time_sync_t;

#define TIME_CHANGE_SECOND                        1
#define TIME_CHANGE_MINUTE (TIME_CHANGE_SECOND << 1)
#define TIME_CHANGE_HOUR   (TIME_CHANGE_MINUTE << 1)
#define TIME_CHANGE_DAY    (TIME_CHANGE_HOUR   << 1)
#define TIME_CHANGE_WEEK   (TIME_CHANGE_DAY    << 1)
#define TIME_CHANGE_MONTH  (TIME_CHANGE_WEEK   << 1)
#define TIME_CHANGE_YEAR   (TIME_CHANGE_MONTH  << 1)
#define TIME_CHANGE_NONE   (TIME_CHANGE_SECOND >> 1)
#endif // DS_CAP_SYS_TIME

#ifdef DS_CAP_TIMERS
  typedef enum {
    TIMER_ABSOLUTE,                                   // Timer fires at a given absolute time
    TIMER_SUNRISE,                                    // Timer fires at sunrise
    TIMER_SUNSET,                                     // Timer fires at sunset
    TIMER_COUNTDOWN_ABS,                              // Timer fires at some moment from now, counted via absolute time
    TIMER_COUNTDOWN_TICK,                             // Timer fires at some moment from now, counted via ticker
    TIMER_INVALID                                     // Unsupported timer type (must be the last)
  } timer_type_t;

  class Timer {                                       // Generic timer (abstract)

    protected:
      int id;                                         // Timer identifier (optional)
      timer_type_t type;                              // Timer type
      String action;                                  // Timer action (short description of what it is supposed to do)
      bool armed;                                     // True if timer is armed (will fire); false if ignored with no action
      bool recurrent;                                 // True if timer should be auto-rearmed after firing; false otherwise
      bool transient;                                 // True if timer should be disposed of after firing

      void setType(const timer_type_t /* type */);    // Set timer type

    public:
      Timer(const timer_type_t /* type */, const String action = "undefined",
        const bool armed = true, const bool recurrent = true, const bool transient = false, const int id = -1);  // Constructor
      virtual ~Timer() = 0;                           // Disallow creation of objects of this type
      virtual int getID() const;                      // Return timer identifier
      virtual void setID(const int /* new_id */);     // Set timer identifier
      virtual timer_type_t getType() const;           // Get timer type
      virtual const String& getAction() const;        // Return timer action
      virtual void setAction(const String& /* new_action */); // Set timer action
      virtual bool isArmed() const;                   // Return true if timer is armed
      virtual void arm();                             // Arm the timer (default)
      virtual void disarm();                          // Disarm the timer
      virtual bool isRecurrent() const;               // Return true if timer is recurrent
      virtual void repeatForever();                   // Make timer repetitive (default)
      virtual void repeatOnce();                      // Make timer a one-time shot
      virtual bool isTransient() const;               // Return true if timer is transient (i.e., will be dead after firing)
      virtual void keep();                            // Keep the timer around (default)
      virtual void forget();                          // Mark the timer for disposal
      bool operator==(const Timer& /* timer */) const; // Comparison operator
      bool operator!=(const Timer& /* timer */) const; // Comparison operator
  };
#endif // DS_CAP_TIMERS

#ifdef DS_CAP_TIMERS_ABS

#define TIMER_DOW_NONE                              0
#define TIMER_DOW_SUNDAY                            1
#define TIMER_DOW_MONDAY    (TIMER_DOW_SUNDAY    << 1)
#define TIMER_DOW_TUESDAY   (TIMER_DOW_MONDAY    << 1)
#define TIMER_DOW_WEDNESDAY (TIMER_DOW_TUESDAY   << 1)
#define TIMER_DOW_THURSDAY  (TIMER_DOW_WEDNESDAY << 1)
#define TIMER_DOW_FRIDAY    (TIMER_DOW_THURSDAY  << 1)
#define TIMER_DOW_SATURDAY  (TIMER_DOW_FRIDAY    << 1)
#define TIMER_DOW_INVALID   (TIMER_DOW_SATURDAY  << 1)
#define TIMER_DOW_ANY       (TIMER_DOW_SUNDAY | TIMER_DOW_MONDAY   | TIMER_DOW_TUESDAY | TIMER_DOW_WEDNESDAY \
                                              | TIMER_DOW_THURSDAY | TIMER_DOW_FRIDAY  | TIMER_DOW_SATURDAY)

  class TimerAbsolute : public virtual Timer {        // Absolute time timer

    protected:
      struct tm time;                                 // Timer firing details

    public:
      TimerAbsolute(const String action = "undefined", const uint8_t hour = 0, const uint8_t minute = 0, const uint8_t second = 0,
        const uint8_t dow = TIMER_DOW_ANY, const bool armed = true, const bool recurrent = true, const bool transient = false,
        const int id = -1);  // Constructor
      virtual ~TimerAbsolute() {}                     // Destructor
      virtual uint8_t getHour() const;                // Return hour setting
      virtual void setHour(const uint8_t /* new_hour */); // Set hour setting
      virtual uint8_t getMinute() const;              // Return minute setting
      virtual void setMinute(const uint8_t /* new_minute */); // Set minute setting
      virtual uint8_t getSecond() const;              // Return second setting
      virtual void setSecond(const uint8_t /* new_second */); // Set second setting
      virtual uint8_t getDayOfWeek() const;           // Get day of week setting
      virtual void setDayOfWeek(const uint8_t /* new_dow */); // Set day of week setting
      virtual void enableDayOfWeek(const uint8_t /* new_dow */); // Enable some day(s) of week
      virtual void disableDayOfWeek(const uint8_t /* new_dow */); // Disable some day(s) of week
      bool operator==(const TimerAbsolute& /* timer */) const; // Comparison operator
      bool operator!=(const TimerAbsolute& /* timer */) const; // Comparison operator
      bool operator==(const struct tm& /* _tm */) const; // Time comparison operator
      bool operator!=(const struct tm& /* _tm */) const; // Time comparison operator
  };
#endif // DS_CAP_TIMERS_ABS

#ifdef DS_CAP_TIMERS_SOLAR
  class TimerSolar : public TimerAbsolute {   // Solar event-based timer

    public:
      TimerSolar(const String action = "undefined", const timer_type_t type = TIMER_SUNRISE, const int8_t offset = 0, const uint8_t dow = TIMER_DOW_ANY,
        const bool armed = true, const bool recurrent = true, const bool transient = false, const int id = -1);  // Constructor
      virtual int8_t getOffset() const;               // Return offset in minutes from event
      virtual void setOffset(const int8_t /* offset */); // Set offset in minutes from event
      virtual void adjust();                          // Recalculate alignment to solar times
      bool operator==(const TimerSolar& /* timer */) const; // Comparison operator
      bool operator!=(const TimerSolar& /* timer */) const; // Comparison operator
  };
#endif // DS_CAP_TIMERS_SOLAR

#if defined(DS_CAP_TIMERS_COUNT_ABS) || defined(DS_CAP_TIMERS_COUNT_TICK)
  class TimerCountdown : public virtual Timer {       // Countdown timer

    protected:
      float interval;                                 // Countdown duration (s)

    public:
      TimerCountdown(const timer_type_t /* type */, const String action = "undefined", const float interval = 1,
        const bool armed = true, const bool recurrent = true, const bool transient = false, const int id = -1);  // Constructor
      virtual ~TimerCountdown() = 0;                  // Disallow creation of objects of this type
      virtual float getInterval() const;              // Return timer interval (s)
      virtual void setInterval(const float /* interval */); // Set timer interval (s)
      bool operator==(const TimerCountdown& /* timer */) const; // Comparison operator
      bool operator!=(const TimerCountdown& /* timer */) const; // Comparison operator
  };
#endif // DS_CAP_TIMERS_COUNT_ABS || DS_CAP_TIMERS_COUNT_TICK

#ifdef DS_CAP_TIMERS_COUNT_ABS
  class TimerCountdownAbs : public TimerCountdown, public TimerAbsolute {

    protected:
      time_t getNextTime() const;                     // Return next firing time
      void setNextTime(const time_t /* new_time */);  // Set next firing time

    public:
      TimerCountdownAbs(const String action = "undefined", const float interval = 1, const uint32_t offset = 0, const uint8_t dow = TIMER_DOW_ANY,
        const bool armed = true, const bool recurrent = true, const bool transient = false, const int id = -1);  // Constructor
      ~TimerCountdownAbs() {}                         // Destructor
      virtual float getInterval() const;              // Return timer interval (s)
      virtual void setInterval(const float /* interval */); // Set timer interval (s)
      virtual uint32_t getOffset() const;             // Return timer offset in seconds from midnight
      virtual void setOffset(const uint32_t /* offset */); // Set timer offset in seconds from midnight
      virtual void update(const time_t from_time = 0); // Prepare timer for firing. 0 means from current time
      bool operator==(const TimerCountdownAbs& /* timer */) const; // Comparison operator
      bool operator!=(const TimerCountdownAbs& /* timer */) const; // Comparison operator
  };
#endif // DS_CAP_TIMERS_COUNT_ABS

#ifdef DS_CAP_TIMERS_COUNT_TICK
  class TimerCountdownTick : public TimerCountdown {

    protected:
      Ticker ticker;                                  // Ticker instance
      Ticker::callback_function_t callback;           // Ticker callback

    public:
      TimerCountdownTick(const String action = "undefined", const float interval = 1, Ticker::callback_function_t callback = nullptr,
        const bool armed = true, const bool recurrent = true, const bool transient = false, const int id = -1);  // Constructor
      ~TimerCountdownTick() {}                        // Destructor
      virtual void arm();                             // Arm the timer (default)
      virtual void disarm();                          // Disarm the timer
      virtual void repeatForever();                   // Make timer repetitive (default)
      virtual void repeatOnce();                      // Make timer a one-time shot
      bool operator==(const TimerCountdownTick& /* timer */) const; // Comparison operator
      bool operator!=(const TimerCountdownTick& /* timer */) const; // Comparison operator
  };
#endif // DS_CAP_TIMERS_COUNT_TICK

  // System class is just a collection of system-wide routines, so all of them are made static on purpose
  class System {

    protected:

      static void addCapability(String& /* capabilities */, PGM_P /* capability */); // Append capability to the list

    public:

      // Shared methods that are always defined
      static void begin();                            // Initialize system
      static void update();                           // Update system
      static String getCapabilities();                // Return list of configured capabilities
      static uint32_t getVersion();                   // Get system version

      // Methods and members specific to capabilities
#ifdef DS_CAP_APP_ID
      static const char *app_name;                    // Application name
      static const char *app_version;                 // Application version
      static const char *app_build;                   // Application build identifier
      static const char *app_url;                     // Application URL
#endif // DS_CAP_APP_ID

#ifdef DS_CAP_APP_LOG
    protected:
      static size_t app_log_size;                     // Application log current size

    public:
      static File app_log;                            // Application log current file
      static size_t app_log_size_max;                 // Maximum size of application log. Setting this to 0 disables log at runtime

      static bool appLogWriteLn(const String& /* line */, bool copy_to_syslog = false); // Write a line into application log, optionally copying to syslog
#endif // DS_CAP_APP_LOG

#ifdef DS_CAP_SYS_LED
      static JLed led;                                // Builtin LED
#endif // DS_CAP_SYS_LED

#ifdef DS_CAP_SYS_LOG
      static Print *log;                              // System log
#endif // DS_CAP_SYS_LOG

#ifdef DS_CAP_SYS_RESET
      static uint32 getResetReason();                 // Return reset reason
      // TODO: add reason string and the reset call itself
#endif // DS_CAP_SYS_RESET

#ifdef DS_CAP_SYS_RTCMEM
      static bool getRTCMem(uint32_t* /* result */, const uint8_t idx = 0, const uint8_t num = 1); // Read 'num' 4 bytes slots from RTC memory offset 'idx' into 'result'
      static bool setRTCMem(const uint32_t* /* source */, const uint8_t idx = 0, const uint8_t num = 1); // Store 'num' 4 bytes slots into RTC memory offset 'idx' from 'source'
#endif // DS_CAP_SYS_RTCMEM

#ifdef DS_CAP_SYS_TIME
    protected:
      static time_sync_t time_sync_status;            // Time synchronization status
      static time_t time_sync_time;                   // Last time the time was synchronized
      static uint8_t time_change_flags;               // Time change flags
      static void timeSyncHandler();                  // Time sync event handler

    public:
      static time_t time;                             // Current time (Unix)
      static struct tm tm_time;                       // Current time as structure

      static time_t getTimeSyncTime();                // Return last time sync time
      static void setTimeSyncTime(const time_t /* new_time */); // Set last time sync time
      static time_sync_t getTimeSyncStatus();         // Return time sync status
      static void setTimeSyncStatus(const time_sync_t /* new_status */); // Set time sync status
      static time_t getTime();                        // Return current time
      static void setTime(const time_t /* new_time */);     // Set current time
      static String getTimeStr();                     // Return current time string
      static String getTimeStr(const time_t /* t */); // Return time string for a given time
      static void (*onTimeSync)();                    // Hook to be called when time gets synchronized
      static bool newSecond();                        // Return true if new second has arrived
      static bool newMinute();                        // Return true if new minute has arrived
      static bool newHour();                          // Return true if new hour has arrived
      static bool newDay();                           // Return true if new day has arrived
      static bool newWeek();                          // Return true if new week has arrived
      static bool newMonth();                         // Return true if new month has arrived
      static bool newYear();                          // Return true if new year has arrived
#endif // DS_CAP_SYS_TIME

#ifdef DS_CAP_SYS_UPTIME
      static String getUptimeStr();                   // Return uptime as string
#ifdef DS_CAP_SYS_TIME
      static String getBootTimeStr();                 // Return boot time string
#endif // DS_CAP_SYS_TIME
#endif // DS_CAP_SYS_UPTIME

#ifdef DS_CAP_SYS_FS
      static fs::FS& fs;                              // File system control object
#endif // DS_CAP_SYS_FS

#ifdef DS_CAP_SYS_NETWORK
    public:
      static const char *hostname;                    // Hostname; also the SSID of the temporary configuration network
#ifndef DS_CAP_WIFIMANAGER
      static const char *wifi_ssid;                   // Wi-Fi SSID
      static const char *wifi_pass;                   // Wi-Fi password
#endif // !DS_CAP_WIFIMANAGER

      static void connectNetwork(                     // Connect to a known network. LED can be used to signal connection progress
#ifdef DS_CAP_SYS_LED
        JLed *led = nullptr
#endif // DS_CAP_SYS_LED
      );
      static String getNetworkName();                 // Return network name
      static String getNetworkDetails();              // Return network details
      static String getLocalIPAddress();              // Return assigned IP address
#ifdef DS_CAP_SYS_TIME
      static const char *time_server;                 // NTP server (defaults to pool.ntp.org)
      static String getTimeServer();                  // Return NTP server name (possibly, overridden via DHCP)
#endif // DS_CAP_SYS_TIME
      static bool networkIsConnected();               // Return true if network connected
#endif // DS_CAP_SYS_NETWORK

#ifdef DS_CAP_WIFIMANAGER
    protected:
      static bool need_network_config;                // True if network configuration is required

    public:
      static void configureNetwork();                 // Configure network (blocking)
      static bool needsNetworkConfiguration();        // Return true if network needs configuration
      static void requestNetworkConfiguration();      // Request network configuration
      static String getNetworkConfigPassword();       // Return Wi-Fi configuration password
#endif // DS_CAP_WIFIMANAGER

#ifdef DS_CAP_WEBSERVER
    protected:
      static void serveFront();                       // Serve the front page
      static void serveAbout();                       // Serve the "about" page
#ifdef DS_CAP_APP_LOG
      static void serveAppLog();                      // Serve the "log" page
#endif // DS_CAP_APP_LOG
#ifdef DS_CAP_WEB_TIMERS
      static void serveTimers();                      // Serve the "timers" page
      static void serveTimersSave();                  // Serve the "timers save" page
#endif // DS_CAP_WEB_TIMERS

    public:
      static ESP8266WebServer web_server;             // Web server
      static String web_page;                         // Web page buffer
      static void pushHTMLHeader(const String& title = "", const String& head_user = "", bool redirect = false);  // Add standard header to the web page
      static void pushHTMLFooter();                   // Add standard footer to the web page
      static void (*registerWebPages)();              // Hook for registering user-supplied pages
      static void sendWebPage();                      // Send a web page
#ifdef DS_CAP_WEB_TIMERS
      static std::forward_list<String> timer_actions; // List of timer actions
#endif // DS_CAP_WEB_TIMERS
#endif // DS_CAP_WEBSERVER

#ifdef DS_CAP_BUTTON
    protected:
      static void buttonEventHandler(ace_button::AceButton* /* button */, uint8_t /* event_type */, uint8_t /* button_state */); // Button handler

    public:
      static ace_button::AceButton button;            // Builtin button on pin BUTTON_BUILTIN (0 by default)
      static void (*onButtonInit)();                  // User code to initialize button
      static void (*onButtonPress)(ace_button::AceButton* /* button */, uint8_t /* event_type */, uint8_t /* button_state */); // Hook to be called when button is operated
#endif // DS_CAP_BUTTON

#ifdef DS_CAP_TIMERS_ABS
    public:
      static bool abs_timers_active;                  // True if absolute or solar timers should be served
      static std::forward_list<TimerAbsolute *> timers; // List of timers
      static TimerAbsolute* getTimerAbsByID(const int /* id */); // Return absolute timer with a matching ID
      static void (*timerHandler)(const TimerAbsolute* /* timer */); // Timer handler
#endif // DS_CAP_TIMERS_ABS

#ifdef DS_CAP_TIMERS_SOLAR
    protected:
      static uint16_t getSolarEvent(const timer_type_t /* ev_type */); // Return solar event time

    public:
      static uint16_t getSunrise();                  // Return sunrise time (in minutes from midnight)
      static uint16_t getSunset();                   // Return sunset time (in minutes from midnight)
#endif // DS_CAP_TIMERS_SOLAR

#ifdef DS_CAP_WEB_TIMERS
    protected:
#ifdef DS_CAP_SYS_FS
      static String timers_cfg_name;                 // Full path to the timers configuration file
#endif // DS_CAP_SYS_FS
      static void timersJS(String& /* str */);       // Generate timer configuration as JavaScript code
#endif // DS_CAP_WEB_TIMERS

  };

} // namespace ds

#endif // _DS_SYSTEM_H_
