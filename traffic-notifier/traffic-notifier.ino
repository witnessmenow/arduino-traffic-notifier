// ------------------------------
// ##########################
// Arduino Traffic Notifier
// ##########################
//
// Reads traffic from Google Maps
// Displays on a screen and lights an RBG LED
// Uses Telegram messenger to configure
//
// https://github.com/witnessmenow/arduino-traffic-notifier
//
// By Brian Lough
// https://www.youtube.com/channel/UCezJOfu7OtqGzd5xrP3q6WA
// ------------------------------

#include <GoogleMapsApi.h>
// For accessing Google Maps Api
// Availalbe on library manager (GoogleMapsApi)
// https://github.com/witnessmenow/arduino-google-maps-api

#include <UniversalTelegramBot.h>
// For sending configurations to the device
// Availalbe on library manager (UniversalTelegramBot)
// https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot

#include <SH1106Wire.h>
#include <OLEDDisplayFonts.h>
// For comunicating with the OLED screen
// Available on the library manager (esp8266-oled-ssd1306)
// https://github.com/squix78/esp8266-oled-ssd1306

#include <ArduinoJson.h>
// For parsing the response from Google Api
// Available on the library manager (ArduinoJson)
// https://github.com/bblanchon/ArduinoJson



#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D6;
const int SDC_PIN = D7;

//------- Replace the following! ------
char ssid[] = "SSID";       // your network SSID (name)
char password[] = "password";  // your network key
#define API_KEY "YourGoogleApiKey"  // your google apps API Token
#define ARDUINO_MAPS_BOT_TOKEN "YourTelegramBotToken" //Telegram
#define CHAT_ID "1234567" //Telegram chat Id

WiFiClientSecure client;
GoogleMapsApi api(API_KEY, client);
UniversalTelegramBot bot(ARDUINO_MAPS_BOT_TOKEN, client);
long telegramDue;

SH1106Wire        display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);

//Free Google Maps Api only allows for 2500 "elements" a day, so carful you dont go over
long mapDueTime;
const int mapDueDelay = 60000;
const int mapDueDelayIfFailed = 5000;


//Inputs
String origin = "Galway,+Ireland";
String destination = "Dublin,+Ireland";
String displayText = "Galway -> Dublin";

//Optional
String departureTime = "now"; //This can also be a timestamp, needs to be in the future for traffic info
String trafficModel = "best_guess"; //defaults to this anyways. see https://developers.google.com/maps/documentation/distance-matrix/intro#DistanceMatrixRequests for more info

String durationInTraffic;
int differenceInMinutes;
int percentageDifference;

void setup() {

  Serial.begin(115200);
  display.init();
  display.flipScreenVertically();

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.drawString(0, 0, "Connecting to Wifi");
  display.display();

  // Set WiFi to station mode and disconnect from an AP if it was Previously
  // connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Attempt to connect to Wifi network:
  Serial.print("Connecting Wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  display.clear();
  display.drawString(0, 0, "Connected");
  display.display();
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  bot.sendMessage(CHAT_ID, "Starting Up", "");
  delay(1000);
}

void displayTraffic() {
  display.clear();
  display.drawString(0, 0, displayText);
  display.drawString(0, 15, "Traf: " + durationInTraffic);
  if (percentageDifference > 0) {
    display.drawString(0, 30, percentageDifference + "% (" + String(differenceInMinutes) + " mins) slower");
  } else {
    display.drawString(0, 30, percentageDifference + "% (" + String(differenceInMinutes) + " mins) quicker");
  }
  display.display();
}

bool checkGoogleMaps() {
  Serial.println("Getting traffic for " + origin + " to " + destination);
    String responseString = api.distanceMatrix(origin, destination, departureTime, trafficModel);
    DynamicJsonBuffer jsonBuffer;
    JsonObject& response = jsonBuffer.parseObject(responseString);
    if (response.success()) {
      if (response.containsKey("rows")) {
        JsonObject& element = response["rows"][0]["elements"][0];
        String status = element["status"];
        if(status == "OK") {

          durationInTraffic = element["duration_in_traffic"]["text"].as<String>();

          int durationInSeconds = element["duration"]["value"];
          int durationInTrafficInSeconds = element["duration_in_traffic"]["value"];
          int difference = durationInSeconds - durationInTrafficInSeconds;
          differenceInMinutes = difference/60;
          percentageDifference = difference / durationInSeconds * 100;

          Serial.println("Duration In Traffic: " + durationInTraffic + "(" + durationInTrafficInSeconds + ")");

          return true;

        }
        else {
          Serial.println("Got an error status: " + status);
          return false;
        }
      } else {
        Serial.println("Reponse did not contain rows");
        return false;
      }
    } else {
      if(responseString == ""){
        Serial.println("No response, probably timed out");
      } else {
        Serial.println("Failed to parse Json. Response:");
        Serial.println(responseString); 
      }

      return false;
    }

    return false;
}

void handleNewMessages(int numNewMessages) {
  bool auth = true;
  for(int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    auth = chat_id == CHAT_ID;
    String text = bot.messages[i].text;
    if (auth)
    {
      if (text.startsWith("/destination")) {
        Serial.println(text);
        text.remove(0, 13);
        destination = text;
        bot.sendMessage(chat_id, "Set Destination: " + String(text), "");
      }
      if (text.startsWith("/origin")) {
        Serial.println(text);
        text.remove(0, 8);
        origin = text;
        bot.sendMessage(chat_id, "Set Origin: " + String(text), "");
      }
      if (text.startsWith("/text")) {
        Serial.println(text);
        text.remove(0, 6);
        displayText = text;
        bot.sendMessage(chat_id, "Set text: " + String(text), "");
      }
      if (text == "/now") {
        mapDueTime = 0;
        bot.sendMessage(chat_id, "Refeshing times now.", "");
      }

      if (text == "/values") {
        String values = "Current Values\n";
        values = values + "/text : " + displayText + "\n";
        values = values + "/origin : " + origin + "\n";
        values = values + "/destination : " + destination + "\n";
        bot.sendMessage(chat_id, values, "Markdown");
      }
      
      if (text == "/start") {
        String welcome = "Welcome from Google Maps Bot\n";
        welcome = welcome + "/destination : set destination\n";
        welcome = welcome + "/origin : set origin\n";
        bot.sendMessage(chat_id, welcome, "Markdown");
      }
    }
  }
}

void loop() {

  long now = millis();
  if (now > mapDueTime)  {
    Serial.println("Checking google maps");
    if (checkGoogleMaps()) {
      mapDueTime = now + mapDueDelay;
      displayTraffic();
    } else {
      mapDueTime = now + mapDueDelayIfFailed;
    }
  }
  delay(500);
  now = millis();
  if (now > telegramDue)  {
    Serial.println("Checking Telegram messages");
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    telegramDue = now + 1000;
  }
  delay(100);
  
}
