/* Yeelight Smart Switch App for ESP8266
 * Developed & tested with a Witty Cloud Development board
 * (c) DNS 2018-2019
 *
 * To make logging work, set on compile: Flash Size: 4M (1M SPIFFS)
 *
 * Usage:
 * 0) review the configuration settings below; compile and flash your ESP8266
 * 1) boot with the push button pressed, connect your computer to the Wi-Fi network "ybutton1", enter and save your network credentials
 * 2) in your network, go to http://ybutton1.local, run the Yeelight scan and link the switch to the bulb found
 * 3) use the push button to control your bulb
 */

#include <FS.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>      // https://github.com/tzapu/WiFiManager
#include <EEPROM.h>
#include <ESP8266WiFi.h>      // https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <jled.h>             // https://github.com/jandelgado/jled
#include <AceButton.h>        // https://github.com/bxparks/AceButton
#include <LinkedList.h>       // https://github.com/ivanseidel/LinkedList

using namespace ace_button;

// Configuration
const char *HOSTNAME = "ybutton1";        // <hostname>.local of the button in the local network. Also SSID of the temporary network for Wi-Fi configuration
const char *WIFICONFIGPASS = "Yeelight";  // Password used to connect to the temporary network for Wi-Fi configuration
const int PUSHBUTTON = D2;                // MCU pin connected to the main push button (D2 for Witty Cloud). The code below assumes the button is pulled high (HIGH == OFF)
const int BUILTINLED = D4;                // MCU pin connected to the built-in LED (D4 for Witty Cloud). The code below assumes the LED is pulled high (HIGH == OFF)

// Normally no need to change below this line
const char *APPNAME = "ESP8266 Yeelight Switch";
const char *APPVERSION = "2.0beta";
const char *APPURL = "https://github.com/denis-stepanov/esp8266-yeelight-switch";
const unsigned int BAUDRATE = 115200;     // Serial connection speed

// Yeelight protocol; see https://www.yeelight.com/en_US/developer
const char *YL_MSG_TOGGLE = "{\"id\":1,\"method\":\"toggle\",\"params\":[]}\r\n";
const char *YL_MSG_DISCOVER = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1982\r\nMAN: \"ssdp:discover\"\r\nST: wifi_bulb";
const unsigned int YL_ID_LENGTH = 18U;

// Yeelight bulb object. TODO: make a library out of this
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

// Logger class. TODO: make a library out of this
const char *LOGFILENAME = "/log.txt";
const char *LOGFILENAME2 = "/log2.txt";

//// Logs tend to fill up the drive. SPIFFS manual recommends to always keep some space available,
//// plus, logger will usually overshoot max log size by a few bytes. So reserve some free space
const unsigned int LOGSLACK = 2048U;
const size_t LOGSIZEMAX = 1048576U; // For large file systems, hard-limit log size. It is not likely that more than 2MiB of logs will be needed
class Logger {
    File logFile;
    bool enabled;
    size_t logSize;
    size_t logSizeMax;
  public:
    Logger();
    ~Logger();
    bool isEnabled() const { return enabled; };
    void writeln(const char *);
    void writeln(const String &);
    void rotate();
};

//// Initialize logger
Logger::Logger() {
  enabled = SPIFFS.begin();
  if (enabled) {
    FSInfo fsi;
    SPIFFS.info(fsi);
    if (fsi.totalBytes > LOGSLACK)
      logSizeMax = (fsi.totalBytes - LOGSLACK) / 2 < LOGSIZEMAX ? (fsi.totalBytes - LOGSLACK) / 2 : LOGSIZEMAX;
    else {
      enabled = false;
      logSize = 0;
      logSizeMax = 0;
    }
    logFile = SPIFFS.open(LOGFILENAME, "a");
    if (!logFile) {
      SPIFFS.end();
      enabled = false;
      logSize = 0;
      logSizeMax = 0;
    } else
      logSize = logFile.size();
  }
}

//// Terminate logger
Logger::~Logger() {
  logFile.close();
  SPIFFS.end();
}

