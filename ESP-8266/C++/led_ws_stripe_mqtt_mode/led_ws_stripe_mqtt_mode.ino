#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <iostream>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <sstream>
#include <FastLED.h>
#include <ctime>
#include <cstdlib>
#include "local_config.h"
#include "State.h"
#include <FS.h>

//-----------------------Constants----------------------------

#define LED_PIN h_outputPin
#define NUM_LEDS h_num_led
#define LED_TYPE WS2813
#define COLOR_ORDER GRB

#define NTP_OFFSET 60 * 60     // In seconds
#define NTP_INTERVAL 60 * 1000 // In miliseconds
#define NTP_ADDRESS "europe.pool.ntp.org"

const String deviceID = h_device_id;

// Network config:
const char *ssid = h_wifi_ssid;
const char *password = h_wifi_password;

// MQTT config:
const char *MQTT_HOST = h_mqttbroker_host;
const char *MQTT_CLIENT_ID = ("ESP8266Client" + deviceID).c_str();
const char *MQTT_USER = h_mqtt_user;
const char *MQTT_PASSWORD = h_mqtt_password;

// Topic:
const String commandTopic = "device/" + deviceID + "/command";
const String requestTopic = "device/" + deviceID + "/request";
const String pubTopic = "main/" + deviceID + "/1"; // Incomming topic id

State state;

unsigned long previousMillisMode = 0;
double currentLED[] = {0, 0, 0, 0, 0};

const char *filename = "/savedState.json";

//-----------------------------------------------------------

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL); // CRGB destroy? not more than 2 colors possible? For single stripe first one in stripe color! Mode to show multiple colors!
WiFiClient espClient;
PubSubClient pubSubClient(espClient);
CRGB leds[NUM_LEDS];

void setup()
{
  Serial.begin(19200);
  Serial.println("Setting up!");
  timeClient.begin();
  srand(time(0));

  // setup environments
  setup_spiffs();
  readSavedState();
  setup_wifi();
  setup_mqtt();
  setup_fastLED();

  // Send online state
  delay(1000);
  mqtt_publish_state();
}

void setup_spiffs()
{
  Serial.println("Setting up Spiffs!");
  bool success = SPIFFS.begin();
  if (success)
  {
    Serial.println("File system mounted with success");
  }
  else
  {
    Serial.println("Error mounting the file system");
    return;
  }
}

void setup_fastLED()
{
  Serial.println("Setting up FastLED!");
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  Serial.println("FastLED activated!");
}

