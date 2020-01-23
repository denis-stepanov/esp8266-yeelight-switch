# esp8266-yeelight-switch
Arduino project for controlling an Yeelight bulb using an ESP8266 MCU and a push button.

Program features:
* Use of local API, meaning nearly instantaneous light switching;
* Support for Yeelight devices discovery on the network;
* Support for multiple bulbs switching;
* Visible user feedback using the ESP8266's built-in LED;
* Support for Wi-Fi network reconfiguration;
* Web interface with mDNS support to configure the switch;
* Support for turning the bulb on or off via web interface, including a direct URL for toggle;
* Storing of the user-selected light device in EEPROM (survives power off);
* No hardcoded or entered bulb IP addresses;
* Detailed diagnostics sent over serial interface.

Current known limitations:
* The bulb has to be online when the switch boots, otherwise the switch will start unlinked;
* The switch is not intended to operate on battery; see issue #3 for more details.

Usage:
 1. review the configuration settings at the top of the program; compile and flash your ESP8266;
 2. boot with the push button pressed, connect your computer to the Wi-Fi network "ybutton1", password "Yeelight", go to the captive portal, enter and save your Wi-Fi network credentials;
 3. in your Wi-Fi network, go to http://ybutton1.local, run the Yeelight scan and link the switch to the bulb found;
 4. use the push button to control your bulb manually;
 5. access to http://ybutton1.local/flip to toggle the bulb from a script.
 
 LED response to the button:
 * 1 blink  - bulb flip OK;
 * 1 + 2 blinks - one of the bulbs did not respond;
 * 2 blinks - button not linked to a bulb;
 * 1 long blink - Wi-Fi disconnected.
 
 The LED is constantly lit during Wi-Fi reconfiguration process.
 
 Prerequsites:
 1. Hardware:
    1. ESP8266 (tested with ESP-12E Witty Cloud);
    1. Push button connected to a GPIO and pulled high;
 1. Software:
    1. Arduino IDE, https://www.arduino.cc/en/main/software (version tested: 1.8.5);
    1. ESP8266 core for Arduino, https://github.com/esp8266/Arduino (version tested: 2.6.3);
    1. WiFiManager library for Arduino, https://github.com/tzapu/WiFiManager (version tested: 0.15).
 
 If you have an ESP with an onboard button, such as Witty Cloud Development board, the program can be used out of the box. Otherwise you need to wire the button and update the GPIO number in the source code.
