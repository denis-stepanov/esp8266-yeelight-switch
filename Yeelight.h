/* Yeelight Smart Switch App for ESP8266
 * Yeelight bulb definitions
 * (c) DNS 2018-2021
 */

#ifndef _YEELIGHT_H_
#define _YEELIGHT_H_

#include <WiFiClient.h>           // Wi-Fi support
#include <WiFiUdp.h>              // UDP support

// FIXME descriptions
// FIXME camelCase

// Yeelight bulb object. TODO: make a true library out of this
class YBulb {

  protected:

    String id;
    String ip;
    uint16_t port;
    String name;
    String model;
    bool power;
    bool active;

  public:

    static const uint8_t ID_LENGTH = 18;          // Length of the Yeelight device ID (chars)
    static const uint16_t TIMEOUT = 1000;         // Bulb connection timeout (ms)

    YBulb(const String& yid, const String& yip, const uint16_t yport = 55443) :
      id(yid), ip(yip), port(yport), power(false), active(false) {}
    ~YBulb(){};

    const String& GetID() const { return id; }
    const String& GetIP() const { return ip; }
    // TODO: setIP()?
    uint16_t GetPort() const { return port; }
    const String& GetName() const { return name; }
    void SetName(const String& yname) { name = yname; }
    const String& GetModel() const { return model; }
    void SetModel(const String& ymodel) { model = ymodel; }
    bool GetPower() const { return power; }
    void SetPower(bool new_power) { power = new_power; }
    bool isActive() const { return active; }
    void Activate() { active = true; }
    void Deactivate() { active = false; }
    int Flip(WiFiClient&) const;
    void printStatusHTML(String&) const;         // Print bulb status in HTML
    void printConfHTML(String&, uint8_t) const;  // Print bulb configuration controls in HTML
    bool operator==(const String& id2) const {
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
