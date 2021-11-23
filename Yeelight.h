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
    IPAddress ip;                                // IP-address of the bulb
    uint16_t port;                               // Port of the bulb
    String name;                                 // Bulb name
    String model;                                // Bulb model ("color", "stripe", etc)
    bool power;                                  // Current power state (true = "on")
    bool active;                                 // True if the bulb is actively controlled (e.g., linked to a switch)

    virtual void printHTML(String&) const;       // Print bulb info in HTML

  public:

    static const size_t ID_LENGTH = 18;          // Length of the Yeelight device ID (chars)
    static const uint16_t TIMEOUT = 1000;        // Bulb connection timeout (ms)

    YBulb(const String& yid, const IPAddress& yip = 0, const uint16_t yport = 55443) :
      id(yid), ip(yip), port(yport), power(false), active(false) {}  // Constructor
    virtual ~YBulb() {}                          // Destructor

    virtual const String& getID() const { return id; }   // Return bulb ID
    virtual String getShortID() const;                   // Return shortened bulb ID
    virtual const IPAddress& getIP() const { return ip; }            // Return bulb IP-address
    virtual void setIP(const IPAddress& yip) { ip = yip; }           // Set bulb IP-address
    virtual uint16_t getPort() const { return port; }    // Return bulb port
    virtual const String& getName() const { return name; }           // Return bulb name
    virtual void setName(const String& yname) { name = yname; }      // Set bulb name
    virtual const String& getModel() const { return model; }         // Return bulb model
    virtual void setModel(const String& ymodel) { model = ymodel; }  // Set bulb model
    virtual bool getPower() const { return power; }      // Return bulb power state (true = "on")
    virtual String getPowerStr() const { return power ? F("on") : F("off"); } // Return bulb power state as string
    virtual void setPower(bool new_power) { power = new_power; }     // Set bulb power state (true = "on")
    virtual void setPower(const String& new_power) { power = new_power == F("on"); } // Set bulb power state from string ("on" or "off")
    virtual bool isActive() const { return active; }     // True if bulb control is active
    virtual void activate() { active = true; }           // Activate bulb control
    virtual void deactivate() { active = false; }        // Deactivate bulb control
    virtual bool turnOn(WiFiClient&);                    // Turn the bulb on. Returns true on success
    virtual bool turnOff(WiFiClient&);                   // Turn the bulb off. Returns true on success
    virtual bool flip(WiFiClient&);                      // Toggle bulb power state. Returns true on success
    virtual void printStatusHTML(String&) const;         // Print bulb status in HTML
    virtual void printConfHTML(String&, uint8_t) const;  // Print bulb configuration controls in HTML
    virtual bool operator==(const String& id2) const {   // Bulb comparison
      return id == id2;
    }
    virtual bool operator==(const YBulb& yb) const {     // Bulb comparison
      return *this == yb.getID();
    }
};

// Yeelight discovery
class YDiscovery {

  protected:

    WiFiUDP udp;                                 // UDP socket used for discovery process
    unsigned long t0;                            // Discovery start time
    unsigned long t1;                            // Discovery time in progress

  public:

    static const IPAddress SSDP_MULTICAST_ADDR;  // Yeelight is using a flavor of SSDP protocol
    static const uint16_t SSDP_PORT;             // ... but the port is different from standard
    static const size_t SSDP_BUFFER_SIZE = 512;  // With contemporary bulbs, the reply is about 500 bytes
    static const unsigned long TIMEOUT = 3000;   // Discovery timeout (ms)

    YDiscovery();                                // Constructor
    virtual ~YDiscovery() {}                     // Destructor
    virtual bool send();                         // Send discovery request
    virtual YBulb *receive();                    // Receive discovery reply
    virtual bool isInProgress();                 // True if discovery process is in progress
};

#endif // _YEELIGHT_H_
