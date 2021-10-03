/* Yeelight Smart Switch App for ESP8266
 * Developed & tested with a Witty Cloud Development board
 * (c) DNS 2018-2021
 *
 * To make logging work, set on compile: Flash Size: 4M (FS:1MB)
 *
 * Usage:
 * 0) review the configuration settings below; compile and flash your ESP8266
 * 1) boot with the push button pressed, connect your computer to the Wi-Fi network "ybutton1", enter and save your network credentials
 * 2) in your network, go to http://ybutton1.local, run the Yeelight scan and link the switch to the bulb found
 * 3) use the push button to control your bulb
 */

#include "MySystem.h"                            // System-level definitions

#include <WiFiUdp.h>
#include <EEPROM.h>
#include <AceButton.h>        // https://github.com/bxparks/AceButton
#include <LinkedList.h>       // https://github.com/ivanseidel/LinkedList

using namespace ds;

// Configuration
const char *System::hostname PROGMEM = "ybutton1";   // <hostname>.local in the local network. Also SSID of the temporary network for Wi-Fi configuration
const int PUSHBUTTON = D2;                // MCU pin connected to the main push button (D2 for Witty Cloud). The code below assumes the button is pulled high (HIGH == OFF)

// Normally no need to change below this line
const char *System::app_name    PROGMEM = "ESP8266 Yeelight Switch";
const char *System::app_version PROGMEM = "2.0.0-beta.3";
const char *System::app_url     PROGMEM = "https://github.com/denis-stepanov/esp8266-yeelight-switch";

using namespace ace_button;

// Yeelight protocol; see https://www.yeelight.com/en_US/developer
const char *YL_MSG_TOGGLE = "{\"id\":1,\"method\":\"toggle\",\"params\":[]}\r\n";
const char *YL_MSG_DISCOVER = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1982\r\nMAN: \"ssdp:discover\"\r\nST: wifi_bulb";

// To create a bulb, pass its ID and IP-address
YBulb::YBulb(const char *yid, const char *yip, const uint16_t yport = 55443) :
  port(yport), power(false), active(false) {
  memset(id, 0, sizeof(id));
  if (yid)
    strncpy(id, yid, sizeof(id) - 1);
  memset(ip, 0, sizeof(ip));
  if (yip)
    strncpy(ip, yip, sizeof(ip) - 1);
  memset(name, 0, sizeof(name));
  memset(model, 0, sizeof(model));
}

void YBulb::SetName(const char *yname) {
  if (yname) {
    memset(name, 0, sizeof(name));
    strncpy(name, yname, sizeof(name) - 1);
  }
}

void YBulb::SetModel(const char *ymodel) {
  if (ymodel) {
    memset(model, 0, sizeof(model));
    strncpy(model, ymodel, sizeof(model) - 1);
  }
}

int YBulb::Flip(WiFiClient &wfc) const {
  if (wfc.connect(ip, port)) {
    wfc.print(YL_MSG_TOGGLE);
    wfc.stop();
    return 0;
  } else
    return -1;
}

// Global variables
const unsigned long BLINK_DELAY = 100UL;    // (ms)
const unsigned long GLOW_DELAY = 1000UL;    // (ms)
AceButton button(PUSHBUTTON);
bool button_pressed = false;

WiFiClient client;                  // Client used to talk to a bulb
WiFiUDP udp;                        // UDP socket used for discovery process

const unsigned int MAX_DISCOVERY_REPLY_SIZE = 512U; // With current bulbs, the reply is about 500 bytes
char discovery_reply[MAX_DISCOVERY_REPLY_SIZE]; // Buffer to hold one discovery reply
const uint16_t CONNECTION_TIMEOUT = 1000U;  // Bulb connection timeout (ms)

LinkedList<YBulb *> bulbs;          // List of known bulbs
uint8_t nabulbs = 0;                // Number of active bulbs

extern const uint8_t EEPROM_FORMAT_VERSION = 49;  // The first version of the format stored 1 bulb id right after the marker. ID stars with ASCII '0' == 48

// Button handler
void handleButtonEvent(AceButton* /* button */, uint8_t eventType, uint8_t /* buttonState */) {
  button_pressed = eventType == AceButton::kEventPressed;
}

