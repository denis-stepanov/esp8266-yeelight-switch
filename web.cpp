/* Yeelight Smart Switch App for ESP8266
 * * Web pages
 * (c) DNS 2018-2021
 */

#include "MySystem.h"                      // System-level definitions
#include "BulbManager.h"                   // Bulb manager

using namespace ds;

#define web_page System::web_page          // To make the code a bit lighter

// Data providers
extern BulbManager bulb_manager;           // Bulb manager

// TODO: make auto-redirect from reply pages

// Root page. Show status
void handleRoot() {
  const auto nabulbs = bulb_manager.getNumActive();
  System::pushHTMLHeader(System::hostname);
  web_page += "<h3>Yeelight Button</h3>";
  if (nabulbs) {
    web_page += "<p>Linked to the bulb";
    if (nabulbs > 1)
      web_page += "s";
    web_page += ":</p><p><ul>";
    bulb_manager.printStatusHTML();
    web_page += "</ul></p>";
  } else
    web_page += "<p>Not linked to a bulb</p>";
  web_page += "<p>[<a href=\"conf\">Change</a>]";
  if (bulb_manager.getNumActive())
    web_page += " [<a href=\"flip\">Flip</a>]";
  web_page += " [<a href=\"timers\">Timers</a>]";
  web_page += " [<a href=\"log\">Log</a>]";
  web_page += " [<a href=\"about\">About</a>]";
  web_page += "</p><hr/><p><i>Connected to network ";
  web_page += System::getNetworkName();
  web_page += ", hostname ";
  web_page += System::hostname;
  web_page += ".local (";
  web_page += System::getLocalIPAddress();
  web_page += "), ";
  web_page += System::getNetworkDetails();
  web_page += ".</i></p>";
  web_page += "<p><small><a href=\"";
  web_page += System::app_url;
  web_page += "\">";
  web_page += System::app_name;
  web_page += "</a> v";
  web_page += System::app_version;
  web_page += " build ";
  web_page += System::app_build;
  web_page += "</small></p>";
  System::pushHTMLFooter();
  System::sendWebPage();
}

// Bulb discovery page
void handleConf() {
  System::pushHTMLHeader((String)System::hostname + " conf");
  web_page += "<h3>Yeelight Button Configuration</h3>";
  web_page += "<p>[<a href=\"/conf\">Rescan</a>] [<a href=\"/save\">Unlink</a>] [<a href=\"..\">Back</a>]</p>";
  web_page += "<p><i>Scanning ";
  web_page += System::getNetworkName();
  web_page += " for Yeelight devices...</i></p>";
  web_page += "<p><i>Hint: turn all bulbs off, except the desired ones, in order to identify them easily.</i></p>";

  // Use chunked transfer to show scan in progress. Works in Chrome, not so well in Firefox
  System::web_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  System::sendWebPage();
  const auto num_bulbs = bulb_manager.discover();
  web_page = "<p>Found ";
  web_page += num_bulbs;
  web_page += " bulb";
  if (num_bulbs != 1)
    web_page += "s";
  web_page +=". Select bulbs to link from the list below.</p>";
  web_page += "<form action=\"/save\"><p style=\"font-family: monospace;\">";
  bulb_manager.printConfHTML();
  web_page += "</p><p><input type=\"submit\" value=\"Link\"/></p></form>";
  System::pushHTMLFooter();
  System::web_server.sendContent(web_page);
  System::web_server.sendContent("");
  System::web_server.client().stop();   // Terminate chunked transfer
}

// Configuration saving page
void handleSave() {

  bulb_manager.save();

  const auto nargs = System::web_server.args();
  const auto nabulbs = bulb_manager.getNumActive();
  System::pushHTMLHeader(System::hostname + nabulbs || !nargs ? " saved" : " error");
  web_page += "<h3>Yeelight Button Configuration ";
  web_page += nabulbs || !nargs ?  "Saved" : " Error";
  web_page += "</h3>";
  if (nargs) {
    if (nabulbs) {
      web_page += "<p>";
      web_page += nabulbs;
      web_page += " bulb";
      if (nabulbs != 1)
        web_page += "s";
      web_page += " linked</p>";
    } else
      web_page += "<p>Too many bulbs passed</p>";
  } else
    web_page += "<p>Bulbs unlinked</p>";
  web_page += "<p>[<a href=\"..\">Back</a>]</p>";
  System::pushHTMLFooter();
  System::sendWebPage();
}

// Bulb flip page. Accessing this page immediately flips the light
void handleFlip() {
  bulb_manager.flip();
  String lmsg = "Web page flip received from ";
  lmsg += System::web_server.client().remoteIP().toString();
  System::appLogWriteLn(lmsg);

  System::pushHTMLHeader((const String)System::hostname + " flip");
  web_page += "<h3>Yeelight Button Flip</h3>";
  web_page += bulb_manager.getNumActive() ? "<p>Light flipped</p>" : "<p>No linked bulbs found</p>";
  web_page += "<p>[<a href=\"/flip\">Flip</a>] [<a href=\"..\">Back</a>]</p>";
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

// Install handler
void (*System::registerWebPages)() = registerPages;
