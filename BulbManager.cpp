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

// Count active bulbs
uint8_t BulbManager::countActive() {
  nabulbs = 0;
  for (uint8_t i = 0; i < bulbs.size(); i++)
    nabulbs += bulbs.get(i)->isActive();
  return nabulbs;
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

      for (uint8_t j = 0; j < bulbs.size(); j++) {
        YBulb *bulb = bulbs.get(j);
        if (*bulb == bulbid_c) {
          bulb->activate();
          break;
        }
      }
    }

    if (countActive() == n)
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
  unsigned int nargs = System::web_server.args();
  const size_t used_eeprom_size = 2 + 1 + 1 + (YBulb::ID_LENGTH + 1) * nargs;   // TODO: maybe put some constraint on nargs (externally provided parameter)
  EEPROM.begin(used_eeprom_size);
  unsigned int eeprom_addr = 4;

  deactivateAll();

  if(nargs) {
    for(unsigned int i = 0; i < nargs; i++) {
      unsigned char n = System::web_server.arg(i).c_str()[0] - '0';
      if (n < bulbs.size()) {
        YBulb *bulb = bulbs.get(n);
        char bulbid_c[YBulb::ID_LENGTH + 1] = {0,};
        strncpy(bulbid_c, bulb->getID().c_str(), YBulb::ID_LENGTH);
        EEPROM.put(eeprom_addr, bulbid_c);
        eeprom_addr += sizeof(bulbid_c);
        bulb->activate();
      } else
        System::log->printf(TIMED("Bulb #%d does not exist\n"), n);
    }

    if (countActive()) {

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

// Discover bulbs
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
    YBulb *new_bulb = nullptr;

    // Check if we already have this bulb in the list
    // TODO: add search function
    for (uint8_t i = 0; i < bulbs.size(); i++) {
      YBulb *bulb = bulbs.get(i);
      if (*bulb == *discovered_bulb) {
        new_bulb = bulb;
        break;
      }
    }
    if (new_bulb) {
      System::log->printf(TIMED("Received bulb id: %s is already registered; ignoring\n"), discovered_bulb->getID().c_str());
      delete discovered_bulb;
    } else {
      bulbs.add(discovered_bulb);
      System::log->printf(TIMED("Registered bulb id: %s, name: %s, model: %s, power: %s\n"),
        discovered_bulb->getID().c_str(), discovered_bulb->getName().c_str(),
        discovered_bulb->getModel().c_str(), discovered_bulb->getPowerStr().c_str());
    }
  }
  
  System::log->printf(TIMED("Total bulbs discovered: %d\n"), bulbs.size());
  return bulbs.size();
}

// Yeelight bulb flip
int BulbManager::flip() {
  int ret = 0;
  if (nabulbs) {
    for (uint8_t i = 0; i < bulbs.size(); i++) {
      YBulb *bulb = bulbs.get(i);
      if (bulb && bulb->isActive()) {
        if (bulb->flip(client)) {
          System::log->printf(TIMED("Bulb connection to %s failed\n"), bulb->getIP().toString().c_str());
          ret = -2;
          yield();        // Connection timeout is lenghty; allow for background processing (is this really needed?)
        } else
          System::log->printf(TIMED("Bulb %d toggle sent\n"), i + 1);
      }
    }
  } else {
    System::log->printf(TIMED("No linked bulbs found\n"));
    ret = -1;
  }
  return ret;
}

// Activate all bulbs
void BulbManager::activateAll() {
  for (uint8_t i = 0; i < bulbs.size(); i++)
    bulbs.get(i)->activate();
  nabulbs = 0;    
}

// Deactivate all bulbs
void BulbManager::deactivateAll() {
  for (uint8_t i = 0; i < bulbs.size(); i++)
    bulbs.get(i)->deactivate();
  nabulbs = 0;  
}

// Print bulbs status in HTML
void BulbManager::printStatusHTML() const {
  for (uint8_t i = 0; i < const_cast<BulbManager *>(this)->bulbs.size(); i++) {
    const auto bulb = const_cast<BulbManager *>(this)->bulbs.get(i);
    if (bulb->isActive())
      bulb->printStatusHTML(System::web_page);
  }
}

// Print bulb configuration controls in HTML
void BulbManager::printConfHTML() const {
  for (uint8_t i = 0; i < const_cast<BulbManager *>(this)->bulbs.size(); i++)
    const_cast<BulbManager *>(this)->bulbs.get(i)->printConfHTML(System::web_page, i);
}