// Yeelight discovery. Note - no bulb removal at the moment
void yl_discover(void) {
  const unsigned long DISCOVERY_TIMEOUT = 3000UL;   // (ms)
  const IPAddress upnp_ip(239,255,255,250);         // Yeelight is using a flavor of UPnP protocol
  const uint16_t upnp_port_yeelight = 1982U;        // but the port is different from standard

  // Send broadcast message
  System::log->printf(TIMED("Sending Yeelight discovery request...\n"));
  udp.beginPacketMulticast(upnp_ip, upnp_port_yeelight, WiFi.localIP(), 32);
  udp.write(YL_MSG_DISCOVER, strlen(YL_MSG_DISCOVER));
  udp.endPacket();

  // Switch to listening for the replies on the same port
  uint16_t udp_port = udp.localPort();
  udp.stop();
  udp.begin(udp_port);
  unsigned long time0 = millis();
  while (1) {
    unsigned long time1 = millis();
    if (time1 < time0)
      time0 = time1;
    if (time1 - time0 >= DISCOVERY_TIMEOUT)
      break; 
    int len = udp.parsePacket();
    if (len > 0) {
      System::log->printf(TIMED("Received %d bytes from %s, port %d\n"), len, udp.remoteIP().toString().c_str(), udp.remotePort());
      len = udp.read(discovery_reply, sizeof(discovery_reply));
      if (len > 0) {
        discovery_reply[len] = 0;

        YBulb *new_bulb = nullptr;
        char *line_ctx, *host = nullptr, *port = nullptr;
        char *token = strtok_r(discovery_reply, "\r\n", &line_ctx);
        while (token) {
          char hostport[24];

          if (!strncmp(token, "Location: ", 10)) {
            if (strtok(token, "/")) {
              memset(hostport, 0, sizeof(hostport));
              strncpy(hostport, strtok(nullptr, "/"), sizeof(hostport) - 1);
            }
          } else if (!strncmp(token, "id: ", 4)) {
            strtok(token, " ");
            token = strtok(nullptr, " ");

            // Check if we already have this bulb in the list
            for (uint8_t i = 0; i < bulbs.size(); i++) {
              YBulb *bulb = bulbs.get(i);
              if (*bulb == token) {
                new_bulb = bulb;
                break;
              }
            }
            if (new_bulb)
              System::log->printf(TIMED("Bulb already registered; ignoring\n"));
            else {

              // Register
              host = strtok(hostport, ":");
              port = strtok(nullptr, ":");
              if (host && port) {
                new_bulb = new YBulb(token, host, atoi(port));
                bulbs.add(new_bulb);
                System::log->printf(TIMED("Registered bulb %s from %s\n"), new_bulb->GetID(), new_bulb->GetIP());
              } else
                System::log->printf(TIMED("Bad address; ignoring 1 bulb\n"));
            }
          } else if (!strncmp(token, "model: ", 7)) {
            if (strtok(token, " ") && new_bulb) {
              new_bulb->SetModel(strtok(nullptr, " "));
              System::log->printf(TIMED("Bulb model: %s\n"), new_bulb->GetModel());
            }
          } else if (!strncmp(token, "name: ", 6)) {
            if (strtok(token, " ") && new_bulb) {
              new_bulb->SetName(strtok(nullptr, " "));
              System::log->printf(TIMED("Bulb name: %s\n"), new_bulb->GetName());   // Currently, Yeelights always seem to return an empty name here :(
            }
          } else if (!strncmp(token, "power: ", 7)) {
            if (strtok(token, " ") && new_bulb) {
              new_bulb->SetPower(strcmp(strtok(nullptr, " "), "off"));
              System::log->printf(TIMED("Bulb power: %s\n"), new_bulb->GetPower() ? "on" : "off");
            }
          } 
          token = strtok_r(nullptr, "\r\n", &line_ctx);
        }
      }    
    }
    yield();  // The loop is lengthy; allow for background processing
  }
  System::log->printf(TIMED("Total bulbs discovered: %d\n"), bulbs.size());
}

