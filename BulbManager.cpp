/* Yeelight Smart Switch App for ESP8266
 * Bulb manager implementation
 * (c) DNS 2018-2021
 */

#include "BulbManager.h"                   // Bulb manager
#include <EEPROM.h>                        // EEPROM support
#include "MySystem.h"                      // System-level definitions

using namespace ds;

// Constructor
BulbManager::BulbManager(): nabulbs(0) {
  
  // Reduce connection timeout for inactive bulbs
  client.setTimeout(YBulb::TIMEOUT);
}

// Destructor
BulbManager::~BulbManager() {
  for (auto bulb : bulbs)
    delete bulb;
}

// Find a bulb by ID
YBulb* BulbManager::find(const String& id) const {
  for (const auto bulb : bulbs)
    if (*bulb == id)
      return bulb;
  return nullptr;
}

// Find a bulb with the same ID
YBulb* BulbManager::find(const YBulb& bulb) const {
  return find(bulb.getID());
}

// Start operation
void BulbManager::begin() {
  discover();
  load();
}

// Process external event
void BulbManager::processEvent(event_t event, const String& reason = "") {
  const unsigned long BLINK_DELAY = 100;    // (ms)
  const unsigned long GLOW_DELAY = 1000;    // (ms)

  // LED diagnostics:
  // 1 blink  - light flip OK
  // 1 + 2 blinks - one of the bulbs did not respond
  // 2 blinks - button not linked to bulbs
  // 1 glowing - Wi-Fi disconnected
  if (System::networkIsConnected()) {
    if (isLinked()) {

      // Flipping may block, causing JLED-style blink not being properly processed. Hence, force sequential processing (first blink, then flip)
      // To make JLED working smoothly in this case, an asynchronous WiFiClient.connect() method would be needed
      // This is not included in ESP8266 Core (https://github.com/esp8266/Arduino/issues/922), but is available as a separate library (like ESPAsyncTCP)
      // Since, for this project, it is a minor issue (flip being sent to bulbs with 100 ms delay), we stay with the blocking connect()
      System::led.On().Update();
      delay(BLINK_DELAY);       // 1 blink
      System::led.Off().Update();

      String msg(reason);
      msg += reason.isEmpty() ? "Bulbs are" : "; bulbs are ";
      const auto bulbs_on = isOn();
      switch (event) {
        case EVENT_ON:   msg += bulbs_on ? "already ON"  : "going to ON"; break;
        case EVENT_OFF:  msg += bulbs_on ? "going to OFF": "already OFF"; break;
        case EVENT_FLIP: msg += bulbs_on ? "going to OFF": "going to ON"; break;
      }
      System::appLogWriteLn(msg, true);

      auto action_ok = false;
      switch (event) {

        case EVENT_ON:
          if (!bulbs_on)
            action_ok = turnOn();
          break;

        case EVENT_OFF:
          if (bulbs_on)
            action_ok = turnOff();
          break;

        case EVENT_FLIP:
          action_ok = flip();
          break;
      }
      if (!action_ok)

        // Some bulbs did not respond
        // Because of connection timeout, the blinking will be 1 + pause + 2
        System::led.Blink(BLINK_DELAY, BLINK_DELAY * 2).Repeat(2);  // 2 blinks

    } else {

      // Button not linked
      System::log->printf(TIMED("Button not linked to bulbs\n"));
      System::led.Blink(BLINK_DELAY, BLINK_DELAY * 2).Repeat(2);    // 2 blinks
    }
  } else {

    // No Wi-Fi
    System::log->printf(TIMED("No Wi-Fi connection\n"));
    System::led.Breathe(GLOW_DELAY).Repeat(1);            // 1 glowing
  }
}

// Load stored configuration
void BulbManager::load() {

  // Load settings from EEPROM
  EEPROM.begin(4);
  if (EEPROM.read(0) == 'Y' && EEPROM.read(1) == 'B' && EEPROM.read(2) == EEPROM_FORMAT_VERSION) {
    char bulbid_c[YBulb::ID_LENGTH + 1] = {0,};
    const uint8_t n = EEPROM.read(3);
    System::log->printf(TIMED("Found %d bulb%s configuration in EEPROM\n"), n, n == 1 ? "" : "s");
    EEPROM.end();
    EEPROM.begin(2 + 1 + 1 + (YBulb::ID_LENGTH + 1) * n);
    unsigned int eeprom_addr = 4;
    for (uint8_t i = 0; i < n; i++) {
      EEPROM.get(eeprom_addr, bulbid_c);
      eeprom_addr += sizeof(bulbid_c);

      auto existing_bulb = find(bulbid_c);
      if (existing_bulb) {
        nabulbs += !existing_bulb->isActive();
        existing_bulb->activate();
      }
    }

    if (nabulbs == n)
      System::log->printf(TIMED("Successfully linked to %d bulb%s\n"), nabulbs, nabulbs == 1 ? "" : "s");
    else
      System::log->printf(TIMED("Linking completed with %d out of %d bulb%s skipped\n"), n - nabulbs, n, n == 1 ? "" : "s");
  } else
    System::log->printf(TIMED("No bulb configuration found in EEPROM; need to link bulbs manually\n"));
  EEPROM.end();

}

