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

  // Icon
  page += "<center><span style=\"font-size: 3cm;\"";
  if (bulb_manager.isOff())
    page +=  " class=\"off\"";
  page += ">\xf0\x9f\x92\xa1";    // UTF-8 'ELECTRIC LIGHT BULB'
  page += "</span><br/>";

  //// Newlines are intentional, to facilitate scripting
  page += "\nLights are ";
  page += bulb_manager.isOn() ? "ON" : "OFF";
  page += "\n<p>";

  page += "\n<input type='button' name='on' value='   On   ' onclick='location.href=\"/?on\"'>&nbsp;&nbsp;";
  page += "\n<input type='button' name='flip' value='Toggle' onclick='location.href=\"/?flip\"'>&nbsp;&nbsp;";
  page += "\n<input type='button' name='off' value='   Off   ' onclick='location.href=\"/?off\"'>";

  page += "\n</p>\n";

  // Table of bulbs
  page += "Linked bulbs:<br/>\n";
  bulb_manager.printStatusHTML(page);
  page += "</center>\n";

  pushFooter();
  System::sendWebPage();
}

// Bulb discovery page
void handleConf() {
  auto &page = System::web_page;

  pushHeader("Yeelight Button Configuration");
  page += "<p>[&nbsp;<a href=\"/conf\">rescan</a>&nbsp;] [&nbsp;<a href=\"/save\">unlink all</a>&nbsp;]</p>";
  page += "<p><i>Scanning ";
  page += System::getNetworkName();
  page += " for Yeelight devices...</i></p>";
  page += "<p><i>Hint: turn all bulbs off, except the desired ones, in order to identify them easily.</i></p>";

  // Use chunked transfer to show scan in progress
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
  bulb_manager.printConfHTML(page);
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