void setup_wifi()
{
  Serial.println("Connecting to WiFi: ");
  Serial.print(h_wifi_ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_mqtt()
{
  Serial.println("Setting up MQTT: ");
  Serial.println(MQTT_HOST);
  pubSubClient.setServer(MQTT_HOST, 1883);
  pubSubClient.setCallback(callback);
  pubSubClient.setBufferSize(1024);
  Serial.println("Setup done MQTT");
}

void connect_mqtt()
{
  Serial.println("Connecting to MQTT: ");
  Serial.println(MQTT_HOST);

  if (!pubSubClient.connected())
  {
    Serial.println("MQTT connecting");
    while (!pubSubClient.connected())
    {
      pubSubClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
      delay(1000);
      Serial.print("-");
    }

    // Subscribe to topic
    pubSubClient.subscribe(commandTopic.c_str());
    pubSubClient.subscribe(requestTopic.c_str());

    Serial.println("");
  }
  Serial.println("MQTT Connected");
}

void readSavedState()
{
  File file = SPIFFS.open(filename, "r");

  if (!file)
  {
    defaultState();
    return;
  }

  String data;
  while (file.available())
  {
    data += (char)file.read();
  }

  if (data != "")
  {
    try {
      readJSON(data);
    }
    catch (...) {
      defaultState();
    }
  }
  else
  {
    defaultState();
  }

  file.close();
}

void defaultState() {
  Serial.println("Failed to open file for reading");

  CRGB color1(170, 80, 0);
  CRGB color2(50, 0, 0);

  state.mode = 1;
  state.brightness = 100;
  state.speed = 50;
  state.colorList = {color1, color2};
}

void mqtt_publish_state()
{
  if (!pubSubClient.connected())
  {
    connect_mqtt();
  }

  pubSubClient.publish(pubTopic.c_str(), createJSON().c_str());

  Serial.println();
  Serial.println("Pushed State to topic:");
  Serial.println(pubTopic);
  Serial.println();
}

void writeToFile()
{
  File file = SPIFFS.open(filename, "w");

  if (!file)
  {
    Serial.println("Error opening file for writing");
    return;
  }

  int bytesWritten = file.print(createJSON().c_str());

  if (bytesWritten > 0)
  {
    Serial.println("File was written");
    Serial.println(bytesWritten);
  }
  else
  {
    Serial.println("File write failed");
  }

  file.close();
}

String createJSON()
{
  String pubJson;

  timeClient.update();
  DynamicJsonDocument pubdoc(1024);

  pubdoc["timestamp"] = timeClient.getEpochTime();
  pubdoc["mode"] = state.mode;
  pubdoc["speed"] = state.speed;

  for (int i = 0; i < state.specialPointList.size(); i++)
  {
    pubdoc["specialPointList"][i] = state.specialPointList[i];
  }

  for (int i = 0; i < state.colorList.size(); i++)
  {
    pubdoc["colorList"][i]["r"] = state.colorList[i].red;
    pubdoc["colorList"][i]["g"] = state.colorList[i].green;
    pubdoc["colorList"][i]["b"] = state.colorList[i].blue;
  }

  serializeJson(pubdoc, pubJson);
  return pubJson;
}

void readJSON(String data)
{
  DynamicJsonDocument doc(1024);

  deserializeJson(doc, data);

  state.mode = doc["mode"];
  state.brightness = doc["brightness"];
  state.speed = doc["speed"];

  state.specialPointList = {};

  for (int i = 0; i < doc["specialPointList"].size(); i++)
  {
    state.specialPointList.push_back(doc["specialPointList"][i]);
  }

  state.colorList = {};

  for (int i = 0; i < doc["colorList"].size(); i++)
  {
    CRGB color(doc["colorList"][i]["r"], doc["colorList"][i]["g"], doc["colorList"][i]["b"]);
    state.colorList.push_back(color);
  }
}

// On arriving message:
void callback(char *topic, byte *message, unsigned int length)
{
  Serial.println("---------------------------------------");
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);

  String data;
  for (int i = 0; i < length; i++)
  {
    data += (char)message[i];
  }

  Serial.println("Arrived data: ");
  Serial.print(data);
  Serial.println();

  if (String(topic) == commandTopic)
  {
    readJSON(data);

    currentLED[0] = 0;
    currentLED[1] = 0;
    currentLED[2] = 0;
    currentLED[3] = 0;
    currentLED[4] = 0;

    writeToFile();
  }
  else if (String(topic) == requestTopic)
  {
    mqtt_publish_state();
  }
  else
  {
    Serial.println("Error: wrong topic");
  }

  Serial.println("---------------------------------------");
}

void loop()
{
  if (!pubSubClient.connected())
  {
    connect_mqtt();
  }
  pubSubClient.loop();

  unsigned long currentMillis = millis();
  if (currentMillis >= previousMillisMode + state.speed)
  {
    previousMillisMode = currentMillis;
    executeMode();
  }
}

void executeMode()
{
  if (state.mode == 1)
  {
    staticMode();
  }
  else if (state.mode == 2)
  {
    singleStripeMode();
  }
  else if (state.mode == 3)
  {
    gradientMode();
  }
  else if (state.mode == 4)
  {
    blinkMode();
  }
  else if (state.mode == 5)
  {
    swipeBlinkMode();
  }
  else if (state.mode == 6)
  {
    meetMode();
  }
  else if (state.mode == 7)
  {
    multiStaticColor();
  }
}

//------------------- Modes ---------------

void staticMode()
{
  if (currentLED[0] < NUM_LEDS)
  {
    leds[(int)currentLED[0]] = state.colorList[0];
    FastLED.show();
    currentLED[0]++;
  }
}

void gradientMode()
{
  fill_gradient_RGB(leds, 0, state.colorList[0], NUM_LEDS - 1, state.colorList[1]);
  FastLED.show();
}

void blinkMode()
{
  if (currentLED[0] != 0)
  {
    int random_integer;
    int lowest = 1, highest = state.colorList.size();
    int range = (highest - lowest) + 1;
    random_integer = lowest + rand() % range;
    fill_solid(leds, NUM_LEDS, state.colorList[random_integer]);
    currentLED[0] = 0;
  }
  else
  {
    fill_solid(leds, NUM_LEDS, state.colorList[0]); // First color
    currentLED[0] = 1;
  }
  FastLED.show();
}

void swipeBlinkMode()
{
  if (currentLED[0] < (sizeof(leds) / sizeof(*leds)))
  {
    leds[(int)currentLED[0]] = state.colorList[1];
    currentLED[0]++;
  }
  else
  {
    fill_solid(leds, NUM_LEDS, state.colorList[0]);
    currentLED[0] = 0;
  }
  FastLED.show();
}

void rainbowMode()
{
}

void meetMode()
{
  int meetingPoint = state.specialPointList[0];
  int speed = 30;
  int zeroToPoint = meetingPoint;
  int lastToPoint = NUM_LEDS - meetingPoint;

  double speedZeroToPoint = (double)zeroToPoint / speed;
  double speedLastToPoint = (double)lastToPoint / speed;

  if (currentLED[1] < meetingPoint)
  {
    resetStripeForMode();
  }

  int currentZeroToPoint = currentLED[0];
  int currentLastToPoint = currentLED[1];

  if (currentZeroToPoint <= meetingPoint)
  {
    leds[currentZeroToPoint] = state.colorList[0];
    currentLED[0] = currentLED[0] + speedZeroToPoint;
  }
  else
  {
    resetStripeForMode();
  }

  if (currentLastToPoint > meetingPoint)
  {
    leds[currentLastToPoint] = state.colorList[1];
    currentLED[1] = currentLED[1] - speedLastToPoint;
  }
  else
  {
    resetStripeForMode();
  }

  FastLED.show();
}

void resetStripeForMode()
{
  currentLED[0] = 0;
  currentLED[1] = NUM_LEDS;
  fill_solid(leds, NUM_LEDS, state.colorList[2]);
}

void singleStripeMode()
{
  int stripeSize = 10;
  int currentStripeTail = currentLED[0] - stripeSize;
  if (currentLED[0] < (NUM_LEDS + stripeSize))
  {
    if (currentLED[0] < NUM_LEDS)
    {
      leds[(int)currentLED[0]] = state.colorList[0];
    }

    if (currentStripeTail > 0)
    {
      leds[currentStripeTail] = state.colorList[1];
    }
    currentLED[0]++;
    FastLED.show();
  }
  else
  {
    currentLED[0] = 0;
  }
}

void multiStripeMode()
{
}

void starsMode()
{
}

void multiStaticColor()
{
  if (currentLED[0] < NUM_LEDS)
  {
    if (currentLED[0] < state.specialPointList[0])
    {
      leds[(int)currentLED[0]] = state.colorList[0];
    }
    
    for (int i = 0; i < state.specialPointList.size(); i++)
    {
      int point = state.specialPointList[i];
      int nextPoint = state.specialPointList[i+1];
      if (currentLED[0] >= point && currentLED[0] < nextPoint)
      {
        leds[(int)currentLED[0]] = state.colorList[i+1];
        break;
      }
    }
    
    FastLED.show();
    currentLED[0]++;
  }
}