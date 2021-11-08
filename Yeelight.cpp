/* Yeelight Smart Switch App for ESP8266
 * Yeelight bulb implementation
 * (c) DNS 2018-2021
 */

#include "Yeelight.h"                      // Yeelight support
#include <ESP8266WiFi.h>                   // Wi-Fi support

// Yeelight protocol; see https://www.yeelight.com/en_US/developer
const char *YL_MSG_TOGGLE = "{\"id\":1,\"method\":\"toggle\",\"params\":[]}\r\n";
const char *YL_MSG_DISCOVER = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1982\r\nMAN: \"ssdp:discover\"\r\nST: wifi_bulb";

/////////////////////// YBulb ///////////////////////

// Toggle bulb power state
int YBulb::flip(WiFiClient &wfc) const {
  if (wfc.connect(ip, port)) {
    wfc.print(YL_MSG_TOGGLE);
    wfc.stop();
    return 0;
  } else
    return -1;
}

// Print bulb status in HTML
// TODO: synergy between two functions?
void YBulb::printStatusHTML(String& str) const {
  str += "<li>id(";
  str += id;
  str += "), ip(";
  str += ip;
  str += "), name(";
  str += name;
  str += "), model(";
  str += model;
  str += ")</li>";
}

// Print bulb configuration controls in HTML
void YBulb::printConfHTML(String& str, uint8_t num) const {
  str += "<input type=\"checkbox\" name=\"bulb\" value=\"";
  str += num;
  str += "\"";
  if (active)
    str += " checked";
  str += "/> ";
  str += ip;
  str += " id(";
  str += id;
  str += ") name(";
  str += name;
  str += ") model(";
  str += model;
  str += ") power(";
  str += power ? "<b>on</b>" : "off";
  str += ")<br/>";
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
  YBulb *new_bulb = nullptr;
  auto reply_buffer = new char[MAX_REPLY_SIZE + 1];
  if (!reply_buffer)
    return new_bulb;
  while (isInProgress() && !new_bulb) {
    if (!udp.parsePacket())
      continue;
    const auto len = udp.read(reply_buffer, MAX_REPLY_SIZE);
    if (len <= 0)
      continue;
    reply_buffer[len] = '\0';  // Null-terminate
    String reply(reply_buffer);
    String host;
    String port;
    while (true) {
      const auto idx = reply.indexOf("\r\n");
      if (idx == -1)
        break;
      auto line = reply.substring(0, idx);
      reply.remove(0, idx + 2);
      if (line.startsWith("Location: yeelight://")) {
        line.remove(0, line.indexOf('/') + 2);
        host = line.substring(0, line.indexOf(':'));
        port = line.substring(line.indexOf(':') + 1);
      } else if (line.startsWith("id: ")) {
        const String id = line.substring(4);
        if (id && host && port)
          new_bulb = new YBulb(id, host, port.toInt());
      } else if (line.startsWith("model: ") && new_bulb)
        new_bulb->setModel(line.substring(7));
      else if (line.startsWith("name: ") && new_bulb)
        new_bulb->setName(line.substring(6));  // Currently, Yeelights always seem to return an empty name here :(
      else if (line.startsWith("power: ") && new_bulb)
        new_bulb->setPower(line.substring(7) == "on");
    }
  }
  delete [] reply_buffer;
  return new_bulb;
}

// True if discovery process is in progress
bool YDiscovery::isInProgress() {

  yield();  // Discovery process is lengthy; allow for background processing
  t1 = millis();
  return t1 - t0 < TIMEOUT;
}
