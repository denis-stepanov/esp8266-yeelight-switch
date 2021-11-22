/* Yeelight Smart Switch App for ESP8266
 * * Timers support
 * (c) DNS 2021
 */

#include "MySystem.h"                      // System-level definitions
#include "BulbManager.h"                   // Bulb manager

using namespace ds;

// Timer handler
void myTimerHandler(const TimerAbsolute* timer) {

  String reason("Timer \"");
  reason += timer->getAction();
  reason += "\" fired";

  if (timer->getAction() == "light on") {
    bulb_manager.processEvent(BulbManager::EVENT_ON, reason);
  }
  else
  if (timer->getAction() == "light off") {
    bulb_manager.processEvent(BulbManager::EVENT_OFF, reason);
  }
  else
  if (timer->getAction() == "light toggle") {
    bulb_manager.processEvent(BulbManager::EVENT_FLIP, reason);
  }
}

// Install handler
void (*System::timerHandler)(const TimerAbsolute*) = myTimerHandler;
