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

#include "FS.h"

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D6;
const int SDC_PIN = D7;

const int RGB_RED_PIN = D3;
const int RGB_GREEN_PIN = D2;
const int RGB_BLUE_PIN = D1;


//------- Replace the following! ------
char ssid[] = "SSID";       // your network SSID (name)
char password[] = "password";  // your network key
#define API_KEY "YourGoogleApiKey"  // your google apps API Token
#define ARDUINO_MAPS_BOT_TOKEN "YourTelegramBotToken" //Telegram
#define CHAT_ID "123456712" //Telegram chat Id

WiFiClientSecure client;
GoogleMapsApi api(API_KEY, client);
UniversalTelegramBot bot(ARDUINO_MAPS_BOT_TOKEN, client);
long telegramDue;

SH1106Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);

//Free Google Maps Api only allows for 2500 "elements" a day, so careful you dont go over
long mapDueTime;
const int mapDueDelay = 60000;
const int mapDueDelayIfFailed = 5000;

//Inputs
String origin = "Galway,+Ireland";
String destination = "Dublin,+Ireland";
String displayText = "Galway -> Dublin";
float limit = 10.0;

//Optional
String departureTime = "now"; //This can also be a timestamp, needs to be in the future for traffic info
String trafficModel = "best_guess"; //defaults to this anyways. see https://developers.google.com/maps/documentation/distance-matrix/intro#DistanceMatrixRequests for more info

String durationInTraffic;
int differenceInMinutes;
float percentageDifference;

void setup() {

  Serial.begin(115200);

    // Initialize the LED pins as an outputs
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);

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


  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  loadConfig();

  bot.sendMessage(CHAT_ID, "Starting Up", "");
  delay(1000);
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  origin = json["origin"].as<String>();
  destination = json["destination"].as<String>();
  displayText = json["displayText"].as<String>();
  limit = json["limit"].as<float>();
  return true;
}

bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["origin"] = origin;
  json["destination"] = destination;
  json["displayText"] = displayText;
  json["limit"] = limit;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}

void turnLEDRed() {
  digitalWrite(RGB_RED_PIN, HIGH);
  digitalWrite(RGB_BLUE_PIN, LOW);
  digitalWrite(RGB_GREEN_PIN, LOW);
}

void turnLEDGreen() {
  digitalWrite(RGB_RED_PIN, LOW);
  digitalWrite(RGB_BLUE_PIN, LOW);
  digitalWrite(RGB_GREEN_PIN, HIGH);
}

void displayTraffic() {
  display.clear();
  display.drawString(0, 0, displayText);
  display.drawString(0, 15, "Traf: " + durationInTraffic);
  if (percentageDifference < 0.0) {
    display.drawString(0, 30, String(differenceInMinutes * (-1)) + " mins (" + String(percentageDifference * (-1)) + "%) slower");
  } else {
    display.drawString(0, 30, String(differenceInMinutes) + " mins (" + String(percentageDifference) + "%) quicker");
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
          percentageDifference = (difference * 100.0) / durationInSeconds;
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

void setLed() {
  if (percentageDifference < limit){
    turnLEDRed();
  } else {
    turnLEDGreen();
  }
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
      if (text.startsWith("/limit")) {
        Serial.println(text);
        text.remove(0, 7);
        limit = text.toFloat();
        bot.sendMessage(chat_id, "Set limit: " + String(limit), "");
      }
      if (text == "/save") {
        saveConfig();
        mapDueTime = 0;
        bot.sendMessage(chat_id, "Saved config, checking maps api now.", "");
      }

      if (text == "/values") {
        String values = "Current Values\n";
        values = values + "/text : " + displayText + "\n";
        values = values + "/origin : " + origin + "\n";
        values = values + "/destination : " + destination + "\n";
        values = values + "/limit : " + limit + "\n";
        bot.sendMessage(chat_id, values, "Markdown");
      }

      if (text == "/start") {
        String welcome = "Welcome from Google Maps Bot\n";
        welcome = welcome + "/destination : set destination\n";
        welcome = welcome + "/origin : set origin\n";
        welcome = welcome + "/text : sets display text\n";
        welcome = welcome + "/save : saves new values to config and refreshes screen\n";
        welcome = welcome + "/limit : sets the cut off for the colour of the LED\n";
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
      setLed();
    } else {
      mapDueTime = now + mapDueDelayIfFailed;
    }
  }
  delay(500);
  now = millis();
  if (now > telegramDue)  {
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
