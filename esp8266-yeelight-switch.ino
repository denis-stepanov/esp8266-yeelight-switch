/* Yeelight Smart Switch App for ESP8266
 * Developed & tested with a Witty Cloud Development board
 * (c) DNS 2018-2019
 *
 * Usage:
 * 0) review the configuration settings below; compile and flash your ESP8266
 * 1) boot with the push button pressed, connect your computer to the Wi-Fi network "ybutton1", enter and save your network credentials
 * 2) in your network, go to http://ybutton1.local, run the Yeelight scan and link the switch to the bulb found
 * 3) use the push button to control your bulb
 */

#include <WiFiUdp.h>
#include <WiFiManager.h>      // https://github.com/tzapu/WiFiManager
#include <EEPROM.h>
#include <ESP8266WiFi.h>      // https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <AceButton.h>        // https://github.com/bxparks/AceButton

using namespace ace_button;

// Configuration
const char *HOSTNAME = "ybutton1";        // <hostname>.local of the button in the local network. Also SSID of the temporary network for Wi-Fi configuration
const char *WIFICONFIGPASS = "Yeelight";  // Password used to connect to the temporary network for Wi-Fi configuration
const int PUSHBUTTON = D2;                // MCU pin connected to the main push button (D2 for Witty Cloud). The code below assumes the button is pulled high (HIGH == OFF)
const int BUILTINLED = D4;                // MCU pin connected to the built-in LED (D4 for Witty Cloud). The code below assumes the LED is pulled high (HIGH == OFF)

// Normally no need to change below this line
const char *APPNAME = "ESP8266 Yeelight Switch";
const char *APPVERSION = "1.1.1";
const char *APPURL = "https://github.com/denis-stepanov/esp8266-yeelight-switch";
const unsigned int BAUDRATE = 115200;     // Serial connection speed

// Yeelight protocol; see https://www.yeelight.com/en_US/developer
const char *YL_MSG_TOGGLE PROGMEM = "{\"id\":1,\"method\":\"toggle\",\"params\":[]}\r\n";
const char *YL_MSG_DISCOVER PROGMEM = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1982\r\nMAN: \"ssdp:discover\"\r\nST: wifi_bulb";
#define YL_ID_LENGTH 18

// Yeelight bulb object. TODO: make a library out of this
class YBulb {
    char id[24];
    char ip[16];
    uint16_t port;
    char name[32];
    char model[16];
    bool power;

  public:
    YBulb(char *, char *, uint16_t);
    ~YBulb(){};
    char *GetID() { return id; }
    char *GetIP() { return ip; }
    uint16_t GetPort() { return port; }
    char *GetName() { return name; }
    void SetName(char *);
    char *GetModel() { return model; }
    void SetModel(char *);
    bool GetPower() {return power; }
    void SetPower(bool ypower) {power = ypower; }
};

// To create a bulb, pass its ID and IP-address
YBulb::YBulb(char *yid, char *yip, uint16_t yport = 55443) {
  memset(id, 0, sizeof(id));
  if (yid)
    strncpy(id, yid, sizeof(id) - 1);
  memset(ip, 0, sizeof(ip));
  if (yip)
    strncpy(ip, yip, sizeof(ip) - 1);
  port = yport;
  memset(name, 0, sizeof(name));
  memset(model, 0, sizeof(model));
  power = false;
}

void YBulb::SetName(char *yname) {
  if (yname) {
    memset(name, 0, sizeof(name));
    strncpy(name, yname, sizeof(name) - 1);
  }
}

void YBulb::SetModel(char *ymodel) {
  if (ymodel) {
    memset(model, 0, sizeof(model));
    strncpy(model, ymodel, sizeof(model) - 1);
  }
}

// Global variables
AceButton button(PUSHBUTTON);
int builtinled_state = HIGH;

WiFiClient client;                  // Client used to talk to a bulb
WiFiUDP udp;                        // UDP socket used for discovery process
ESP8266WebServer server(80);        // Switch configuration web server

#define MAX_DISCOVERY_REPLY_SIZE 512 // With current bulbs, the reply is about 500 bytes
char discovery_reply[MAX_DISCOVERY_REPLY_SIZE]; // Buffer to hold one discovery reply
#define CONNECTION_TIMEOUT 1000     // Bulb connection timeout (ms)

