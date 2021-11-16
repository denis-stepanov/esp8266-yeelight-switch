/* Yeelight Smart Switch App for ESP8266
 * * Button support
 * (c) DNS 2018-2021
 */

#include "MySystem.h"                      // System-level definitions

using namespace ds;
using namespace ace_button;

auto button_pressed = false;               // Button flag

// Button handler
void handleButtonEvent(AceButton* /* button */, uint8_t eventType, uint8_t /* buttonState */) {
  button_pressed = eventType == AceButton::kEventPressed;
}

// Install handler
void (*System::onButtonPress)(AceButton*, uint8_t, uint8_t) = handleButtonEvent;
