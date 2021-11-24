/* Yeelight Smart Switch App for ESP8266
 * Bulb manager definitions
 * (c) DNS 2018-2021
 */

#ifndef BULBMANAGER_H
#define BULBMANAGER_H

#include <WiFiClient.h>                    // Wi-Fi communication
#include <vector>                          // Dynamic array
#include "Yeelight.h"                      // Yeelight support

class BulbManager {

  protected:

    std::vector<dsy::YBulb *> bulbs;       // List of known bulbs
    uint8_t nabulbs;                       // Number of active bulbs
    WiFiClient client;                     // Wi-Fi client

    static const uint8_t EEPROM_FORMAT_VERSION = 49;  // The first version of the format stored 1 bulb id right after the marker. ID stars with ASCII '0' == 48

    dsy::YBulb* find(const String&) const; // Find a bulb by ID
    dsy::YBulb* find(const dsy::YBulb&) const;        // Find a bulb with the same ID

  public:

    typedef enum Event { EVENT_FLIP, EVENT_ON, EVENT_OFF } event_t; // Possible actions

    BulbManager();                         // Constructor
    ~BulbManager();                        // Destructor
    void begin();                          // Start operation
    void processEvent(event_t, const String&); // Process external event
    void load();                           // Load stored configuration
    void save();                           // Save new configuration
    uint8_t discover();                    // Discover bulbs. Returns number of known bulbs
    bool turnOn();                         // Turn on bulbs. Returns true on full success
    bool turnOff();                        // Turn off bulbs. Returns true on full success
    bool flip();                           // Flip bulbs. Returns true on full success
    bool isOn() const;                     // Return true if lights are on
    bool isOff() const { return !isOn(); } // Return true if lights are off
    void activateAll();                    // Activate all bulbs
    void deactivateAll();                  // Deactivate all bulbs
    uint8_t getNum() const { return bulbs.size(); }  // Return number of known bulbs
    uint8_t getNumActive() const { return nabulbs; } // Return number of active bulbs
    bool isLinked() const { return nabulbs; }        // Return true if there are linked bulbs
    void printStatusHTML(String &) const;  // Print bulbs status in HTML
    void printConfHTML(String &) const;    // Print bulb configuration controls in HTML
};

// Declare a singletone-like instance
extern BulbManager bulb_manager;           // Global bulb manager

#endif // BULBMANAGER_H