#define MAX_BULBS 24                // Max number of bulbs which can be handled by the discovery process
#define MAX_ACTIVE_BULBS 8          // Max number of active bulbs
YBulb *bulbs[MAX_BULBS] = {NULL,};  // List of all known bulbs
unsigned char nbulbs = 0;           // Size of the list (<= MAX_BULBS)
YBulb *abulbs[MAX_ACTIVE_BULBS] = {NULL,}; // Pointers to the bulbs in use
unsigned char nabulbs = 0;          // Size of the list (<= MAX_ACTIVE_BULBS)

// Yeelight discovery. Note - no bulb removal at the moment
#define DISCOVERY_TIMEOUT 3000      // (ms)
void yl_discover(void) {
  IPAddress upnp_ip(239,255,255,250); // Yeelight is using a flavor of UPnP protocol
  uint16_t upnp_port_yeelight = 1982; // but the port is different from standard

  // Send broadcast message
  Serial.println("Sending Yeelight discovery request...");
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
      Serial.printf("Received %d bytes from %s, port %d\n", len, udp.remoteIP().toString().c_str(), udp.remotePort());
      len = udp.read(discovery_reply, sizeof(discovery_reply));
      if (len > 0) {
        discovery_reply[len] = 0;

        YBulb *bulb = NULL;
        char *line_ctx, *host = NULL, *port = NULL;
        char *token = strtok_r(discovery_reply, "\r\n", &line_ctx);
        while (token) {
          char hostport[24];

          if (!strncmp(token, "Location: ", 10)) {
            if (strtok(token, "/")) {
              memset(hostport, 0, sizeof(hostport));
              strncpy(hostport, strtok(NULL, "/"), sizeof(hostport) - 1);
            }
          } else if (!strncmp(token, "id: ", 4)) {
            strtok(token, " ");
            token = strtok(NULL, " ");

            // Check if we already have this bulb in the list
            bool bulb_exists = false;
            for (unsigned char i = 0; i < nbulbs; i++)
              if (!strcmp(token, bulbs[i]->GetID())) {
                bulb_exists = true;
                bulb = bulbs[i];
                break;
            }
            if (bulb_exists)
              Serial.println("Bulb already registered; ignoring");
            else {

              // Register
              host = strtok(hostport, ":");
              port = strtok(NULL, ":");
              if (nbulbs < MAX_BULBS && host && port) {
                bulb = new YBulb(token, host, atoi(port));
                bulbs[nbulbs++] = bulb;
                Serial.printf("Registered bulb %s from %s\n", bulb->GetID(), bulb->GetIP());
              } else
                Serial.println("Reached supported bulbs limit; ignoring 1 bulb");
            }
          } else if (!strncmp(token, "model: ", 7)) {
            if (strtok(token, " ") && bulb) {
              bulb->SetModel(strtok(NULL, " "));
              Serial.printf("Bulb model: %s\n", bulb->GetModel());
            }
          } else if (!strncmp(token, "name: ", 6)) {
            if (strtok(token, " ") && bulb) {
              bulb->SetName(strtok(NULL, " "));
              Serial.printf("Bulb name: %s\n", bulb->GetName());   // Currently, Yeelights always seem to return an empty name here :(
            }
          } else if (!strncmp(token, "power: ", 7)) {
            if (strtok(token, " ") && bulb) {
              bulb->SetPower(strcmp(strtok(NULL, " "), "off"));
              Serial.printf("Bulb power: %s\n", bulb->GetPower() ? "on" : "off");
            }
          } 
          token = strtok_r(NULL, "\r\n", &line_ctx);
        }
      }    
    }
    yield();  // The loop is lengthy; allow for background processing
  }
  Serial.printf("Total bulbs discovered: %d\n", nbulbs);
}

// Yeelight bulb flip
int yl_flip(void) {
  int ret = 0;
  if (nabulbs) {
    for (unsigned char i = 0; i < nabulbs; i++)
      if (client.connect(abulbs[i]->GetIP(), abulbs[i]->GetPort())) {
        client.print(YL_MSG_TOGGLE);
        client.stop();
        Serial.printf("Bulb %d toggle sent\n", i + 1);
      } else {
        Serial.printf("Bulb connection to %s failed\n", abulbs[i]->GetIP());
        ret = -2;
        yield();
      }
  } else {
    Serial.println("No linked bulbs found");
    ret = -1;
  }
  return ret;
}

