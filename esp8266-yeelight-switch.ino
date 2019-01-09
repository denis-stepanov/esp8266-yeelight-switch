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

// Configuration
const char *HOSTNAME = "ybutton1";        // <hostname>.local of the button in the local network. Also SSID of the temporary network for Wi-Fi configuration
const char *WIFICONFIGPASS = "Yeelight";  // Password used to connect to the temporary network for Wi-Fi configuration
const int PUSHBUTTON = D2;                // MCU pin connected to the main push button (D2 for Witty Cloud). The code below assumes the button is pulled high (HIGH == OFF)
const int BUILTINLED = D4;                // MCU pin connected to the built-in LED (D4 for Witty Cloud). The code below assumes the LED is pulled high (HIGH == OFF)

// Normally no need to change below this line
const char *APPNAME = "ESP8266 Yeelight Switch";
const char *APPVERSION = "1.0.0";
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
int button_state = HIGH, button_state_prev = HIGH;
int builtinled_state = HIGH;

WiFiClient client;                  // Client used to talk to a bulb
WiFiUDP udp;                        // UDP socket used for discovery process
ESP8266WebServer server(80);        // Switch configuration web server

#define MAX_DISCOVERY_REPLY_SIZE 512 // With current bulbs, the reply is about 500 bytes
char discovery_reply[MAX_DISCOVERY_REPLY_SIZE]; // Buffer to hold one discovery reply 

#define MAX_BULBS 24                // Max number of bulbs which can be handled by the discovery process
YBulb *bulbs[MAX_BULBS] = {NULL,};  // List of all known bulbs
unsigned short nbulbs = 0;          // Size of the list (<= MAX_BULBS)
YBulb *currbulb = NULL;             // Pointer to the bulb in use

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
            for (unsigned short i = 0; i < nbulbs; i++)
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
  Serial.printf("Total bulbs discovered: %hu\n", nbulbs);
}

// Yeelight bulb flip
int yl_flip(void) {
  if (currbulb) {
    if (client.connect(currbulb->GetIP(), currbulb->GetPort())) {
      client.print(YL_MSG_TOGGLE);
      client.stop();
      Serial.println("Bulb toggle sent");
    } else {
      Serial.printf("Bulb connection to %s failed\n", currbulb->GetIP());
      return -2;
    }
  } else {
    Serial.println("No linked bulb found");
    return -1;
  }
  return 0;
}

// Web server configuration pages
// Root page. Show status
void handleRoot() {
  String page = "<html><head><title>";
  page += HOSTNAME;
  page += "</title></head><body><h3>Yeelight Button</h3>";
  if (currbulb) {
    page += "<p>Linked to the bulb id(";
    page += currbulb->GetID();
    page += "), ip(";
    page += currbulb->GetIP();
    page += "), name(";
    page += currbulb->GetName();
    page += "), model(";
    page += currbulb->GetModel();
    page += ")</p>";
  } else
    page += "<p>Not linked to a bulb</p>";
  page += "<p>[<a href=\"conf\">Change</a>]";
  if (currbulb)
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
  page += "<p><i>Hint: turn all bulbs off, except the desired one, in order to identify it easily.</i></p>";

  // Use chunked transfer to show scan in progress. Works in Chrome, not so well in Firefox
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); 
  server.send(200, "text/html", page);
  yl_discover();
  page = "<p>Found ";
  page += nbulbs;
  page += " bulb(s). Select a bulb from the list below.</p>";
  page += "<form action=\"/save\">";
  for (int i = 0; i < nbulbs; i++) {
    page += "<p><input type=\"radio\" name=\"bulb\" value=\"";
    page += bulbs[i]->GetID();
    page += "\"";
    if (currbulb && !strcmp(currbulb->GetID(), bulbs[i]->GetID()))
      page += " checked";
    page += ">";
    page += bulbs[i]->GetIP();
    page += " id(";
    page += bulbs[i]->GetID();
    page += ") name(";
    page += bulbs[i]->GetName();
    page += ") model(";
    page += bulbs[i]->GetModel();
    page += ") power(";
    page += bulbs[i]->GetPower() ? "<b>on</b>" : "off";
    page += ")</p>";
  }
  page += "<p><input type=\"submit\" value=\"Link\"></p></form></body></html>";
  server.sendContent(page);
  server.sendContent("");
  server.client().stop();   // Terminate chunked transfer
}