// Yeelight bulb flip
int yl_flip(void) {
  int ret = 0;
  if (nabulbs) {
    for (uint8_t i = 0; i < bulbs.size(); i++) {
      YBulb *bulb = bulbs.get(i);
      if (bulb->isActive()) {
        if (bulb->Flip(client)) {
          System::log->printf(TIMED("Bulb connection to %s failed\n"), bulb->GetIP());
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

// Return number of active bulbs
uint8_t yl_nabulbs(void) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < bulbs.size(); i++)
    if (bulbs.get(i)->isActive())
      n++;
  return n;
}

// Program setup
void setup(void) {
  System::begin();

  // I/O
  pinMode(PUSHBUTTON, INPUT);

  // If the push button is pressed on boot, offer Wi-Fi configuration
  if (button.isPressedRaw())
    System::configureNetwork();
  button.setEventHandler(handleButtonEvent);

  // Run discovery
  yl_discover();

  // Load settings from EEPROM
  EEPROM.begin(4);
  if (EEPROM.read(0) == 'Y' && EEPROM.read(1) == 'B' && EEPROM.read(2) == EEPROM_FORMAT_VERSION) {
    char bulbid_c[YL_ID_LENGTH + 1] = {0,};
    const uint8_t n = EEPROM.read(3);
    System::log->printf(TIMED("Found %d bulb%s configuration in EEPROM\n"), n, n == 1 ? "" : "s");
    EEPROM.end();
    EEPROM.begin(2 + 1 + 1 + (YL_ID_LENGTH + 1) * n);
    unsigned int eeprom_addr = 4;
    for (uint8_t i = 0; i < n; i++) {
      EEPROM.get(eeprom_addr, bulbid_c);
      eeprom_addr += sizeof(bulbid_c);

      for (uint8_t j = 0; j < bulbs.size(); j++) {
        YBulb *bulb = bulbs.get(j);
        if (*bulb == bulbid_c) {
          bulb->Activate();
          break;
        }
      }
    }

    nabulbs = yl_nabulbs();
    if (nabulbs == n)
      System::log->printf(TIMED("Successfully linked to %d bulb%s\n"), nabulbs, nabulbs == 1 ? "" : "s");
    else
      System::log->printf(TIMED("Linking completed with %d out of %d bulb%s skipped\n"), n - nabulbs, n, n == 1 ? "" : "s");
  } else
    System::log->printf(TIMED("No bulb configuration found in EEPROM; need to link bulbs manually\n"));
  EEPROM.end();

  // Reduce connection timeout for inactive bulbs
  client.setTimeout(CONNECTION_TIMEOUT);
}

// Program loop
void loop(void) {

  // Check the button state
  if (button_pressed) {
    button_pressed = false;
    System::appLogWriteLn("Button pressed", true);

    // LED diagnostics:
    // 1 blink  - light flip OK
    // 1 + 2 blinks - one of the bulbs did not respond
    // 2 blinks - button not linked to bulbs
    // 1 glowing - Wi-Fi disconnected
    if (!System::networkIsConnected()) {

      // No Wi-Fi
      System::log->printf(TIMED("No Wi-Fi connection\n"));
      System::led.Breathe(GLOW_DELAY).Repeat(1);            // 1 glowing
    } else {
      if (nabulbs) {

        // Flipping may block, causing JLED-style blink not being properly processed. Hence, force sequential processing (first blink, then flip)
        // To make JLED working smoothly in this case, an asynchronous WiFiClient.connect() method would be needed
        // This is not included in ESP8266 Core (https://github.com/esp8266/Arduino/issues/922), but is available as a separate library (like ESPAsyncTCP)
        // Since, for this project, it is a minor issue (flip being sent to bulbs with 100 ms delay), we stay with blocking connect()
        System::led.On().Update();
        delay(BLINK_DELAY);       // 1 blink. Note that using delay() inside loop() may skew sysClock, as per AceTime documentation
        System::led.Off().Update();

        if (yl_flip())

          // Some bulbs did not respond
          // Because of connection timeout, the blinking will be 1 + pause + 2
          System::led.Blink(BLINK_DELAY, BLINK_DELAY * 2).Repeat(2);  // 2 blinks

      } else {

        // Button not linked
        System::log->printf(TIMED("Button not linked to bulbs\n"));
        System::led.Blink(BLINK_DELAY, BLINK_DELAY * 2).Repeat(2);    // 2 blinks
      }
    }
  }

  // Background processing
  System::update();
  button.check();
}