//// Write a line to a log
void Logger::writeln(const char *line) {
  writeln((String)line);
}

//// Write a line to a log
void Logger::writeln(const String &line) {
  if (enabled) {
    const String timestamp = "--:--:-- --/--/---- "; // TODO NTP
    String msg = timestamp + line;
    logFile.println(msg);
    logFile.flush();
    logSize += msg.length();
  }
}

//// Check log size and rotate if needed
void Logger::rotate() {
  if (enabled && logSize >= logSizeMax) {
    Serial.printf("Max log size (%u) reached, rotating...\n", logSizeMax);
    logFile.close();
    SPIFFS.remove(LOGFILENAME2);          // Rename will fail if file exists
    SPIFFS.rename(LOGFILENAME, LOGFILENAME2);
    logSize = 0;
    logFile = SPIFFS.open(LOGFILENAME, "a");
    if(!logFile) {
      enabled = false;
      Serial.println("Log rotation failed");
    }
  }
}

// Global variables
JLed led(BUILTINLED);
const unsigned long BLINK_DELAY = 100UL;    // (ms)
const unsigned long GLOW_DELAY = 1000UL;    // (ms)
AceButton button(PUSHBUTTON);
bool button_pressed = false;

WiFiClient client;                  // Client used to talk to a bulb
WiFiUDP udp;                        // UDP socket used for discovery process
ESP8266WebServer server;            // Switch configuration web server

const unsigned int MAX_DISCOVERY_REPLY_SIZE = 512U; // With current bulbs, the reply is about 500 bytes
char discovery_reply[MAX_DISCOVERY_REPLY_SIZE]; // Buffer to hold one discovery reply
const uint16_t CONNECTION_TIMEOUT = 1000U;  // Bulb connection timeout (ms)

LinkedList<YBulb *> bulbs;          // List of known bulbs
uint8_t nabulbs = 0;                // Number of active bulbs

Logger logger;

const char *COMPILATION_TIMESTAMP = __DATE__ " " __TIME__;

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
              Serial.println("Bulb already registered; ignoring");
            else {

              // Register
              host = strtok(hostport, ":");
              port = strtok(nullptr, ":");
              if (host && port) {
                new_bulb = new YBulb(token, host, atoi(port));
                bulbs.add(new_bulb);
                Serial.printf("Registered bulb %s from %s\n", new_bulb->GetID(), new_bulb->GetIP());
              } else
                Serial.println("Bad address; ignoring 1 bulb");
            }
          } else if (!strncmp(token, "model: ", 7)) {
            if (strtok(token, " ") && new_bulb) {
              new_bulb->SetModel(strtok(nullptr, " "));
              Serial.printf("Bulb model: %s\n", new_bulb->GetModel());
            }
          } else if (!strncmp(token, "name: ", 6)) {
            if (strtok(token, " ") && new_bulb) {
              new_bulb->SetName(strtok(nullptr, " "));
              Serial.printf("Bulb name: %s\n", new_bulb->GetName());   // Currently, Yeelights always seem to return an empty name here :(
            }
          } else if (!strncmp(token, "power: ", 7)) {
            if (strtok(token, " ") && new_bulb) {
              new_bulb->SetPower(strcmp(strtok(nullptr, " "), "off"));
              Serial.printf("Bulb power: %s\n", new_bulb->GetPower() ? "on" : "off");
            }
          } 
          token = strtok_r(nullptr, "\r\n", &line_ctx);
        }
      }    
    }
    yield();  // The loop is lengthy; allow for background processing
  }
  Serial.printf("Total bulbs discovered: %d\n", bulbs.size());
}

