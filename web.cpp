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
      String reason(F("Web page command \""));
      reason += cmd;
      reason += F("\" received from ");
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

  pushHeader(F("Yeelight Button"));

  // Icon
  page += F("<center><span style=\"font-size: 3cm;\"");
  if (bulb_manager.isOff())
    page +=  F(" class=\"off\"");
  page += F(">\xf0\x9f\x92\xa1");    // UTF-8 'ELECTRIC LIGHT BULB'
  page += F("</span><br/>");

  //// Newlines are intentional, to facilitate scripting
  page += F("\nLights are ");
  page += bulb_manager.isOn() ? "ON" : "OFF";
  page += F("\n<p>");

  page += F("\n<input type='button' name='on' value='   On   ' onclick='location.href=\"/?on\"'>&nbsp;&nbsp;");
  page += F("\n<input type='button' name='flip' value='Toggle' onclick='location.href=\"/?flip\"'>&nbsp;&nbsp;");
  page += F("\n<input type='button' name='off' value='   Off   ' onclick='location.href=\"/?off\"'>");

  page += F("\n</p>\n");

  // Table of bulbs
  page += F("Linked bulbs:<br/>\n");
  bulb_manager.printStatusHTML(page);
  page += F("</center>\n");

  pushFooter();
  System::sendWebPage();
}

// Bulb discovery page
void handleConf() {
  auto &page = System::web_page;

  pushHeader(F("Yeelight Button Configuration"));
  page += F("<p>[&nbsp;<a href=\"/conf\">rescan</a>&nbsp;] [&nbsp;<a href=\"/save\">unlink all</a>&nbsp;]</p>\n");
  page += F("<p><i>Scanning ");
  page += System::getNetworkName();
  page += F(" for Yeelight devices...</i></p>\n");
  page += F("<p><i>Hint: turn all bulbs off, except the desired ones, in order to identify them easily.</i></p>\n");

  // Use chunked transfer to show scan in progress
  System::web_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  System::sendWebPage();
  const auto num_bulbs = bulb_manager.discover();
  page = F("<p>Found ");
  page += num_bulbs;
  page += F(" bulb");
  if (num_bulbs != 1)
    page += 's';
  page += F(". Select bulbs to link from the list below.</p>\n");
  page += F("<form action=\"/save\">\n");
  bulb_manager.printConfHTML(page);
  page += F("<p><input type=\"submit\" value=\"Link\"/></p>\n</form>\n");
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
  pushHeader(String("Yeelight Button Configuration") + linked || !nargs ? F(" Saved") : F(" Error"), true);
  if (nargs) {
    if (linked) {
      page += F("<p>");
      page += nabulbs;
      page += F(" bulb");
      if (nabulbs > 1)
        page += 's';
      page += F(" linked</p>");
    } else
      page += F("<p>Too many bulbs passed</p>");
  } else
    page += F("<p>Bulbs unlinked</p>");
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
