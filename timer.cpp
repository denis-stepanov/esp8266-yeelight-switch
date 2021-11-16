/* Yeelight Smart Switch App for ESP8266
 * * Timers support
 * (c) DNS 2021
 */

#include "MySystem.h"                      // System-level definitions
#include "BulbManager.h"                   // Bulb manager

using namespace ds;

// Timer handler
void myTimerHandler(const TimerAbsolute* timer) {

  String msg("Timer \"");
  msg += timer->getAction();
  msg += "\" fired: bulbs are ";

  const auto bulbs_on = bulb_manager.isOn();

  if (timer->getAction() == "light on") {
    if (bulbs_on)
      msg += "already ON";
    else {
      bulb_manager.turnOn();
      msg += "going to ON";
    }
  }
  else
  if (timer->getAction() == "light off") {
    if (bulbs_on) {
      bulb_manager.turnOff();
      msg += "going to OFF";
    } else
      msg += "already OFF";
  }
  else
  if (timer->getAction() == "light toggle") {
    bulb_manager.flip();
    msg += bulbs_on ? "going to OFF" : "going to ON";
  }
  System::appLogWriteLn(msg);
}

// Install handler
void (*System::timerHandler)(const TimerAbsolute*) = myTimerHandler;
