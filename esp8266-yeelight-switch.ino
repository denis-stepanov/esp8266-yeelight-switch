/* Yeelight Smart Switch App for ESP8266
 * Developed & tested with a Witty Cloud Development board
 * (c) DNS 2018-2021
 *
 * Usage:
 * 1) review the configuration settings in MySystem.h; compile and flash your ESP8266;
 * 2) boot, long press the button until the LED lights up, connect your computer to the Wi-Fi network "ybutton1", password "42ybutto",
 *    go to captive portal, enter and save your Wi-Fi network credentials;
 * 3) in your Wi-Fi network, go to http://ybutton1.local, run the Yeelight scan and link the switch to the bulb found;
 * 4) use the push button to control your bulb manually;
 * 5) access to http://ybutton1.local/flip to toggle the bulb from a script.
 */

#include "MySystem.h"                      // System-level definitions
#include "BulbManager.h"                   // Bulb manager

using namespace ds;

// System configuration
const char *System::app_name    PROGMEM = "ESP8266 Yeelight Switch";
const char *System::app_version PROGMEM = "2.0.0";
const char *System::app_url     PROGMEM = "https://github.com/denis-stepanov/esp8266-yeelight-switch";
const char *System::hostname    PROGMEM = "ybutton1"; // <hostname>.local in the local network. Also, SSID of the temporary network for Wi-Fi configuration

// Global variables
extern bool button_pressed;               // Button flag

// Program setup
void setup() {

  System::begin();
  bulb_manager.begin();

  // Register supported timer actions
  System::timer_actions.push_front("light toggle");
  System::timer_actions.push_front("light off");
  System::timer_actions.push_front("light on");
}

// Program loop
void loop() {

  // Check the button state
  if (button_pressed) {
    button_pressed = false;
    bulb_manager.processEvent(BulbManager::EVENT_FLIP, "Button pressed");
  }

  // Background processing
  System::update();
}
