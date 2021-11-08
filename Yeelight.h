/* Yeelight Smart Switch App for ESP8266
 * Yeelight bulb definitions
 * (c) DNS 2018-2021
 */

#ifndef _YEELIGHT_H_
#define _YEELIGHT_H_

#include <WiFiClient.h>           // Wi-Fi support
#include <WiFiUdp.h>              // UDP support

// Yeelight bulb object
class YBulb {

  protected:

    String id;                                   // Yeelight device ID
    String ip;                                   // IP-address of the bulb
    uint16_t port;                               // Port of the bulb
    String name;                                 // Bulb name
    String model;                                // Bulb model ("color", "stripe", etc)
    bool power;                                  // Current power state (true = "on")
    bool active;                                 // True if the bulb is actively controlled (e.g., linked to a switch)

  public:

    static const uint8_t ID_LENGTH = 18;         // Length of the Yeelight device ID (chars)
    static const uint16_t TIMEOUT = 1000;        // Bulb connection timeout (ms)

    YBulb(const String& yid, const String& yip, const uint16_t yport = 55443) :
      id(yid), ip(yip), port(yport), power(false), active(false) {}      // Constructor
    ~YBulb(){};

    const String& getID() const { return id; }   // Return bulb ID
    const String& getIP() const { return ip; }   // Return bulb IP-address
    // TODO?: setIP()
    uint16_t getPort() const { return port; }    // Return bulb port
    const String& getName() const { return name; }           // Return bulb name
    void setName(const String& yname) { name = yname; }      // Set bulb name
    const String& getModel() const { return model; }         // Return bulb model
    void setModel(const String& ymodel) { model = ymodel; }  // Set bulb model
    bool getPower() const { return power; }      // Return bulb power state (true = "on")
    void setPower(bool new_power) { power = new_power; }     // Set bulb power state (true = "on")
    bool isActive() const { return active; }     // True if bulb control is active
    void activate() { active = true; }           // Activate bulb control
    void deactivate() { active = false; }        // Deactivate bulb control
    int flip(WiFiClient&) const;                 // Toggle bulb power state
    void printStatusHTML(String&) const;         // Print bulb status in HTML
    void printConfHTML(String&, uint8_t) const;  // Print bulb configuration controls in HTML
    bool operator==(const String& id2) const {   // Bulb comparison
      return id == id2;
    }
};

// Yeelight discovery
class YDiscovery {

  protected:

    WiFiUDP udp;                                 // UDP socket used for discovery process
    unsigned long t0;                            // Discovery start time
    unsigned long t1;                            // Discovery time in progress

  public:

    const size_t MAX_REPLY_SIZE = 512;           // With contemporary bulbs, the reply is about 500 bytes
    static const unsigned long TIMEOUT = 3000;   // Discovery timeout (ms)

    YDiscovery();                                // Constructor
    bool send();                                 // Send discovery request
    YBulb *receive();                            // Receive discovery reply
    bool isInProgress();                         // True if discovery process is in progress
};

#endif // _YEELIGHT_H_