// Save new configuration
// EEPROM format:
//   0-1: 'YB' - Yeelight Bulb configuration marker
//     2: format version. Increment each time the format changes
//     3: number of stored bulbs
//  4-22: <selected bulb ID> (19 bytes, null-terminated)
//      : ...
void BulbManager::save() {
  const unsigned int nargs = System::web_server.args();
  const size_t used_eeprom_size = 2 + 1 + 1 + (YBulb::ID_LENGTH + 1) * nargs;   // TODO: maybe put some constraint on nargs (externally provided parameter)
  EEPROM.begin(used_eeprom_size);
  unsigned int eeprom_addr = 4;

  deactivateAll();

  if(nargs) {
    for(unsigned int i = 0; i < nargs; i++) {
      const unsigned int n = System::web_server.arg(i).c_str()[0] - '0';
      if (n < bulbs.size()) {
        const auto bulb = bulbs[n];
        char bulbid_c[YBulb::ID_LENGTH + 1] = {0,};
        strncpy(bulbid_c, bulb->getID().c_str(), YBulb::ID_LENGTH);
        EEPROM.put(eeprom_addr, bulbid_c);
        eeprom_addr += sizeof(bulbid_c);
        bulb->activate();
        nabulbs++;
      } else
        System::log->printf(TIMED("Bulb #%d does not exist\n"), n);
    }

    if (nabulbs) {

      // Write the header
      EEPROM.write(0, 'Y');
      EEPROM.write(1, 'B');
      EEPROM.write(2, EEPROM_FORMAT_VERSION);
      EEPROM.write(3, nabulbs);
      System::log->printf(TIMED("%d bulb%s stored in EEPROM, using %u byte(s)\n"), nabulbs, nabulbs == 1 ? "" : "s", eeprom_addr);
    } else
      System::log->printf(TIMED("No bulbs were stored in EEPROM\n"));
  } else {

    // Unlink all

    // Overwriting the EEPROM marker will effectively cause forgetting the settings
    EEPROM.write(0, 0);
    System::log->printf(TIMED("Bulbs unlinked from the switch"));
  }

  // TODO: check for errors?
  EEPROM.commit();
  EEPROM.end();
}

// Discover bulbs. Returns number of known bulbs
// Note - no bulb removal at the moment
uint8_t BulbManager::discover() {
  YDiscovery discovery;                           // Yeelight discovery

  // Send broadcast message
  System::log->printf(TIMED("Sending Yeelight discovery request...\n"));
  discovery.send();

  while (discovery.isInProgress()) {
    const auto discovered_bulb = discovery.receive();
    if (!discovered_bulb)
      continue;

    // Check if we already have this bulb in the list
    if (find(*discovered_bulb)) {
      System::log->printf(TIMED("Received bulb id: %s is already registered; ignoring\n"), discovered_bulb->getID().c_str());
      delete discovered_bulb;
    } else {
      bulbs.push_back(discovered_bulb);
      System::log->printf(TIMED("Registered bulb id: %s, name: %s, model: %s, power: %s\n"),
        discovered_bulb->getID().c_str(), discovered_bulb->getName().c_str(),
        discovered_bulb->getModel().c_str(), discovered_bulb->getPowerStr().c_str());
    }
  }
  
  System::log->printf(TIMED("Total bulbs discovered: %d\n"), bulbs.size());
  return bulbs.size();
}

// Turn on bulbs. Returns true on full success
bool BulbManager::turnOn() {
  return isOn() ? true : flip();
}

// Turn off bulbs. Returns true on full success
bool BulbManager::turnOff() {
  return isOff() ? true : flip();
}

// Flip bulbs. Returns true on full success
bool BulbManager::flip() {
  auto ret = true;
  if (isLinked()) {
    for (const auto bulb : bulbs) {
      if (bulb->isActive()) {
        if (bulb->flip(client))
          System::log->printf(TIMED("Bulb %s toggle sent\n"), bulb->getID().c_str());
        else {
          System::log->printf(TIMED("Bulb connection to %s failed\n"), bulb->getIP().toString().c_str());
          ret = false;
          yield();        // Connection timeout is lenghty; allow for background processing (is this really needed?)
        }
      }
    }
  } else {
    System::log->printf(TIMED("No linked bulbs found\n"));
    ret = false;
  }
  return ret;
}

// Return true if lights are on
bool BulbManager::isOn() const {

  // Ignore for now that bulbs could be in discordant states (issue #21). Treat first active bulb state as the global state
  for (auto bulb : bulbs)
    if (bulb->isActive())
      return bulb->getPower();
  return false;
}

// Activate all bulbs
void BulbManager::activateAll() {
  for (auto bulb : bulbs)
    bulb->activate();
  nabulbs = bulbs.size();
}

// Deactivate all bulbs
void BulbManager::deactivateAll() {
  for (auto bulb : bulbs)
    bulb->deactivate();
  nabulbs = 0;  
}

// Print bulbs status in HTML
void BulbManager::printStatusHTML(String &page) const {
  page +=
    "<table border=\"1\" cellpadding=\"3\" cellspacing=\"0\" style=\"font-family: monospace; border-collapse: collapse;\">\n"
    "<tr><th>Name</th><th>ID</th><th>IP Address</th><th>Model</th><th>Power</th></tr>\n";
  if (isLinked()) {
    for (const auto bulb : bulbs)
      if (bulb->isActive())
        bulb->printStatusHTML(page);
  } else
    page += "<tr><td colspan=\"5\" style=\"text-align: center\">- Not linked to a bulb -</tr>\n";
  page += "</table>\n";
}

// Print bulb configuration controls in HTML
void BulbManager::printConfHTML(String &page) const {
  page +=
    "<table border=\"1\" cellpadding=\"3\" cellspacing=\"0\" style=\"font-family: monospace; border-collapse: collapse;\">\n"
    "<tr><th>Link</th><th>Name</th><th>ID</th><th>IP Address</th><th>Model</th><th>Power</th></tr>\n";
  for (uint8_t i = 0; i < bulbs.size(); i++)
    bulbs[i]->printConfHTML(page, i);
  page += "</table>\n";
}

// Define a singletone-like instance
BulbManager bulb_manager;           // Global bulb manager
