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
// TODO: this is too much of a plain C. Arduinize

// Yeelight bulb object. TODO: make a true library out of this
class YBulb {

  protected:

    char id[24];
    char ip[16];
    uint16_t port;
    char name[32];
    char model[16];
    bool power;
    bool active;

  public:

   static const uint8_t ID_LENGTH = 18;        // Length of the Yeelight device ID (chars)
   static const uint16_t TIMEOUT = 1000;       // Bulb connection timeout (ms)

    YBulb(const char *, const char *, const uint16_t);
    ~YBulb(){};

    const char *GetID() const { return id; }
    const char *GetIP() const { return ip; }
    uint16_t GetPort() const { return port; }
    const char *GetName() const { return name; }
    void SetName(const char *);
    const char *GetModel() const { return model; }
    void SetModel(const char *);
    bool GetPower() const { return power; }
    void SetPower(const bool ypower) { power = ypower; }
    bool isActive() const { return active; }
    void Activate() { active = true; }
    void Deactivate() { active = false; }
    int Flip(WiFiClient&) const;
    void printStatusHTML() const;              // Print bulb status in HTML
    void printConfHTML(uint8_t) const;         // Print bulb configuration controls in HTML
    bool operator==(const char *id2) const {
      return !strcmp(id, id2);
    }
};

// Yeelight discovery
class YDiscovery {

  protected:

    WiFiUDP udp;                                 // UDP socket used for discovery process
    unsigned long t0;                            // Discovery start time
    unsigned long t1;                            // Discovery time in progress

  public:

    static const unsigned long TIMEOUT = 3000;   // Discovery timeout (ms)

    YDiscovery();                                // Constructor
    bool send();                                 // Send discovery request
    YBulb *receive();                            // Receive discovery reply
    bool isInProgress();                         // True if discovery process is in progress
};

#endif // _YEELIGHT_H_
