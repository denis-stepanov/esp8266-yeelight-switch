/* Yeelight Smart Switch App for ESP8266
 * * Web pages
 * (c) DNS 2018-2021
 */

#include "MySystem.h"                      // System-level definitions
#include "BulbManager.h"                   // Bulb manager

using namespace ds;

// Initialize page buffer with page header
static void pushHeader(const String& title, bool redirect = false) {
  auto &page = System::web_page;

  System::pushHTMLHeader(title,
    F(
    "<style>\n"
    "  .off { filter: grayscale(100%); }\n"
    "</style>\n"),
    redirect);
  page += F("<h3>");
  page += title;
  page += F("</h3>\n");

  // Navigation
  page += F(
    "<table cellpadding=\"0\" cellspacing=\"0\" width=\"100%\"><tr><td>"
    "[&nbsp;<a href=\"/\">home</a>&nbsp;]&nbsp;&nbsp;&nbsp;"
    "[&nbsp;<a href=\"/conf\">config</a>&nbsp;]&nbsp;&nbsp;&nbsp;"
    "[&nbsp;<a href=\"/timers\">timers</a>&nbsp;]&nbsp;&nbsp;&nbsp;"
    "[&nbsp;<a href=\"/log\">log</a>&nbsp;]<br/>"
    "[&nbsp;<a href=\"/about\">about</a>&nbsp;]&nbsp;&nbsp;&nbsp;"
    "</td><td align=\"right\" valign=\"top\">"
  );
  if (System::getTimeSyncStatus() != TIME_SYNC_NONE)
    page += System::getTimeStr();
  page += F(
    "</td></tr></table>"
    "<hr/>\n"
  );
}

// Append page footer to the buffer
static void pushFooter() {
  System::pushHTMLFooter();
}


// Root page. Show status
void handleRoot() {
  auto &page = System::web_page;

  // Execute command, if any
  if (System::web_server.args() > 0) {
    for (unsigned int i = 0; i < (unsigned int)System::web_server.args(); i++) {
      const String cmd = System::web_server.argName(i);
      String reason("Web page command \"");
      reason += cmd;
      reason += "\" received from ";
      reason += System::web_server.client().remoteIP().toString();
      if (cmd == "on")
        bulb_manager.processEvent(BulbManager::EVENT_ON, reason);
      else if (cmd == "off")
        bulb_manager.processEvent(BulbManager::EVENT_OFF, reason);
      else if (cmd == "flip")
        bulb_manager.processEvent(BulbManager::EVENT_FLIP, reason);
      else
        System::log->printf(TIMED("Invalid command: '%s', ignoring\n"), cmd.c_str());
    }
  }

  pushHeader("Yeelight Button");
  if (bulb_manager.isLinked()) {
    page += "<p>Linked to the bulb";
    if (bulb_manager.getNumActive() > 1)
      page += "s";
    page += ":</p><p><ul>";
    bulb_manager.printStatusHTML();
    page += "</ul></p>";
  } else
    page += "<p>Not linked to a bulb</p>";
  pushFooter();
  System::sendWebPage();
}

// Bulb discovery page
void handleConf() {
  auto &page = System::web_page;

  pushHeader("Yeelight Button Configuration");
  page += "<p>[<a href=\"/conf\">Rescan</a>] [<a href=\"/save\">Unlink</a>] [<a href=\"..\">Back</a>]</p>";
  page += "<p><i>Scanning ";
  page += System::getNetworkName();
  page += " for Yeelight devices...</i></p>";
  page += "<p><i>Hint: turn all bulbs off, except the desired ones, in order to identify them easily.</i></p>";

  // Use chunked transfer to show scan in progress. Works in Chrome, not so well in Firefox
  System::web_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  System::sendWebPage();
  const auto num_bulbs = bulb_manager.discover();
  page = "<p>Found ";
  page += num_bulbs;
  page += " bulb";
  if (num_bulbs != 1)
    page += "s";
  page +=". Select bulbs to link from the list below.</p>";
  page += "<form action=\"/save\"><p style=\"font-family: monospace;\">";
  bulb_manager.printConfHTML();
  page += "</p><p><input type=\"submit\" value=\"Link\"/></p></form>";
  pushFooter();
  System::web_server.sendContent(page);
  System::web_server.sendContent("");
  System::web_server.client().stop();   // Terminate chunked transfer
}

// Configuration saving page
void handleSave() {
  auto &page = System::web_page;

  bulb_manager.save();

  const auto nargs = System::web_server.args();
  const auto linked = bulb_manager.isLinked();
  const auto nabulbs = bulb_manager.getNumActive();
  pushHeader(String("Yeelight Button Configuration") + linked || !nargs ? " Saved" : " Error", true);
  if (nargs) {
    if (linked) {
      page += "<p>";
      page += nabulbs;
      page += " bulb";
      if (nabulbs > 1)
        page += "s";
      page += " linked</p>";
    } else
      page += "<p>Too many bulbs passed</p>";
  } else
    page += "<p>Bulbs unlinked</p>";
  pushFooter();
  System::sendWebPage();
}

// Activate web pages
void registerPages() {
  System::web_server.on("/",     handleRoot);
  System::web_server.on("/conf", handleConf);
  System::web_server.on("/save", handleSave);
}

// Install handler
void (*System::registerWebPages)() = registerPages;
