/* Yeelight Smart Switch App for ESP8266
 * * Web pages
 * (c) DNS 2018-2021
 */

#include "MySystem.h"                      // System-level definitions
#include <EEPROM.h>                        // EEPROM support
#include <LinkedList.h>                    // Linked list support
#include "YBulb.h"                         // Yeelight support

using namespace ds;

// Data providers
extern LinkedList<YBulb *> bulbs;          // List of known bulbs
extern uint8_t nabulbs;                    // Number of active bulbs
extern const uint8_t EEPROM_FORMAT_VERSION;// EEPROM format version
extern const char *LOGFILENAME;            // Main log file name
extern const char *LOGFILENAME2;           // Rotated log file name

// Web pages business logic
void yl_discover(void);                    // Yeelight discovery
int yl_flip(void);                         // Yeelight bulb flip
uint8_t yl_nabulbs(void);                  // Return number of active bulbs

// TODO: make auto-redirect from reply pages

// Root page. Show status
void handleRoot() {
  System::pushHTMLHeader(System::hostname);
  System::web_page += "<h3>Yeelight Button</h3>";
  if (nabulbs) {
    System::web_page += "<p>Linked to the bulb";
    if (nabulbs > 1)
      System::web_page += "s";
    System::web_page += ":</p><p><ul>";
    for (uint8_t i = 0; i < bulbs.size(); i++) {
      YBulb *bulb = bulbs.get(i);
      if (bulb->isActive()) {
        System::web_page += "<li>id(";
        System::web_page += bulb->GetID();
        System::web_page += "), ip(";
        System::web_page += bulb->GetIP();
        System::web_page += "), name(";
        System::web_page += bulb->GetName();
        System::web_page += "), model(";
        System::web_page += bulb->GetModel();
        System::web_page += ")</li>";
      }
    }
    System::web_page += "</ul></p>";
  } else
    System::web_page += "<p>Not linked to a bulb</p>";
  System::web_page += "<p>[<a href=\"conf\">Change</a>]";
  if (nabulbs)
    System::web_page += " [<a href=\"flip\">Flip</a>]";
  System::web_page += " [<a href=\"log\">Log</a>]";
  System::web_page += " [<a href=\"about\">About</a>]";
  System::web_page += "</p><hr/><p><i>Connected to network ";
  System::web_page += System::getNetworkName();
  System::web_page += ", hostname ";
  System::web_page += System::hostname;
  System::web_page += ".local (";
  System::web_page += System::getLocalIPAddress();
  System::web_page += "), ";
  System::web_page += System::getNetworkDetails();
  System::web_page += ".</i></p>";
  System::web_page += "<p><small><a href=\"";
  System::web_page += System::app_url;
  System::web_page += "\">";
  System::web_page += System::app_name;
  System::web_page += "</a> v";
  System::web_page += System::app_version;
  System::web_page += " build ";
  System::web_page += System::app_build;
  System::web_page += "</small></p>";
  System::pushHTMLFooter();
  System::sendWebPage();
}

// Bulb discovery page
void handleConf() {
  System::pushHTMLHeader((String)System::hostname + " conf");
  System::web_page += "<h3>Yeelight Button Configuration</h3>";
  System::web_page += "<p>[<a href=\"/conf\">Rescan</a>] [<a href=\"/save\">Unlink</a>] [<a href=\"..\">Back</a>]</p>";
  System::web_page += "<p><i>Scanning ";
  System::web_page += System::getNetworkName();
  System::web_page += " for Yeelight devices...</i></p>";
  System::web_page += "<p><i>Hint: turn all bulbs off, except the desired ones, in order to identify them easily.</i></p>";

  // Use chunked transfer to show scan in progress. Works in Chrome, not so well in Firefox
  System::web_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  System::sendWebPage();
  yl_discover();
  System::web_page = "<p>Found ";
  System::web_page += bulbs.size();
  System::web_page += " bulb";
  if (bulbs.size() != 1)
    System::web_page += "s";
  System::web_page +=". Select bulbs to link from the list below.</p>";
  System::web_page += "<form action=\"/save\"><p style=\"font-family: monospace;\">";
  for (uint8_t i = 0; i < bulbs.size(); i++) {
    YBulb *bulb = bulbs.get(i);
    System::web_page += "<input type=\"checkbox\" name=\"bulb\" value=\"";
    System::web_page += i;
    System::web_page += "\"";
    if (bulb->isActive())
      System::web_page += " checked";
    System::web_page += "/> ";
    System::web_page += bulb->GetIP();
    System::web_page += " id(";
    System::web_page += bulb->GetID();
    System::web_page += ") name(";
    System::web_page += bulb->GetName();
    System::web_page += ") model(";
    System::web_page += bulb->GetModel();
    System::web_page += ") power(";
    System::web_page += bulb->GetPower() ? "<b>on</b>" : "off";
    System::web_page += ")<br/>";
  }
  System::web_page += "</p><p><input type=\"submit\" value=\"Link\"/></p></form>";
  System::pushHTMLFooter();
  System::web_server.sendContent(System::web_page);
  System::web_server.sendContent("");
  System::web_server.client().stop();   // Terminate chunked transfer
}