// Button handler
#define BLINK_DELAY 100     // (ms)
void handleButtonEvent(AceButton*, uint8_t eventType, uint8_t /* buttonState */) {

  if (eventType == AceButton::kEventPressed) {

    // LED diagnostics:
    // 1 blink  - light flip OK
    // 1 + 2 blinks - one of the bulbs did not respond
    // 2 blinks - button not linked to bulbs
    // 1 long blink - Wi-Fi disconnected
    Serial.println("Button clicked");
    if (WiFi.status() != WL_CONNECTED) {

      // No Wi-Fi
      Serial.println("No Wi-Fi connection");
      digitalWrite(BUILTINLED, LOW);
      delay(BLINK_DELAY * 10);          // Long blink
      digitalWrite(BUILTINLED, HIGH);
    } else {
      if (nabulbs) {

        digitalWrite(BUILTINLED, LOW);
        delay(BLINK_DELAY);
        digitalWrite(BUILTINLED, HIGH);

        if (yl_flip()) {

          // Some bulbs did not respond
          // Because of connection timeout, the blinking will be 1 + pause + 2
          for (unsigned int i = 0; i < 2; i++) {
            delay(BLINK_DELAY * 2);
            digitalWrite(BUILTINLED, LOW);
            delay(BLINK_DELAY);
            digitalWrite(BUILTINLED, HIGH);
          }
        }
      } else {

        // Button not linked
        Serial.println("Button not linked to bulbs");
        digitalWrite(BUILTINLED, LOW);
        delay(BLINK_DELAY);
        digitalWrite(BUILTINLED, HIGH);
        delay(BLINK_DELAY * 2);
        digitalWrite(BUILTINLED, LOW);
        delay(BLINK_DELAY);
        digitalWrite(BUILTINLED, HIGH);
      }
    }
  }
}

// Web server configuration pages
// Root page. Show status
void handleRoot() {
  String page = "<html><head><title>";
  page += HOSTNAME;
  page += "</title></head><body><h3>Yeelight Button</h3>";
  if (nabulbs) {
    page += "<p>Linked to the bulb";
    if (nabulbs > 1)
      page += "s";
    page += ":</p><p><ul>";
    for (unsigned char i = 0; i < nabulbs; i++) {
      page += "<li>id(";
      page += abulbs[i]->GetID();
      page += "), ip(";
      page += abulbs[i]->GetIP();
      page += "), name(";
      page += abulbs[i]->GetName();
      page += "), model(";
      page += abulbs[i]->GetModel();
      page += ")</li>";
    }
    page += "</ul></p>";
  } else
    page += "<p>Not linked to a bulb</p>";
  page += "<p>[<a href=\"conf\">Change</a>]";
  if (nabulbs)
    page += " [<a href=\"flip\">Flip</a>]";
  page += "</p><hr/><p><i>Connected to network ";
  page += WiFi.SSID();
  page += ", hostname ";
  page += HOSTNAME;
  page += ".local (";
  page += WiFi.localIP().toString();
  page += "), RSSI ";
  page += WiFi.RSSI();
  page += " dBm.</i></p>";
  page += "<p><small>";
  page += APPNAME;
  page += " v";
  page += APPVERSION;
  page += ", <a href=\"";
  page += APPURL;
  page += "\">";
  page += APPURL;
  page += "</a></small></p>";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

// Bulb discovery page
void handleConf() {
  String page = "<html><head><title>";
  page += HOSTNAME;
  page += " conf</title></head><body><h3>Yeelight Button Configuration</h3>";
  page += "<p>[<a href=\"/conf\">Rescan</a>] [<a href=\"/save\">Unlink</a>] [<a href=\"..\">Back</a>]</p>";
  page += "<p><i>Scanning ";
  page += WiFi.SSID();
  page += " for Yeelight devices...</i></p>";
  page += "<p><i>Hint: turn all bulbs off, except the desired ones, in order to identify them easily.</i></p>";

  // Use chunked transfer to show scan in progress. Works in Chrome, not so well in Firefox
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); 
  server.send(200, "text/html", page);
  yl_discover();
  page = "<p>Found ";
  page += nbulbs;
  page += " bulb";
  if (nbulbs != 1)
    page += "s";
  page +=". Select bulbs from the list below (";
  page += MAX_ACTIVE_BULBS;
  page += " max).</p>";
  page += "<form action=\"/save\"><p style=\"font-family: monospace;\">";
  for (unsigned char i = 0; i < nbulbs; i++) {
    page += "<input type=\"checkbox\" name=\"bulb\" value=\"";
    page += i;
    page += "\"";
    for (unsigned char j = 0; j < nabulbs; j++)
      if (abulbs[j] == bulbs[i])
        page += " checked";
    page += "/> ";
    page += bulbs[i]->GetIP();
    page += " id(";
    page += bulbs[i]->GetID();
    page += ") name(";
    page += bulbs[i]->GetName();
    page += ") model(";
    page += bulbs[i]->GetModel();
    page += ") power(";
    page += bulbs[i]->GetPower() ? "<b>on</b>" : "off";
    page += ")<br/>";
  }
  page += "</p><p><input type=\"submit\" value=\"Link\"/></p></form></body></html>";
  server.sendContent(page);
  server.sendContent("");
  server.client().stop();   // Terminate chunked transfer
}

