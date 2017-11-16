#include <ESP8266MQTTClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "DHT.h"

#define DHTPIN 2      // what digital pin DHT is connected to
#define RELAYPIN 10   // what digital pin RELAY is connected to

#define DHTTYPE DHT11   // DHT 11
#define GMT 1

const float HYSTERESIS=1.0; // hysteresis to take in account for switching the heating -> 18 temp setted, hysteresis 2 -> temp: 16 -> switch on, until temp is 20, temp 20 -> switch of , temp: 16 -> switch on
const float OFF_TEMP=-127.0;

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

DHT dht(DHTPIN, DHTTYPE);

MQTTClient mqtt;

const char* TOPIC_SETTINGS="/thermostat/config";
const char* TOPIC_DATA="/thermostat/sensor";
const char* WIFI_SSID="ssid";
const char* WIFI_PASS="pswd";

typedef struct {
  int hours[2]; //ora inizio (indice 0) e ora dine (indice 1)
  float t;      //temperatura per il range di ore
} *hour_dwp;

typedef struct {
  hour_dwp *hours_intervals;
  int n;
} *day_wp;

typedef struct {
  day_wp days[7];
} *week_program;

struct {
  int dof, h, m;
} time_struct;

int mode, temp, c;
boolean relay;
week_program wp=NULL;


void initWifi() {
   Serial.print("Connecting to ");
   Serial.print(WIFI_SSID);
   WiFi.mode(WIFI_STA);
   //wifi_set_sleep_type(LIGHT_SLEEP_T);
   if (WiFi.SSID() != WIFI_SSID) {
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

void free_wp() {
  if(wp==NULL) return;
  int i,j;
  //TODO: free wp structure
  for(i=0;i<7;i++) {
    if(wp->days[i]==NULL) continue;
    for(j=0;j<wp->days[i]->n;j++)
      free((void*)((hour_dwp)(((day_wp)wp->days[i])->hours_intervals[j])));
    free(wp->days[i]->hours_intervals);
    free(wp->days[i]);
  }
  free(wp);
}


void updateConfig(String json_settings) {
  free_wp();
  int i,j,n;
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json_settings);
  //JsonObject hour_entry;
//  JsonArray& days, hours;
  if (!root.success())
  {
    Serial.println("parseObject() failed");
    return;
  }
  if(root["mode"] == "OFF") {
    mode=0;
    temp=root["t"];
  } else if(root["mode"] == "ON") {
    mode=1;
    temp=OFF_TEMP;
  } else {
    mode=2;
    wp=(week_program)malloc(sizeof(*wp));
    JsonArray& days=root["days"];
    for(i=0;i<7;i++) {
      JsonArray& hours=days[i];
      n=hours.size();
      if(n!=0)
        wp->days[i]=(day_wp)malloc(sizeof(*(wp->days[i])));
      else {
        wp->days[i]=NULL;
        continue;
      }
      wp->days[i]->n=n;
      wp->days[i]->hours_intervals=(hour_dwp*)malloc(sizeof(*(wp->days[i]->hours_intervals)) * n);
      for(j=0;j<n;j++) {
        JsonObject& hour_entry=hours[j];
        wp->days[i]->hours_intervals[j]->t=hour_entry["t"];
        wp->days[i]->hours_intervals[j]->hours[0]=hour_entry["h_i"];
        wp->days[i]->hours_intervals[j]->hours[1]=hour_entry["h_e"];
      }
    }
  }
  return;
}

void set_hour_from_time(String times) {
  time_struct.h=(times.substring(17,19).toInt()) + GMT;
  time_struct.m=(times.substring(20,22).toInt());
  return;
}

void set_day_of_the_week_from_time(String times) {
  String t = times.substring(0,3);
  if(t=="Mon") time_struct.dof=0;
  else if(t=="Tue") time_struct.dof=1;
  else if(t=="Wed") time_struct.dof=2;
  else if(t=="Thu") time_struct.dof=3;
  else if(t=="Fri") time_struct.dof=4;
  else if(t=="Sat") time_struct.dof=5;
  else if(t=="Sun") time_struct.dof=6;
  return;
}

void set_relay(boolean value) {
  relay=value;
  if(value)
    digitalWrite(RELAYPIN, HIGH);
  else
    digitalWrite(RELAYPIN, LOW);
  return;
}

float get_temp_from_time() {
  if(mode!=2) return temp;
  int i;
  day_wp d=wp->days[time_struct.dof];
  hour_dwp hdwp;
  if(d==NULL) return OFF_TEMP;
  for(i=0;i<d->n;i++) {
    hdwp=d->hours_intervals[i];
    if((time_struct.h >= hdwp->hours[0]) && (time_struct.h <= hdwp->hours[1]))
      return hdwp->t;
  }
  return OFF_TEMP;
}

void checkHeating(float t) {
  if( ( t < (get_temp_from_time() - HYSTERESIS) ) || ( relay && ( t <= (get_temp_from_time() + HYSTERESIS) ) ) )
    set_relay(true);
  else
    set_relay(false);
  return;
}

void setup() {
  time_struct.dof=0;
  time_struct.h=0;
  time_struct.m=0;
  set_relay(false);
  Serial.begin(115200);
  initWifi();

  dht.begin();

  mode=0; //start with OFF
  
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

  String times=getTime();
  set_hour_from_time(times);
  set_day_of_the_week_from_time(times);

}

void loop() {
  mqtt.handle();

  //TODO: read data from DHT11 sensor 
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  String json="{ \"Temp\": " + String(t) + " , \"Hum\": " + String(h) ;
  
  //TODO: get time
  String time=getTime();
    json+=" , \"time\": " + time;

  //TODO: update relay status ON/OFF based on weekly temperature
  checkHeating(t);
  if(relay)
    json+=" , \"heating\": \"ON\" }";
   else
    json+=" , \"heating\": \"OFF\" }";
  
  //TODO: publish data
  mqtt.publish(TOPIC_DATA, json , 0, 0);

  
  //TODO: some sort of low power mode -> sleep for n seconds
  Serial.println("Going into light sleep for 60 seconds");
  delay(60e3);
  //update time info
  if((++c)<=60){
    if(++(time_struct.m)>=60) {
      if(++(time_struct.h)>=24) {
        time_struct.dof=(++time_struct.dof)%7;
        time_struct.m=time_struct.m%60;
        time_struct.h=time_struct.h%24;
      }
    }
  } else {
    String times=getTime();
    set_hour_from_time(times);
    set_day_of_the_week_from_time(times);
  }
}
