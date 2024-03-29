#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <sstream>
#include "local_config.h"

// Wifi config
const char* ssid     = h_wifi_ssid;
const char* password = h_wifi_password;
// MQTT config
const char* MQTT_HOST = h_mqttbroker_host;
const String deviceID = h_device_id;
const char* MQTT_CLIENT_ID = ("ESP8266Button" + deviceID).c_str();
const char* MQTT_USER = h_mqtt_user;
const char* MQTT_PASSWORD = h_mqtt_password;
const char* LOCATION = h_location;

#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "europe.pool.ntp.org"

const String pubTopic = "main/" +  deviceID + "/button_event";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

WiFiClient espClient;
PubSubClient pubSubClient(espClient);

void setup() {
  Serial.begin(115200);

  timeClient.begin();

  // setup environments
  setup_wifi();
  setup_mqtt();
}

void setup_wifi() {
  Serial.println("Connecting to WiFi: ");
  Serial.print(h_wifi_ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_mqtt() {
  Serial.println("Setting up MQTT: ");
  Serial.println(h_mqttbroker_host);
  pubSubClient.setServer(MQTT_HOST, 1883);
  Serial.println("Setup done MQTT");
}

void connect_mqtt() {
  Serial.println("Connecting to MQTT: ");
  Serial.println(h_mqttbroker_host);

  if (!pubSubClient.connected()) {
    Serial.println("MQTT connecting");
    while (!pubSubClient.connected()) {
      pubSubClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
      delay(1000);
      Serial.print("-");
    }

    Serial.println("");
  }
  Serial.println("MQTT Connected");
}

// { "timestamp": 342432, "location": "location", "isMotionEvent": false }
void mqtt_publish() {
  std::string data;
  DynamicJsonDocument doc(512);

  doc["timestamp"] = timeClient.getEpochTime();
  doc["location"] = LOCATION;
  doc["isMotionEvent"] = false;

  serializeJson(doc, data);

  pubSubClient.publish(pubTopic.c_str(), data.c_str());

  Serial.println("Published the following message:");
  Serial.println(data.c_str());
}

void loop() {
  if(!pubSubClient.connected()) {
    connect_mqtt();
  }
  pubSubClient.loop();

  timeClient.update();

  mqtt_publish();

  delay(1000);

  Serial.println("Going to deep sleep...");
  ESP.deepSleep(0);
}
