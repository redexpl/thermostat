#include <PubSubClient.h> 
#include <stdlib.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "DHT.h"

#define DHTPIN 5      // what digital pin DHT is connected to
#define RELAYPIN 16   // what digital pin RELAY is connected to

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

const char* TOPIC_SETTINGS="/thermostat/config";
const char* TOPIC_DATA="/thermostat/status";
const char* WIFI_SSID="TP-LINK_......";
const char* WIFI_PASS=".....";

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
char json[100];

const char *mqtt_server = "....cloudmqtt.com";
const int mqtt_port = 15739;
const char *mqtt_user = "thermostat";
const char *mqtt_pass = ".........";
const char *mqtt_client_name = "Thermostat"; // Client connections cant have the same connection name
 

WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived:");
  if(!strcmp(topic, TOPIC_SETTINGS)) {
    Serial.println("Message arrived:");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }
}

void reconnect() {
  Serial.println("Reconnecting...");
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("Thermostat-esp8266", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // ... and resubscribe
      Serial.println(client.subscribe(TOPIC_SETTINGS));
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      //delay(5000);
    }
  }
}


void initWifi() {
   Serial.print("Connecting to ");
   Serial.println(WIFI_SSID);
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
  while (!!!espClient.connect("google.com", 80)) {
    Serial.println("connection failed, retrying...");
  }

  espClient.print("HEAD / HTTP/1.1\r\n\r\n");
 
  while(!!!espClient.available()) {
     yield();
  }

  while(espClient.available()){
    if (espClient.read() == '\n') {   
      if (espClient.read() == 'D') {   
        if (espClient.read() == 'a') {   
          if (espClient.read() == 't') {   
            if (espClient.read() == 'e') {   
              if (espClient.read() == ':') {   
                espClient.read();
                String theDate = espClient.readStringUntil('\r');
                espClient.stop();
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
  Serial.println("UC 1");
  //JsonObject hour_entry;
//  JsonArray& days, hours;
  if (!root.success())
  {
    Serial.println("parseObject() failed");
    return;
  }
  Serial.println("UC 2");
  if(root["mode"] == "OFF") {
    mode=0;
    temp=root["t"];
  } else if(root["mode"] == "ON") {
    mode=1;
    temp=OFF_TEMP;
  } else {
    Serial.println("UC 3");
    mode=2;
    wp=(week_program)malloc(sizeof(*wp));
    JsonArray& days=root["days"];
    Serial.println("UC 4");
    for(i=0;i<7;i++) {
      JsonArray& hours=days[i];
      n=hours.size();
      Serial.print("UC 4 - ");
      Serial.println(n);
      if(n!=0)
        wp->days[i]=(day_wp)malloc(sizeof(*(wp->days[i])));
      else {
        wp->days[i]=NULL;
        continue;
      }
      wp->days[i]->n=n;
      wp->days[i]->hours_intervals=(hour_dwp*)malloc(sizeof(hour_dwp) * n);
      for(j=0;j<n;j++) {
        JsonObject& hour_entry=hours[j];
        Serial.print("UC 5 - ");
        hour_entry.printTo(Serial);
        Serial.println("");
        wp->days[i]->hours_intervals[j]=(hour_dwp)malloc(sizeof(*(wp->days[i]->hours_intervals[j])));
        (wp->days[i]->hours_intervals[j])->t=hour_entry["t"];
        Serial.println("UC 6");
        wp->days[i]->hours_intervals[j]->hours[0]=hour_entry["h_i"];
        Serial.println("UC 7"); 
        wp->days[i]->hours_intervals[j]->hours[1]=hour_entry["h_e"];
        Serial.println("UC 8");
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
  Serial.print("DOF: ");
  Serial.println(time_struct.dof);
  day_wp d=wp->days[time_struct.dof];
  hour_dwp hdwp;
  if(d==NULL) return OFF_TEMP;
  for(i=0;i<d->n;i++) {
    hdwp=d->hours_intervals[i];
    Serial.print("Hour: ");
    Serial.print(time_struct.h);
    Serial.print(" >= ");
    Serial.print(hdwp->hours[0]);
    Serial.print(" <= ");
    Serial.print(hdwp->hours[1]);
    Serial.print(" @ ");
    Serial.println(hdwp->t);
    if((time_struct.h >= hdwp->hours[0]) && (time_struct.h <= hdwp->hours[1]))
      return hdwp->t;
  }
  return OFF_TEMP;
}

void checkHeating(float t) {
  Serial.print("Temp from time: ");
  Serial.println(get_temp_from_time());
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
  Serial.begin(9600);
  initWifi();
  client.setServer(mqtt_server, 15739);
  client.setCallback(callback);

  dht.begin();

  mode=0; //start with OFF
  

  String times=getTime();
  set_hour_from_time(times);
  set_day_of_the_week_from_time(times);
  Serial.println("Time: " + times);

  Serial.println("Trying taking config");
  updateConfig("{ \"mode\": \"WP\", \"days\": [[{\"t\": 27.0, \"h_i\":8, \"h_e\": 22}],[{\"t\": 27.0, \"h_i\":8, \"h_e\": 22}],[{\"t\": 27.0, \"h_i\":8, \"h_e\": 22}],[{\"t\": 27.0, \"h_i\":8, \"h_e\": 22}],[{\"t\": 27.0, \"h_i\":8, \"h_e\": 22}],[{\"t\": 19.0, \"h_i\":8, \"h_e\": 15}, {\"t\": 25.0, \"h_i\":16, \"h_e\": 22}],[{\"t\": 22.0, \"h_i\":8, \"h_e\": 22}]] }");
  checkHeating(21.0);
}

void loop() {
  Serial.println("Going into light sleep for 60 seconds");
  delay(59900);
  if (WiFi.status() != WL_CONNECTED)
    initWifi();
    
  if(!client.connected())
    reconnect();

  delay(100);
  client.loop();
    
  //TODO: read data from DHT11 sensor 
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  Serial.print("Hum: ");
  Serial.println(h);
  Serial.print("Temp: ");
  Serial.println(t);
  
  //TODO: get time
  Serial.print("Day of time: ");
  Serial.println(time_struct.dof);
  Serial.print("Hour: ");
  Serial.println(time_struct.h);
  Serial.print("Minute: ");
  Serial.println(time_struct.m);

  //TODO: update relay status ON/OFF based on weekly temperature
  checkHeating(t);
  if(relay)
    sprintf(json,"{ \"Temp\": %d.%02d , \"Hum\": %d.%02d , \"heating\": \"%s\" }",(int)t, ((int)(t*100)%100),(int)h, ((int)(h*100)%100),"ON");
   else
    sprintf(json,"{ \"Temp\": %d.%02d , \"Hum\": %d.%02d , \"heating\": \"%s\" }",(int)t, ((int)(t*100)%100),(int)h, ((int)(h*100)%100),"OFF");

  Serial.println(json);
  //TODO: publish data
  Serial.println("Publishing data");
  client.publish(TOPIC_DATA, json, true);

  //TODO: some sort of low power mode -> sleep for n seconds
  //update time info
  if((++c)<=10){
    if(++(time_struct.m)>=60) {
      if(++(time_struct.h)>=24) {
        time_struct.dof=(++time_struct.dof)%7;
      }
      time_struct.m=time_struct.m%60;
      time_struct.h=time_struct.h%24;
    }
  } else {
    c=0;
    String times=getTime();
    set_hour_from_time(times);
    set_day_of_the_week_from_time(times);
    Serial.println("Time: " + times);
  }
  client.unsubscribe(TOPIC_SETTINGS);
  client.disconnect();
}
