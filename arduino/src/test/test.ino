#include <ESP8266MQTTClient.h>
#include <ESP8266WiFi.h>
//TODO: find labrary for esp or adafruit library work also for esp8266???

#define DHTPIN            2         // Pin which is connected to the DHT sensor.
// Uncomment the type of sensor in use:
#define DHTTYPE           DHT11     // DHT 11 
DHT_Unified dht(DHTPIN, DHTTYPE);
MQTTClient mqtt;

#define TOPIC_SETTINGS "/thermostat/config"
#define TOPIC_DATA "/thermostat/sensor"

void updateConfig(String json_settings) {
  //TODO: update weekly time and temperature
  return;
}

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
//  mqtt.begin("mqtt://test.mosquitto.org:1883", {.lwtTopic = "hello", .lwtMsg = "offline", .lwtQos = 0, .lwtRetain = 0});
//  mqtt.begin("mqtt://user:pass@mosquito.org:1883");
//  mqtt.begin("mqtt://user:pass@mosquito.org:1883#clientId");

}

void loop() {
  mqtt.handle();

  //TODO: read data from DHT11 sensor
  // Get temperature event and print its value.
  String json="{";
  boolean error=false;
  sensors_event_t event;  

  // Get temperature event
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
    error=true;
  }
  else {
    Serial.print("Temperature: ");
    Serial.print(event.temperature);
    Serial.println(" *C");
    json += " \"temp\": " + event.temperature;
  }
  
  // Get humidity event
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity) || error) {
    Serial.println("Error reading humidity!");
    error=true;
  }
  else {
    Serial.print("Humidity: ");
    Serial.print(event.relative_humidity);
    Serial.println("%");
    json += " , \"hum\": " + event.relative_humidity;
  }
  
  //TODO: get time
  if(!error)
    json+=" , \"time\": \"20171011-19:12:00\""

  //TODO: update relay status ON/OFF based on weekly temperature
  if(!error)
    json+=" , \"heating\": \"ON\"";


  if(error)
    json="{ \"Success\": " + false + "}";
  else
    json+=" , \"Success\": " + true + "}";
  
  //TODO: publish data
  mqtt.publish(TOPIC_DATA, json , 0, 0);
  
  //TODO: some sort of low power mode -> sleep for n seconds

}
