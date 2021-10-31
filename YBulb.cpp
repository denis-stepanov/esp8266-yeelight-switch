/* Yeelight Smart Switch App for ESP8266
 * Yeelight bulb implementation
 * (c) DNS 2018-2021
 */

#include "YBulb.h"                         // Yeelight support
#include "MySystem.h"                      // System-level definitions

using namespace ds;

// Yeelight protocol; see https://www.yeelight.com/en_US/developer
const char *YL_MSG_TOGGLE = "{\"id\":1,\"method\":\"toggle\",\"params\":[]}\r\n";
const char *YL_MSG_DISCOVER = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1982\r\nMAN: \"ssdp:discover\"\r\nST: wifi_bulb";

/////////////////////// YBulb ///////////////////////

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

// Print bulb status in HTML
// TODO: synergy between two functions?
void YBulb::printStatusHTML() const {
  System::web_page += "<li>id(";
  System::web_page += id;
  System::web_page += "), ip(";
  System::web_page += ip;
  System::web_page += "), name(";
  System::web_page += name;
  System::web_page += "), model(";
  System::web_page += model;
  System::web_page += ")</li>";
}

// Print bulb configuration controls in HTML
void YBulb::printConfHTML(uint8_t num) const {
  System::web_page += "<input type=\"checkbox\" name=\"bulb\" value=\"";
  System::web_page += num;
  System::web_page += "\"";
  if (active)
    System::web_page += " checked";
  System::web_page += "/> ";
  System::web_page += ip;
  System::web_page += " id(";
  System::web_page += id;
  System::web_page += ") name(";
  System::web_page += name;
  System::web_page += ") model(";
  System::web_page += model;
  System::web_page += ") power(";
  System::web_page += power ? "<b>on</b>" : "off";
  System::web_page += ")<br/>";
}


//////////////////// YDiscovery /////////////////////

// Constructor
YDiscovery::YDiscovery() {
  t0 = millis();
  t1 = t0;
}

// Send discovery request
bool YDiscovery::send() {
  const IPAddress upnp_ip(239, 255, 255, 250);     // Yeelight is using a flavor of UPnP protocol
  const uint16_t upnp_port_yeelight = 1982;        // but the port is different from standard
  const int upnp_ttl = 32;                         // TTL
  auto ret = true;                                 // Return code

  // Send broadcast message
  udp.stop();
  ret = udp.beginPacketMulticast(upnp_ip, upnp_port_yeelight, WiFi.localIP(), upnp_ttl);
  if (ret) {
    ret = udp.write(YL_MSG_DISCOVER, strlen(YL_MSG_DISCOVER));
    if (ret) {
      ret = udp.endPacket();
      if (ret) {

        // Switch to listening for the replies on the same port
        const auto udp_port = udp.localPort();
        udp.stop();
        ret = udp.begin(udp_port);
      }
    }
  }
  return ret;
}

// Receive discovery reply
YBulb *YDiscovery::receive() {
  const unsigned int MAX_DISCOVERY_REPLY_SIZE = 512; // With contemporary bulbs, the reply is about 500 bytes
  char discovery_reply[MAX_DISCOVERY_REPLY_SIZE];    // Buffer to hold one discovery reply
  YBulb *new_bulb = nullptr;

  while (isInProgress()) {
    int len = udp.parsePacket();
    if (len > 0) {
      System::log->printf(TIMED("Received %d bytes from %s, port %d\n"), len, udp.remoteIP().toString().c_str(), udp.remotePort());
      len = udp.read(discovery_reply, sizeof(discovery_reply));
      if (len > 0) {
        discovery_reply[len] = 0;

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
            host = strtok(hostport, ":");
            port = strtok(nullptr, ":");
            if (host && port) {
              new_bulb = new YBulb(token, host, atoi(port));
            } else
              System::log->printf(TIMED("Bad address; ignoring 1 bulb\n"));
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
  }
  return new_bulb;
}

// True if discovery process is in progress
bool YDiscovery::isInProgress() {

  yield();  // Discovery process is lengthy; allow for background processing
  t1 = millis();
  return t1 - t0 < TIMEOUT;
}
