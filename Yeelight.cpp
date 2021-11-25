/* Yeelight Smart Switch App for ESP8266
 * Yeelight bulb implementation
 * (c) DNS 2018-2021
 */

#include "Yeelight.h"                      // Yeelight support
#include <ESP8266WiFi.h>                   // Wi-Fi support

using namespace dsy;

// Yeelight protocol; see https://www.yeelight.com/en_US/developer

// Some black magic to avoid defining multicast address twice
#define _comma ,
#define _ssdp_multicast_addr(dot) 239 dot 255 dot 255 dot 250
#define _ssdp_multicast_addr_comma _ssdp_multicast_addr(_comma)
#define _ssdp_multicast_addr_str __XSTRING(_ssdp_multicast_addr(.))
#define _ssdp_port 1982
#define _ssdp_port_str __STRING(_ssdp_port)

static const char *YL_MSG_DISCOVER PROGMEM =
  "M-SEARCH * HTTP/1.1\r\n"
  "HOST: " _ssdp_multicast_addr_str ":" _ssdp_port_str "\r\n"
  "MAN: \"ssdp:discover\"\r\n"
  "ST: wifi_bulb";

static const char *YL_MSG_TOGGLE PROGMEM =
  "{\"id\":1,"
   "\"method\":\"toggle\","
   "\"params\":[]"
  "}\r\n";

/////////////////////// YBulb ///////////////////////

// Return shortened bulb ID
//// Experience shows that Yeelight IDs are long zero-padded strings, so we can save some space
String YBulb::getShortID() const {
  return id.substring(11);
}

// Turn the bulb on. Returns true on success
bool YBulb::turnOn(WiFiClient& wfc) {
  return power ? true /* already on */ : flip(wfc);
}

// Turn the bulb off. Returns true on success
bool YBulb::turnOff(WiFiClient& wfc) {
  return power ? flip(wfc) : true /* already off */;
}

// Toggle bulb power state. Returns true on success
bool YBulb::flip(WiFiClient& wfc) {
  if (wfc.connect(ip, port)) {
    wfc.print(FPSTR(YL_MSG_TOGGLE));
    wfc.stop();
    power = !power;
    return true;
  } else
    return false;
}

// Print bulb info in HTML
//// Name | ID (shortened) | IP Address | Model | Power
void YBulb::printHTML(String& str) const {
  str += F("<td>");
  str += name;
  str += F("</td><td>");
  str += getShortID();
  str += F("</td><td>");
  str += ip.toString();
  str += F("</td><td>");
  str += model;
  str += F("</td><td>");
  str += getPowerStr();
  str += F("</td>");
}

// Print bulb status in HTML
void YBulb::printStatusHTML(String& str) const {
  str += F("<tr>");
  printHTML(str);
  str += F("</tr>\n");
}

// Print bulb configuration controls in HTML
//// Same as status but prepended with a selector
void YBulb::printConfHTML(String& str, uint8_t num) const {
  str += F("<tr><td>");
  str += F("<input type=\"checkbox\" name=\"bulb\" value=\"");
  str += num;
  str += '"';
  if (active)
    str += F(" checked=\"checked\"");
  str += F("/></td>");
  printHTML(str);
  str += F("</tr>\n");
}


//////////////////// YDiscovery /////////////////////

const IPAddress YDiscovery::SSDP_MULTICAST_ADDR(_ssdp_multicast_addr_comma);
const uint16_t YDiscovery::SSDP_PORT = _ssdp_port;

// Send discovery request
bool YDiscovery::send() {
  const String discovery_msg(FPSTR(YL_MSG_DISCOVER)); // Preload the message from flash, as WiFiUDP cannot work with flash directly
  t0 = millis();
  auto ret = true;

  // Send broadcast message
  udp.stop();
  ret = udp.beginPacketMulticast(SSDP_MULTICAST_ADDR, SSDP_PORT, WiFi.localIP(), 32);
  if (ret) {
    ret = udp.write(discovery_msg.c_str(), discovery_msg.length());
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
  YBulb *new_bulb = nullptr;
  while (isInProgress() && !new_bulb) {
    if (!udp.parsePacket())
      continue;
    const auto len = udp.read(reply_buffer, sizeof(reply_buffer) - 1);
    if (len <= 0)
      continue;
    reply_buffer[len] = '\0';  // Null-terminate
    String reply(reply_buffer);
    IPAddress host;
    uint16_t port = 0;
    while (true) {
      const auto idx = reply.indexOf(F("\r\n"));
      if (idx == -1)
        break;
      auto line = reply.substring(0, idx);
      reply.remove(0, idx + 2);
      if (line.startsWith(F("Location: yeelight://"))) {
        line.remove(0, line.indexOf('/') + 2);
        host.fromString(line.substring(0, line.indexOf(':')));
        port = line.substring(line.indexOf(':') + 1).toInt();
      } else if (line.startsWith(F("id: "))) {
        const auto id = line.substring(4);
        if (!id.isEmpty() && host && port)
          new_bulb = new YBulb(id, host, port);
      } else if (line.startsWith(F("model: ")) && new_bulb)
        new_bulb->setModel(line.substring(7));
      else if (line.startsWith(F("name: ")) && new_bulb)
        new_bulb->setName(line.substring(6));  // Currently, Yeelights always seem to return an empty name here :(
      else if (line.startsWith(F("power: ")) && new_bulb)
        new_bulb->setPower(line.substring(7));
    }
  }
  return new_bulb;
}

// True if discovery process is in progress
bool YDiscovery::isInProgress() const {

  yield();  // Discovery process is lengthy; allow for background processing
  return millis() - t0 < TIMEOUT;
}

// Declare a singleton-like instance
YDiscovery YDISCOVERY;                    // Global discovery handler