// Configuration saving page
// EEPROM format:
//   0-1: 'YB' - Yeelight Bulb configuration marker
//     2: format version. Increment each time the format changes
//     3: number of stored bulbs
//  4-22: <selected bulb ID> (19 bytes, null-terminated)
//      : ...
#define USED_EEPROM_SIZE (2 + 1 + 1 + (YL_ID_LENGTH + 1) * MAX_ACTIVE_BULBS)
#define EEPROM_FORMAT_VERSION 49    // The first version of the format stored 1 bulb id right after the marker. ID stars with ASCII '0' == 48
void handleSave() {

  EEPROM.begin(USED_EEPROM_SIZE);
  int nargs = server.args();
  unsigned int eeprom_addr = 4;

  nabulbs = 0;
  memset(abulbs, 0, sizeof(abulbs));
  if(nargs) {
    if (nargs <= MAX_ACTIVE_BULBS) {
      for(int i = 0; i < nargs; i++) {
        unsigned char n = server.arg(i).c_str()[0] - '0';
        if (n < MAX_ACTIVE_BULBS && n < nbulbs && bulbs[n]) {
          char bulbid_c[YL_ID_LENGTH + 1] = {0,};
          strncpy(bulbid_c, bulbs[n]->GetID(), YL_ID_LENGTH);
          EEPROM.put(eeprom_addr, bulbid_c);
          eeprom_addr += sizeof(bulbid_c);
          abulbs[nabulbs++] = bulbs[n];
        } else
          Serial.printf("Bulb #%d does not exist\n", n);
      }

      if (nabulbs) {

        // Write the header
        EEPROM.write(0, 'Y');
        EEPROM.write(1, 'B');
        EEPROM.write(2, EEPROM_FORMAT_VERSION);
        EEPROM.write(3, nabulbs);
        Serial.printf("%d bulb%s stored in EEPROM, using %u out of %u byte(s) allocated\n", nabulbs, nabulbs == 1 ? "" : "s", eeprom_addr, USED_EEPROM_SIZE);
      } else
        Serial.println("No bulbs were stored in EEPROM");
    } else
      Serial.printf("Number of bulbs to store (%d) exceeds the supported limit of %d\n", nargs, MAX_ACTIVE_BULBS);
  } else {

    // Unlink all

    // Overwriting the EEPROM marker will effectively cause forgetting the settings
    EEPROM.write(0, 0);
    Serial.println("Bulbs unlinked from the switch");
  }

  // TODO: check for errors?
  EEPROM.commit();
  EEPROM.end();

  String page = "<html><head><title>";
  page += HOSTNAME;
  page += nabulbs || !nargs ? " saved" : " error";
  page += "</title></head><body><h3>Yeelight Button Configuration ";
  page += nabulbs || !nargs ?  "Saved" : " Error";
  page += "</h3>";
  if (nargs) {
    if (nabulbs) {
      page += "<p>";
      page += nabulbs;
      page += " bulb";
      if (nabulbs != 1)
        page += "s";
      page += " linked</p>";
    } else
      page += "<p>Too many bulbs passed</p>";
  } else
    page += "<p>Bulbs unlinked</p>";
  page += "<p>[<a href=\"..\">Back</a>]</p>";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

// Bulb flip page. Accessing this page immediately flips the light
void handleFlip() {
  yl_flip();

  String page = "<html><head><title>";
  page += HOSTNAME;
  page += " flip</title></head><body><h3>Yeelight Button Flip</h3>";
  page += nabulbs ? "<p>Light flipped</p>" : "<p>No linked bulbs found</p>";
  page += "<p>[<a href=\"/flip\">Flip</a>] [<a href=\"..\">Back</a>]</p>";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

// Program setup
#define WIFI_CONNECT_TIMEOUT 20000  // (ms)
void setup(void) {

  // Serial line  
  Serial.begin(BAUDRATE);
  Serial.println("");

  // I/O
  pinMode(PUSHBUTTON, INPUT);
  pinMode(BUILTINLED, OUTPUT);
  digitalWrite(BUILTINLED, builtinled_state);

  // If the push button is pressed on boot, offer Wi-Fi configuration
  if (button.isPressedRaw()) {
    Serial.println("Push button pressed on boot; going to Wi-Fi Manager");
    builtinled_state = LOW;
    digitalWrite(BUILTINLED, builtinled_state);

    WiFiManager wifiManager;
    wifiManager.startConfigPortal(HOSTNAME, WIFICONFIGPASS);
  }
  button.setEventHandler(handleButtonEvent);

  // Network
  builtinled_state = HIGH;
  digitalWrite(BUILTINLED, builtinled_state);
  WiFi.mode(WIFI_STA);      // Important to avoid starting with an access point
  WiFi.hostname(HOSTNAME);
  WiFi.begin();             // Connect using stored credentials
  Serial.print("Connecting to ");
  Serial.print(WiFi.SSID());
  unsigned long time0 = millis(), time1 = time0;
  while (WiFi.status() != WL_CONNECTED && time1 - time0 < WIFI_CONNECT_TIMEOUT) {
    delay(100);
    Serial.print(".");

    // Show connection progress with LED
    builtinled_state = builtinled_state == HIGH ? LOW : HIGH;
    digitalWrite(BUILTINLED, builtinled_state);

    time1 = millis();
  }
  Serial.println("");
  builtinled_state = HIGH;
  digitalWrite(BUILTINLED, builtinled_state);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected!");
    Serial.printf("IP address: %s, RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("Connection timeout");
  }

  // Run discovery
  yl_discover();

  // Load settings from EEPROM
  EEPROM.begin(USED_EEPROM_SIZE);
  if (EEPROM.read(0) == 'Y' && EEPROM.read(1) == 'B' && EEPROM.read(2) == EEPROM_FORMAT_VERSION) {
    char bulbid_c[YL_ID_LENGTH + 1] = {0,};
    unsigned int eeprom_addr = 4;
    unsigned char n = 0;
    n = EEPROM.read(3);
    Serial.printf("Found %d bulb%s configuration in EEPROM\n", n, n == 1 ? "" : "s");
    for (unsigned char i = 0; i < n; i++) {
      EEPROM.get(eeprom_addr, bulbid_c);
      eeprom_addr += sizeof(bulbid_c);

      for (unsigned char j = 0; j < nbulbs; j++)
        if (!strcmp(bulbid_c, bulbs[j]->GetID())) {
          abulbs[nabulbs++] = bulbs[j];
          break;
      }
    }

    if (nabulbs == n)
      Serial.printf("Successfully linked to %d bulb%s\n", nabulbs, nabulbs == 1 ? "" : "s");
    else
      Serial.printf("Linking completed with %d out of %d bulb%s skipped\n", n - nabulbs, n, n == 1 ? "" : "s");
  } else
    Serial.println("No bulb configuration found in EEPROM; need to link bulbs manually");
  EEPROM.end();

  // Kick off mDNS
  if (MDNS.begin(HOSTNAME))
    Serial.printf("mDNS responder started; address=%s.local\n", HOSTNAME);

  // Kick off the web server
  server.on("/", handleRoot);
  server.on("/conf", handleConf);
  server.on("/save", handleSave);
  server.on("/flip", handleFlip);
  server.begin();
  Serial.println("Web server started");

  // Reduce connection timeout for inactive bulbs
  client.setTimeout(CONNECTION_TIMEOUT);
}

// Program loop
void loop(void) {

  // Check the button state
  button.check();

  // Background processing
  server.handleClient();
  MDNS.update();
}