// Configuration saving page
#define USED_EEPROM_SIZE 32
void handleSave() {
  String bulbid = server.arg("bulb");
  bool bulb_exists = false;
  EEPROM.begin(USED_EEPROM_SIZE);
  if (bulbid != "") {
    for (unsigned short i = 0; i < nbulbs; i++)
      if (bulbid == bulbs[i]->GetID()) {
        bulb_exists = true;
        currbulb = bulbs[i];
        break;
    }
    if (bulb_exists) {

      // Save to EEPROM. Format:
      //   0-1: 'YB' - Yeelight Bulb configuration marker
      //  2-20: <selected bulb ID> (null-terminated)
      // 21-31: <reserved>
      // TODO: check for errors?
      char bulbid_c[YL_ID_LENGTH + 1] = {0,};
      strncpy(bulbid_c, bulbid.c_str(), sizeof(bulbid_c) - 1);
      EEPROM.write(0, 'Y');
      EEPROM.write(1, 'B');
      EEPROM.put(2, bulbid_c);
      Serial.println("Saved new bulb configuration in EEPROM");
    } else
      Serial.println("Error saving new bulb configuration: the bulb does not exist");
  } else {
    currbulb = NULL;

    // Overwriting the EEPROM marker will effectively cause forgetting the settings
    EEPROM.write(0, 0);
    Serial.println("Bulb unlinked from the switch");
  }
  EEPROM.commit();
  EEPROM.end();

  String page = "<html><head><title>";
  page += HOSTNAME;
  page += bulbid == "" || bulb_exists ? " saved" : " error";
  page += "</title></head><body><h3>Yeelight Button Configuration ";
  page += bulbid == "" || bulb_exists ?  "Saved" : " Error";
  page += "</h3>";
  if (bulbid == "")
    page += "<p>Bulb unlinked</p>";
  else {
    if (!bulb_exists)
      page += "<p>The selected bulb does not exist</p>";
  }
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
  page += currbulb ? "<p>Bulb flipped</p>" : "<p>No linked bulb found</p>";
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
  button_state = digitalRead(PUSHBUTTON);
  if (button_state == LOW) {
    Serial.println("Push button pressed on boot; going to Wi-Fi Manager");
    builtinled_state = LOW;
    digitalWrite(BUILTINLED, builtinled_state);

    WiFiManager wifiManager;
    wifiManager.startConfigPortal(HOSTNAME, WIFICONFIGPASS);
  }

  // Network
  builtinled_state = HIGH;
  digitalWrite(BUILTINLED, builtinled_state);
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

  // Load settings from EEPROM
  char bulbid_c[YL_ID_LENGTH + 1] = {0,};
  EEPROM.begin(USED_EEPROM_SIZE);
  if (EEPROM.read(0) == 'Y' && EEPROM.read(1) == 'B') {
    EEPROM.get(2, bulbid_c);
    Serial.printf("Found bulb configuration in EEPROM: %s\n", bulbid_c);

    // Run discovery and hook up to the stored bulb, if found
    yl_discover();
    unsigned short i;
    for (i = 0; i < nbulbs; i++)
      if (!strcmp(bulbid_c, bulbs[i]->GetID())) {
        currbulb = bulbs[i];
        break;
    }
    if (i < nbulbs)
      Serial.printf("Previously saved bulb %s is online; linking to it\n", bulbid_c);
    else
      Serial.printf("Previously saved bulb %s is not online; skipping\n", bulbid_c);
  } else
    Serial.println("No bulb configuration found in EEPROM; need to link a bulb manually");
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
}

// Program loop
#define BLINK_DELAY 100     // (ms)
void loop(void) {
  
  button_state = digitalRead(PUSHBUTTON);
  if (button_state != button_state_prev) {

    // LED diagnostics:
    // 1 blink  - bulb flip OK
    // 1 + 2 blinks - the bulb did not respond
    // 2 blinks - button not linked to a bulb
    // 1 long blink - Wi-Fi disconnected
    if (button_state == LOW) {
      Serial.println("Button ON");
      if (WiFi.status() != WL_CONNECTED) {

        // No Wi-Fi
        Serial.println("No Wi-Fi connection");
        digitalWrite(BUILTINLED, LOW);
        delay(BLINK_DELAY * 10);          // Long blink
        digitalWrite(BUILTINLED, HIGH);
      } else {
        if (currbulb) {

          digitalWrite(BUILTINLED, LOW);
          delay(BLINK_DELAY);
          digitalWrite(BUILTINLED, HIGH);

          if (yl_flip()) {

            // Bulb did not respond
            // Because of connection timeout, the blinking will be 1 + pause + 2
            for (unsigned int i = 0; i < 2; i++) {
              delay(BLINK_DELAY);
              digitalWrite(BUILTINLED, LOW);
              delay(BLINK_DELAY);
              digitalWrite(BUILTINLED, HIGH);
            }
          }
        } else {

          // Button not linked
          Serial.println("Button not linked to a bulb");
          digitalWrite(BUILTINLED, LOW);
          delay(BLINK_DELAY);
          digitalWrite(BUILTINLED, HIGH);
          delay(BLINK_DELAY);
          digitalWrite(BUILTINLED, LOW);
          delay(BLINK_DELAY);
          digitalWrite(BUILTINLED, HIGH);
        }
      }
    } else
      Serial.println("Button OFF");

    button_state_prev = button_state;
  }

  // Process HTTP traffic
  server.handleClient();
}
