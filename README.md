# arduino-traffic-notifier
Get notified of busy traffic using Google Maps API and an ESP8266

## Makezine readers
The article in the 59th edition incorrectly links to this report, the report you are looking for is [this one](https://github.com/witnessmenow/Bridge-Traffic-Display). Apologies! 

### Features
- Gets estimated travel time from Google Maps API
- Displays on a OLED screen and lights an RBG LED
- Uses Telegram messenger to configure
- Persists configurations using SPIFFS

### Requires
- Maps API Key - [Instructions on Arduino Google Maps Library Github](https://github.com/witnessmenow/arduino-google-maps-api)
- Telegram Bot Token - [Instructions on Universal-Arduino-Telegram-Bot Library Github](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot)
- Telegram Chat ID - get off @myIdBot in Telegram

### Hardware

This should run on any ESP8266 device and you could display the data on any display you prefer, but here is an example hardware and wiring diagram that the code is written to. Total price of this build is roughly $10.

#### Parts List
- [Wemos D1 Mini](http://s.click.aliexpress.com/e/uzFUnIe)
- [1.3" OLED Display](http://s.click.aliexpress.com/e/EqByrzb)
- [RGB LED - 10 pack (only need 1)](http://s.click.aliexpress.com/e/JuN7AAU)
- [Breadboard, 3 x 1k Resistors and Wires - Basic starter kit](http://s.click.aliexpress.com/e/BQvZZ7A)

![alt text](http://i.imgur.com/e9onX8Q.png "Circuit Diagram")


 