// Configuration saving page
// EEPROM format:
//   0-1: 'YB' - Yeelight Bulb configuration marker
//     2: format version. Increment each time the format changes
//     3: number of stored bulbs
//  4-22: <selected bulb ID> (19 bytes, null-terminated)
//      : ...
void handleSave() {

  unsigned int nargs = System::web_server.args();
  const size_t used_eeprom_size = 2 + 1 + 1 + (YL_ID_LENGTH + 1) * nargs;   // TODO: maybe put some constraint on nargs (externally provided parameter)
  EEPROM.begin(used_eeprom_size);
  unsigned int eeprom_addr = 4;

  for (uint8_t i = 0; i < bulbs.size(); i++)
    bulbs.get(i)->Deactivate();
  nabulbs = 0;

  if(nargs) {
    for(unsigned int i = 0; i < nargs; i++) {
      unsigned char n = System::web_server.arg(i).c_str()[0] - '0';
      if (n < bulbs.size()) {
        YBulb *bulb = bulbs.get(n);
        char bulbid_c[YL_ID_LENGTH + 1] = {0,};
        strncpy(bulbid_c, bulb->GetID(), YL_ID_LENGTH);
        EEPROM.put(eeprom_addr, bulbid_c);
        eeprom_addr += sizeof(bulbid_c);
        bulb->Activate();
      } else
        System::log->printf(TIMED("Bulb #%d does not exist\n"), n);
    }

    nabulbs = yl_nabulbs();
    if (nabulbs) {

      // Write the header
      EEPROM.write(0, 'Y');
      EEPROM.write(1, 'B');
      EEPROM.write(2, EEPROM_FORMAT_VERSION);
      EEPROM.write(3, nabulbs);
      System::log->printf(TIMED("%d bulb%s stored in EEPROM, using %u byte(s)\n"), nabulbs, nabulbs == 1 ? "" : "s", eeprom_addr);
    } else
      System::log->printf(TIMED("No bulbs were stored in EEPROM\n"));
  } else {

    // Unlink all

    // Overwriting the EEPROM marker will effectively cause forgetting the settings
    EEPROM.write(0, 0);
    System::log->printf(TIMED("Bulbs unlinked from the switch"));
  }

  // TODO: check for errors?
  EEPROM.commit();
  EEPROM.end();

  System::pushHTMLHeader(System::hostname + nabulbs || !nargs ? " saved" : " error");
  System::web_page += "<h3>Yeelight Button Configuration ";
  System::web_page += nabulbs || !nargs ?  "Saved" : " Error";
  System::web_page += "</h3>";
  if (nargs) {
    if (nabulbs) {
      System::web_page += "<p>";
      System::web_page += nabulbs;
      System::web_page += " bulb";
      if (nabulbs != 1)
        System::web_page += "s";
      System::web_page += " linked</p>";
    } else
      System::web_page += "<p>Too many bulbs passed</p>";
  } else
    System::web_page += "<p>Bulbs unlinked</p>";
  System::web_page += "<p>[<a href=\"..\">Back</a>]</p>";
  System::pushHTMLFooter();
  System::sendWebPage();
}

// Bulb flip page. Accessing this page immediately flips the light
void handleFlip() {
  yl_flip();
  String lmsg = "Web page flip received from ";
  lmsg += System::web_server.client().remoteIP().toString();
  System::appLogWriteLn(lmsg);

  System::pushHTMLHeader((String)System::hostname + " flip");
  System::web_page += "<h3>Yeelight Button Flip</h3>";
  System::web_page += nabulbs ? "<p>Light flipped</p>" : "<p>No linked bulbs found</p>";
  System::web_page += "<p>[<a href=\"/flip\">Flip</a>] [<a href=\"..\">Back</a>]</p>";
  System::pushHTMLFooter();
  System::sendWebPage();
}

// Activate web pages
void registerPages() {
  System::web_server.on("/",     handleRoot);
  System::web_server.on("/conf", handleConf);
  System::web_server.on("/save", handleSave);
  System::web_server.on("/flip", handleFlip);
}
void (*System::registerWebPages)() = registerPages;
