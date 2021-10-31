/* Yeelight Smart Switch App for ESP8266
 * Bulb manager definitions
 * (c) DNS 2018-2021
 */

#ifndef _BULBMANAGER_H_
#define _BULBMANAGER_H_

#include <WiFiClient.h>                    // Wi-Fi communication
#include <LinkedList.h>                    // https://github.com/ivanseidel/LinkedList
#include "YBulb.h"                         // Yeelight support

class BulbManager {

  protected:

    LinkedList<YBulb *> bulbs;             // List of known bulbs
    uint8_t nabulbs;                       // Number of active bulbs
    WiFiClient client;                     // Wi-Fi client

    static const uint8_t EEPROM_FORMAT_VERSION = 49;  // The first version of the format stored 1 bulb id right after the marker. ID stars with ASCII '0' == 48

    uint8_t countActive();                 // Count active bulbs

  public:

    BulbManager();                         // Constructor
    void load();                           // Load stored configuration
    void save();                           // Save new configuration
    uint8_t discover();                    // Discover bulbs
    int flip();                            // Flip bulbs
    void activateAll();                    // Activate all bulbs
    void deactivateAll();                  // Deactivate all bulbs
    uint8_t getNum() const { return const_cast<BulbManager *>(this)->bulbs.size(); }  // Return number of known bulbs
    uint8_t getNumActive() const { return nabulbs; } // Return number of active bulbs
    void printStatusHTML() const;          // Print bulbs status in HTML
    void printConfHTML() const;            // Print bulb configuration controls in HTML
};

#endif // _BULBMANAGER_H_