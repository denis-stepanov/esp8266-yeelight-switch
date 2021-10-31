/* Yeelight Smart Switch App for ESP8266
 * Developed & tested with a Witty Cloud Development board
 * (c) DNS 2018-2021
 *
 * Usage:
 * 1) review the configuration settings at the top of the program and in MySystem.h; compile and flash your ESP8266;
 * 2) boot, long press the button until the LED lights up, connect your computer to the Wi-Fi network "ybutton1", password "42ybutto",
 *    go to captive portal, enter and save your Wi-Fi network credentials;
 * 3) in your Wi-Fi network, go to http://ybutton1.local, run the Yeelight scan and link the switch to the bulb found;
 * 4) use the push button to control your bulb manually;
 * 5) access to http://ybutton1.local/flip to toggle the bulb from a script.
 */

#include "MySystem.h"                      // System-level definitions

#include "BulbManager.h"                   // Bulb manager

using namespace ds;

// Configuration
const char *System::hostname PROGMEM = "ybutton1";   // <hostname>.local in the local network. Also SSID of the temporary network for Wi-Fi configuration

// Normally no need to change below this line
const char *System::app_name    PROGMEM = "ESP8266 Yeelight Switch";
const char *System::app_version PROGMEM = "2.0.0-beta.4";
const char *System::app_url     PROGMEM = "https://github.com/denis-stepanov/esp8266-yeelight-switch";

using namespace ace_button;

// Global variables
const unsigned long BLINK_DELAY = 100;    // (ms)
const unsigned long GLOW_DELAY = 1000;    // (ms)
bool button_pressed = false;
BulbManager bulb_manager;                 // Bulb manager

// Button handler
void handleButtonEvent(AceButton* /* button */, uint8_t eventType, uint8_t /* buttonState */) {
  button_pressed = eventType == AceButton::kEventPressed;
}
void (*System::onButtonPress)(AceButton*, uint8_t, uint8_t) = handleButtonEvent;

// Program setup
void setup() {
  System::begin();

  // Run discovery
  bulb_manager.discover();

  // Load stored configuration
  bulb_manager.load();
}

// Program loop
void loop() {

  // Check the button state
  if (button_pressed) {
    button_pressed = false;
    System::appLogWriteLn("Button pressed", true);

    // LED diagnostics:
    // 1 blink  - light flip OK
    // 1 + 2 blinks - one of the bulbs did not respond
    // 2 blinks - button not linked to bulbs
    // 1 glowing - Wi-Fi disconnected
    if (!System::networkIsConnected()) {

      // No Wi-Fi
      System::log->printf(TIMED("No Wi-Fi connection\n"));
      System::led.Breathe(GLOW_DELAY).Repeat(1);            // 1 glowing
    } else {
      if (bulb_manager.getNumActive()) {

        // Flipping may block, causing JLED-style blink not being properly processed. Hence, force sequential processing (first blink, then flip)
        // To make JLED working smoothly in this case, an asynchronous WiFiClient.connect() method would be needed
        // This is not included in ESP8266 Core (https://github.com/esp8266/Arduino/issues/922), but is available as a separate library (like ESPAsyncTCP)
        // Since, for this project, it is a minor issue (flip being sent to bulbs with 100 ms delay), we stay with blocking connect()
        System::led.On().Update();
        delay(BLINK_DELAY);       // 1 blink
        System::led.Off().Update();

        if (bulb_manager.flip())

          // Some bulbs did not respond
          // Because of connection timeout, the blinking will be 1 + pause + 2
          System::led.Blink(BLINK_DELAY, BLINK_DELAY * 2).Repeat(2);  // 2 blinks

      } else {

        // Button not linked
        System::log->printf(TIMED("Button not linked to bulbs\n"));
        System::led.Blink(BLINK_DELAY, BLINK_DELAY * 2).Repeat(2);    // 2 blinks
      }
    }
  }

  // Background processing
  System::update();
}
