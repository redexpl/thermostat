#include <ESP8266MQTTClient.h> //TODO: change this with my forked version
#include <ESP8266WiFi.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN            2         // Pin which is connected to the DHT sensor.
// Uncomment the type of sensor in use:
#define DHTTYPE           DHT11     // DHT 11 
DHT_Unified dht(DHTPIN, DHTTYPE);
MQTTClient mqtt;

void setup() {
  Serial.begin(115200);
  WiFi.begin("ssid", "pass");

  dht.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //topic, data, data is continuing
  mqtt.onData([](String topic, String data, bool cont) {
    //TODO: update weekly time and temperature
    Serial.printf("Data received, topic: %s, data: %s\r\n", topic.c_str(), data.c_str());
    mqtt.unSubscribe("/qos0");
  });

  mqtt.onSubscribe([](int sub_id) {
    Serial.printf("Subscribe topic id: %d ok\r\n", sub_id);
    mqtt.publish("/qos0", "qos0", 0, 0);
  });
  
  mqtt.onConnect([]() {
    Serial.printf("MQTT: Connected\r\n");
    Serial.printf("Subscribe id: %d\r\n", mqtt.subscribe("/qos0", 0));
//    mqtt.subscribe("/qos1", 1);
//    mqtt.subscribe("/qos2", 2);
  });

  mqtt.begin("mqtt://test.mosquitto.org:1883");
//  mqtt.begin("mqtt://test.mosquitto.org:1883", {.lwtTopic = "hello", .lwtMsg = "offline", .lwtQos = 0, .lwtRetain = 0});
//  mqtt.begin("mqtt://user:pass@mosquito.org:1883");
//  mqtt.begin("mqtt://user:pass@mosquito.org:1883#clientId");

}

void loop() {
  mqtt.handle();

  //TODO: read data from DHT11 sensor
  // Get temperature event and print its value.
  sensors_event_t event;  
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
  }
  else {
    Serial.print("Temperature: ");
    Serial.print(event.temperature);
    Serial.println(" *C");
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println("Error reading humidity!");
  }
  else {
    Serial.print("Humidity: ");
    Serial.print(event.relative_humidity);
    Serial.println("%");
  }
  
  //TODO: get time

  //TODO: update relay status ON/OFF based on weekly temperature

  //TODO: publish data

  //TODO: some sort of low power mode -> sleep for n seconds

}
