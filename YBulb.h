/* Yeelight Smart Switch App for ESP8266
 * Yeelight bulb definitions
 * (c) DNS 2018-2021
 */

#ifndef _YBULB_H_
#define _YBULB_H_

#include <WiFiClient.h>

// Yeelight bulb object. TODO: make a true library out of this
class YBulb {
    char id[24];
    char ip[16];
    uint16_t port;
    char name[32];
    char model[16];
    bool power;
    bool active;

  public:
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
    bool operator==(const char *id2) const {
      return !strcmp(id, id2);
    }
};

// Temporarily #define this, as with templated EEPROM.put() it does not work to define constant in one file and use it in another
#define YL_ID_LENGTH 18U            // Length of Yeelight device ID (chars)

#endif // _YBULB_H_
