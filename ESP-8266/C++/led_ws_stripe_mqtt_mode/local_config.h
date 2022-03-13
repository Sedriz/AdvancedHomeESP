// Wifi:
#define h_wifi_ssid "<Your Network SSID>"
#define h_wifi_password "<Your Network Password>"

// MQTT:
#define h_mqttbroker_host "<The IP of your MQTT broker>"
#define h_mqtt_user "<The Username to connect to your MQTT broker>"
#define h_mqtt_password "<The Password to conenct to your MQTT broker>"

//Device:
#define h_device_id "<Your Device ID. You have to register you device to (Smartboot) to get your ID>"
#define h_location "<The location your Device is registered in>"

//Stripe:
#define LED_PIN 4 // The output pin where your LED-Stripe is connected. 4 -> D2
#define NUM_LEDS 256 //The length of your Stripe 
#define LED_TYPE WS2813 // The model of your LED-Stripe
#define COLOR_ORDER GRB // RGB / GRB

//Timeclient:
#define NTP_OFFSET 60 * 60     // In seconds
#define NTP_INTERVAL 60 * 1000 // In miliseconds
#define NTP_ADDRESS "europe.pool.ntp.org"

