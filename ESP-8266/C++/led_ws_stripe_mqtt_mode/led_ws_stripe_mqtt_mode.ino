#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <iostream>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <sstream>
#include <FastLED.h>
#include "local_config.h"
#include "State.h"

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
int currentLED = 0;

//-----------------------------------------------------------

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL); // Vector. Bisher nur vom ertsen genommen. Bei callback auch nur das erste gelsen. Fehler beim LEsen vermutlich nicht richtig einlesen. Color wird jedes mal neu erstellt schaurn wegen destroy
WiFiClient espClient;
PubSubClient pubSubClient(espClient);
CRGB leds[NUM_LEDS];

void setup()
{
  Serial.begin(19200);
  Serial.println("Setting up!");
  timeClient.begin();

  // setup environments
  setup_mode();
  setup_fastLED();
  setup_wifi();
  setup_mqtt();

  // Send online state
  delay(1000);
  mqtt_publish_state();
}

void setup_mode()
{
  CRGB color1(5,5,5);
  CRGB color2(50,0,0);

  state.mode = 'S';
  state.brightness = 100;
  state.speed = 100;
  state.colorList = {color1, color2};
}

void setup_fastLED()
{
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
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

void mqtt_publish_state()
{
  if (!pubSubClient.connected())
  {
    connect_mqtt();
  }

  timeClient.update();
  DynamicJsonDocument pubdoc(1024);
  String pubJson;

  pubdoc["timestamp"] = timeClient.getEpochTime();
  pubdoc["mode"] = state.mode;
  pubdoc["speed"] = state.speed;

  // for (int i = 0; i < state.colorList.size(); i++)
  // {
  //   pubdoc["colorList"][i]["r"] = state.colorList[i].red; //do
  //   pubdoc["colorList"][i]["g"] = state.colorList[i].green;
  //   pubdoc["colorList"][i]["b"] = state.colorList[i].blue;
  // }

  serializeJson(pubdoc, pubJson);

  pubSubClient.publish(pubTopic.c_str(), pubJson.c_str());

  Serial.println();
  Serial.println("Pushed State to topic:");
  Serial.println(pubTopic);
  Serial.println();
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

  DynamicJsonDocument doc(1024);

  deserializeJson(doc, data);

  if (String(topic) == commandTopic)
  {
    state.mode = doc["mode"];
    state.brightness = doc["brightness"];
    state.speed = doc["speed"];

    state.colorList = {};

    for (int i = 0; i < doc["colorList"].size(); i++)
    {
      CRGB color(doc["colorList"][0]["r"], doc["colorList"][0]["g"], doc["colorList"][0]["b"]);
      state.colorList.push_back(color);
    }

    currentLED = 0;
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

  // Serial.println(currentLED);
}

void executeMode()
{
  if (state.mode == 'S')
  {
    staticMode();
  }
  else if (state.mode == '1')
  {
    singleStripeMode();
  }
  else if (state.mode == 'G')
  {
    gradientMode();
  }
}

//------------------- Modes ---------------

void staticMode() // 83
{
  if (currentLED < (sizeof(leds) / sizeof(*leds)))
  {
    leds[currentLED] = state.colorList[0];
    FastLED.show();
    currentLED++;
  }
}

void gradientMode() //71
{
  if (currentLED < (sizeof(leds) / sizeof(*leds)))
  {
    fill_gradient_RGB(leds, 0, 
      state.colorList[0], NUM_LEDS-1, state.colorList[1]
    );
    FastLED.show();
    currentLED++;
  }
}

void blinkMode()
{
}

void rainbowMode()
{
}

void meetMode()
{
}

void singleStripeMode() // 49
{
  int ledsSize = (sizeof(leds) / sizeof(*leds));
  int stripeSize = 5;
  int currentStripeTail = currentLED - stripeSize;
  if (currentLED < (ledsSize + stripeSize))
  {
    if (currentLED < ledsSize)
    {
      leds[currentLED] = state.colorList[0];
    }

    if (currentStripeTail > 0)
    {
      leds[currentStripeTail] = state.colorList[1];
    }
    currentLED++;
    FastLED.show();
  }
  else
  {
    currentLED = 0;
  }
}

void multistripeMode()
{
}

void startsMode()
{
}