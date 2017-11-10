#include <ESP8266MQTTClient.h>
#include <ESP8266WiFi.h>
#include "DHT.h"

#define DHTPIN 2      // what digital pin DHT is connected to
#define RELAYPIN 10   // what digital pin RELAY is connected to

#define DHTTYPE DHT11   // DHT 11

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

DHT dht(DHTPIN, DHTTYPE);

MQTTClient mqtt;

const char* TOPIC_SETTINGS "/thermostat/config"
const char* TOPIC_DATA "/thermostat/sensor"
const char* WIFI_SSID "ssid"
const char* WIFI_PASS "pswd"


void initWifi() {
   Serial.print("Connecting to ");
   Serial.print(ssid);
   if (strcmp (WiFi.SSID(),ssid) != 0) {
       WiFi.begin(WIFI_SSID, WIFI_PASS);
       // WiFi fix: https://github.com/esp8266/Arduino/issues/2186
       WiFi.persistent(false);
       WiFi.mode(WIFI_OFF);
       WiFi.mode(WIFI_STA);
       WiFi.begin(WIFI_SSID, WIFI_PASS);
   }

   while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
   }
  Serial.print("\nWiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
}

String getTime() {
  WiFiClient client;
  while (!!!client.connect("google.com", 80)) {
    Serial.println("connection failed, retrying...");
  }

  client.print("HEAD / HTTP/1.1\r\n\r\n");
 
  while(!!!client.available()) {
     yield();
  }

  while(client.available()){
    if (client.read() == '\n') {   
      if (client.read() == 'D') {   
        if (client.read() == 'a') {   
          if (client.read() == 't') {   
            if (client.read() == 'e') {   
              if (client.read() == ':') {   
                client.read();
                String theDate = client.readStringUntil('\r');
                client.stop();
                return theDate;
              }
            }
          }
        }
      }
    }
  }
}


void updateConfig(String json_settings) {
  //TODO: update weekly time and temperature
  return;
}

boolean checkHeating(String time, float t) {
  //TODO: check if is needed to activate the relay and so powering on the heating system
  return true;
}

void setup() {
  Serial.begin(115200);
  initWifi();

  dht.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //topic, data, data is continuing
  mqtt.onData([](String topic, String data, bool cont) {
    Serial.printf("Data received, topic: %s, data: %s\r\n", topic.c_str(), data.c_str());
    if(topic.c_str() == TOPIC_SETTINGS)
      updateConfig(data.c_str());
  });

  mqtt.onSubscribe([](int sub_id) {
    Serial.printf("Subscribe topic id: %d ok\r\n", sub_id);
  });
  
  mqtt.onConnect([]() {
    Serial.printf("MQTT: Connected\r\n");
    Serial.printf("Subscribe id: %d\r\n", mqtt.subscribe(TOPIC_SETTINGS));
  });

  mqtt.begin("mqtt://test.mosquitto.org:1883");

  mqtt.handle();

  //TODO: read data from DHT11 sensor 
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  String json="{ \"Temp\": " + t + " , \"Hum\": " + h ;
  
  //TODO: get time
  String time=getTime();
    json+=" , \"time\": " + time;

  //TODO: update relay status ON/OFF based on weekly temperature
  boolean heating = checkHeating(time, t);
  json+=" , \"heating\": \"" (heating) ? "ON" : "OFF" "\" }";
  
  //TODO: publish data
  mqtt.publish(TOPIC_DATA, json , 0, 0);
  
  //TODO: some sort of low power mode -> sleep for n seconds
  Serial.println("Going into deep sleep for 100 seconds");
  ESP.deepSleep(100e6); // 100e6 is 100 seconds

}

void loop() {
}
