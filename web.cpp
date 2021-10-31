/* Yeelight Smart Switch App for ESP8266
 * * Web pages
 * (c) DNS 2018-2021
 */

#include "MySystem.h"                      // System-level definitions
#include "BulbManager.h"                   // Bulb manager

using namespace ds;

// Data providers
extern BulbManager bulb_manager;           // Bulb manager
extern const char *LOGFILENAME;            // Main log file name
extern const char *LOGFILENAME2;           // Rotated log file name

// TODO: make auto-redirect from reply pages

// Root page. Show status
void handleRoot() {
  const auto nabulbs = bulb_manager.getNumActive();
  System::pushHTMLHeader(System::hostname);
  System::web_page += "<h3>Yeelight Button</h3>";
  if (nabulbs) {
    System::web_page += "<p>Linked to the bulb";
    if (nabulbs > 1)
      System::web_page += "s";
    System::web_page += ":</p><p><ul>";
    bulb_manager.printStatusHTML();
    System::web_page += "</ul></p>";
  } else
    System::web_page += "<p>Not linked to a bulb</p>";
  System::web_page += "<p>[<a href=\"conf\">Change</a>]";
  if (bulb_manager.getNumActive())
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
  const auto num_bulbs = bulb_manager.discover();
  System::web_page = "<p>Found ";
  System::web_page += num_bulbs;
  System::web_page += " bulb";
  if (num_bulbs != 1)
    System::web_page += "s";
  System::web_page +=". Select bulbs to link from the list below.</p>";
  System::web_page += "<form action=\"/save\"><p style=\"font-family: monospace;\">";
  bulb_manager.printConfHTML();
  System::web_page += "</p><p><input type=\"submit\" value=\"Link\"/></p></form>";
  System::pushHTMLFooter();
  System::web_server.sendContent(System::web_page);
  System::web_server.sendContent("");
  System::web_server.client().stop();   // Terminate chunked transfer
}

// Configuration saving page
void handleSave() {

  bulb_manager.save();

  unsigned int nargs = System::web_server.args();
  const auto nabulbs = bulb_manager.getNumActive();
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
  bulb_manager.flip();
  String lmsg = "Web page flip received from ";
  lmsg += System::web_server.client().remoteIP().toString();
  System::appLogWriteLn(lmsg);

  System::pushHTMLHeader((String)System::hostname + " flip");
  System::web_page += "<h3>Yeelight Button Flip</h3>";
  System::web_page += bulb_manager.getNumActive() ? "<p>Light flipped</p>" : "<p>No linked bulbs found</p>";
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