// Yeelight bulb flip
int yl_flip(void) {
  int ret = 0;
  if (nabulbs) {
    for (uint8_t i = 0; i < bulbs.size(); i++) {
      YBulb *bulb = bulbs.get(i);
      if (bulb->isActive()) {
        if (bulb->Flip(client)) {
          Serial.printf("Bulb connection to %s failed\n", bulb->GetIP());
          ret = -2;
          yield();        // Connection timeout is lenghty; allow for background processing (is this really needed?)
        } else
          Serial.printf("Bulb %d toggle sent\n", i + 1);
      }
    }
  } else {
    Serial.println("No linked bulbs found");
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
    for (uint8_t i = 0; i < bulbs.size(); i++) {
      YBulb *bulb = bulbs.get(i);
      if (bulb->isActive()) {
        page += "<li>id(";
        page += bulb->GetID();
        page += "), ip(";
        page += bulb->GetIP();
        page += "), name(";
        page += bulb->GetName();
        page += "), model(";
        page += bulb->GetModel();
        page += ")</li>";
      }
    }
    page += "</ul></p>";
  } else
    page += "<p>Not linked to a bulb</p>";
  page += "<p>[<a href=\"conf\">Change</a>]";
  if (nabulbs)
    page += " [<a href=\"flip\">Flip</a>]";
  page += " [<a href=\"log\">Log</a>]";
  page += "</p><hr/><p><i>Connected to network ";
  page += WiFi.SSID();
  page += ", hostname ";
  page += HOSTNAME;
  page += ".local (";
  page += WiFi.localIP().toString();
  page += "), RSSI ";
  page += WiFi.RSSI();
  page += " dBm.</i></p>";
  page += "<p><small><a href=\"";
  page += APPURL;
  page += "\">";
  page += APPNAME;
  page += "</a> v";
  page += APPVERSION;
  page += " build ";
  page += COMPILATION_TIMESTAMP;
  page += "</small></p>";
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
  page += bulbs.size();
  page += " bulb";
  if (bulbs.size() != 1)
    page += "s";
  page +=". Select bulbs to link from the list below.</p>";
  page += "<form action=\"/save\"><p style=\"font-family: monospace;\">";
  for (uint8_t i = 0; i < bulbs.size(); i++) {
    YBulb *bulb = bulbs.get(i);
    page += "<input type=\"checkbox\" name=\"bulb\" value=\"";
    page += i;
    page += "\"";
    if (bulb->isActive())
      page += " checked";
    page += "/> ";
    page += bulb->GetIP();
    page += " id(";
    page += bulb->GetID();
    page += ") name(";
    page += bulb->GetName();
    page += ") model(";
    page += bulb->GetModel();
    page += ") power(";
    page += bulb->GetPower() ? "<b>on</b>" : "off";
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
const uint8_t EEPROM_FORMAT_VERSION = 49U;  // The first version of the format stored 1 bulb id right after the marker. ID stars with ASCII '0' == 48
void handleSave() {

  unsigned int nargs = server.args();
  const size_t used_eeprom_size = 2 + 1 + 1 + (YL_ID_LENGTH + 1) * nargs;   // TODO: maybe put some constraint on nargs (externally provided parameter)
  EEPROM.begin(used_eeprom_size);
  unsigned int eeprom_addr = 4;

  for (uint8_t i = 0; i < bulbs.size(); i++)
    bulbs.get(i)->Deactivate();
  nabulbs = 0;

  if(nargs) {
    for(unsigned int i = 0; i < nargs; i++) {
      unsigned char n = server.arg(i).c_str()[0] - '0';
      if (n < bulbs.size()) {
        YBulb *bulb = bulbs.get(n);
        char bulbid_c[YL_ID_LENGTH + 1] = {0,};
        strncpy(bulbid_c, bulb->GetID(), YL_ID_LENGTH);
        EEPROM.put(eeprom_addr, bulbid_c);
        eeprom_addr += sizeof(bulbid_c);
        bulb->Activate();
      } else
        Serial.printf("Bulb #%d does not exist\n", n);
    }

    nabulbs = yl_nabulbs();
    if (nabulbs) {

      // Write the header
      EEPROM.write(0, 'Y');
      EEPROM.write(1, 'B');
      EEPROM.write(2, EEPROM_FORMAT_VERSION);
      EEPROM.write(3, nabulbs);
      Serial.printf("%d bulb%s stored in EEPROM, using %u byte(s)\n", nabulbs, nabulbs == 1 ? "" : "s", eeprom_addr);
    } else
      Serial.println("No bulbs were stored in EEPROM");
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
  logger.writeln("Web page flip received");

  String page = "<html><head><title>";
  page += HOSTNAME;
  page += " flip</title></head><body><h3>Yeelight Button Flip</h3>";
  page += nabulbs ? "<p>Light flipped</p>" : "<p>No linked bulbs found</p>";
  page += "<p>[<a href=\"/flip\">Flip</a>] [<a href=\"..\">Back</a>]</p>";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

// Display log
const unsigned long LOG_PAGE_SIZE = 2048UL;  // (bytes)
void handleLog() {
  Serial.println("Displaying log");

  // Parse query params
  unsigned int logPage = 0;
  String logFileName(LOGFILENAME);
  String logFileParam;
  for (int i = 0; i < server.args(); i++) {
    String argname = server.argName(i);
    if (argname == "p")
      logPage = String(server.arg(i)).toInt();
    else {
      if (argname == "r") {
        logFileName = LOGFILENAME2;
        logFileParam = "r=1&";
      }
    }
  }

  String page = "<html><head><title>";
  page += HOSTNAME;
  page += " event log</title></head><body><h3>Yeelight Button Event Log</h3>[<a href=\"/\">home</a>] ";
  if (logger.isEnabled()) {
    File logFile = SPIFFS.open(logFileName, "r");
    if (!logFile)
      page += "<span><br/>File opening error";
    else {
      const uint32_t fsize = logFile.size();

      // Print pagination buttons
      if (fsize > (logPage + 1) * LOG_PAGE_SIZE) {
        page += "[<a href=\"/log?";
        page += logFileParam;
        page += "p=";
        page += logPage + 1;
        page += "\">&lt;&lt;</a>]\n";
      } else {
        if (logFileName == LOGFILENAME && SPIFFS.exists(LOGFILENAME2))
          page += "[<a href=\"/log?r=1\">&lt;&lt;</a>]\n";
        else
          page += "[&lt;&lt;]\n";
      }
      if (logPage) {
        page += "[<a href=\"/log?";
        page += logFileParam;
        page += "p=";
        page += logPage - 1;
        page += "\">&gt;&gt;</a>]\n";
      } else {
        if (logFileName == LOGFILENAME2 && SPIFFS.exists(LOGFILENAME)) {
          File logFileNext = SPIFFS.open(LOGFILENAME, "r");
          const unsigned int logPageNext = logFileNext.size() / LOG_PAGE_SIZE;
          logFileNext.close();
          page += "[<a href=\"/log?p=";
          page += logPageNext;
          page += "\">&gt;&gt;</a>]\n";
        } else
          page += "[&gt;&gt;]\n";
      }

      // Print log fragment
      page += "<br/><span style=\"font-family: monospace;\">";

      if (fsize > (logPage + 1) * LOG_PAGE_SIZE) {
        logFile.seek(fsize - (logPage + 1) * LOG_PAGE_SIZE);
        logFile.readStringUntil('\n');
      }
      String line;
      while (logFile.available() && logFile.position() <= fsize - logPage * LOG_PAGE_SIZE) {
        line = logFile.readStringUntil('\n');
        page += "<br>";
        page += line;   // already contains '\n'
      }
      logFile.close();
    }

    page += "</span>\n<hr/>\n";
  } else
    page += "<br/>Logging is disabled (missing or full file system)";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

// Program setup
const unsigned long WIFI_CONNECT_TIMEOUT = 20000UL;  // (ms)
void setup(void) {

  String msg = "booted: ";
  msg += APPNAME;
  msg += " v";
  msg += APPVERSION;
  msg += " build ";
  msg += COMPILATION_TIMESTAMP;
  logger.writeln(msg);

  // Serial line  
  Serial.begin(BAUDRATE);
  Serial.println("");

  // I/O
  pinMode(PUSHBUTTON, INPUT);
  led.LowActive();

  // If the push button is pressed on boot, offer Wi-Fi configuration
  if (button.isPressedRaw()) {
    Serial.println("Push button pressed on boot; going to Wi-Fi Manager");
    logger.writeln("going to Wi-Fi Manager");
    led.On().Update();

    WiFiManager wifiManager;
    wifiManager.startConfigPortal(HOSTNAME, WIFICONFIGPASS);
  }
  button.setEventHandler(handleButtonEvent);
  led.Off().Update();

  // Network
  WiFi.mode(WIFI_STA);      // Important to avoid starting with an access point
  WiFi.hostname(HOSTNAME);
  WiFi.begin();             // Connect using stored credentials
  Serial.print("Connecting to ");
  Serial.print(WiFi.SSID());
  unsigned long time0 = millis(), time1 = time0, timedot = time0;
  led.Breathe(GLOW_DELAY).Forever().Update();
  while (WiFi.status() != WL_CONNECTED && time1 - time0 < WIFI_CONNECT_TIMEOUT) {
    yield();
    led.Update();
    time1 = millis();
    if (time1 - timedot >= 100UL) {
      Serial.print(".");
      timedot = time1;
    }
  }
  Serial.println("");
  led.Off().Update();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected!");
    logger.writeln("Wi-Fi connected on boot");
    Serial.printf("IP address: %s, RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("Connection timeout");
    logger.writeln("Wi-Fi connection timeout on boot");
 }

  // Run discovery
  yl_discover();

  // Load settings from EEPROM
  EEPROM.begin(4);
  if (EEPROM.read(0) == 'Y' && EEPROM.read(1) == 'B' && EEPROM.read(2) == EEPROM_FORMAT_VERSION) {
    char bulbid_c[YL_ID_LENGTH + 1] = {0,};
    const uint8_t n = EEPROM.read(3);
    Serial.printf("Found %d bulb%s configuration in EEPROM\n", n, n == 1 ? "" : "s");
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
  server.on("/",     handleRoot);
  server.on("/conf", handleConf);
  server.on("/save", handleSave);
  server.on("/flip", handleFlip);
  server.on("/log",  handleLog);
  server.begin();
  Serial.println("Web server started");

  // Reduce connection timeout for inactive bulbs
  client.setTimeout(CONNECTION_TIMEOUT);
}

// Program loop
void loop(void) {

  // Check the button state
  if (button_pressed) {
    button_pressed = false;
    Serial.println("Button pressed");
    logger.writeln("Button pressed");

    // LED diagnostics:
    // 1 blink  - light flip OK
    // 1 + 2 blinks - one of the bulbs did not respond
    // 2 blinks - button not linked to bulbs
    // 1 glowing - Wi-Fi disconnected
    if (WiFi.status() != WL_CONNECTED) {

      // No Wi-Fi
      Serial.println("No Wi-Fi connection");
      led.Breathe(GLOW_DELAY).Repeat(1);            // 1 glowing
    } else {
      if (nabulbs) {

        // Flipping may block, causing JLED-style blink not being properly processed. Hence, force sequential processing (first blink, then flip)
        // To make JLED working smoothly in this case, an asynchronous WiFiClient.connect() method would be needed
        // This is not included in ESP8266 Core (https://github.com/esp8266/Arduino/issues/922), but is available as a separate library (like ESPAsyncTCP)
        // Since, for this project, it is a minor issue (flip being sent to bulbs with 100 ms delay), we stay with blocking connect()
        led.On().Update();
        delay(BLINK_DELAY);       // 1 blink
        led.Off().Update();

        if (yl_flip())

          // Some bulbs did not respond
          // Because of connection timeout, the blinking will be 1 + pause + 2
          led.Blink(BLINK_DELAY, BLINK_DELAY * 2).Repeat(2);  // 2 blinks

      } else {

        // Button not linked
        Serial.println("Button not linked to bulbs");
        led.Blink(BLINK_DELAY, BLINK_DELAY * 2).Repeat(2);    // 2 blinks
      }
    }
  }

  // Background processing
  button.check();
  led.Update();
  server.handleClient();
  MDNS.update();
  logger.rotate();
}
