#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include "secrets.h"
#include <DHT.h> //temperatura

// The MQTT topics that this device should publish/subscribe
#define AWS_IOT_PUBLISH_TOPIC   "sala07/qet_QoA"
#define AWS_IOT_SUBSCRIBE_TOPIC_AIRCONDITIONER "sala07/set_airconditioner"
#define AWS_IOT_SUBSCRIBE_TOPIC_TEMP "sala07/get_temperature"

#define DHTTYPE DHT11
#define DHTPIN 5

int currentTemp = 20;
int airOn = false;

WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);
DHT dht(DHTPIN, DHTTYPE);

int LED_temp_down = 18; 
int LED_temp_up = 23; 
int LED_air_on = 21;            /*LED pin defined*/
int Sensor_input_mq2 = 36;    /*Digital pin 5 for sensor qualidade do ar*/
int Sensor_input_mq7 = 34; 

void airconditioner(String &payload) {
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];

  if (strcmp(message, "1") == 0){
    airOn = true;
    digitalWrite (LED_air_on, HIGH) ; /*LED set HIGH */
  } else if (strcmp(message, "0") == 0){
    airOn = false;
    digitalWrite (LED_air_on, LOW) ;  /*LED set LOW if NO Gas detected */
  }
}

void setTemperature(String &payload) {
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];

  if (airOn) {
    int temp = atoi(message);
    if (temp > currentTemp){
      digitalWrite (LED_temp_up, HIGH) ; /*LED set HIGH */
      delay(2000);
      digitalWrite (LED_temp_up, LOW) ; /*LED set HIGH */
    } else {
      digitalWrite (LED_temp_down, HIGH) ;  /*LED set LOW if NO Gas detected */
      delay(2000);
      digitalWrite (LED_temp_down, LOW) ; /*LED set HIGH */
    }
    currentTemp = temp;
  }
}

void messageHandler(String &topic, String &payload) {
  // digitalWrite(buzzer, LOW);
  Serial.println("incoming: " + topic + " - " + payload);

  if (topic == AWS_IOT_SUBSCRIBE_TOPIC_AIRCONDITIONER) {
    airconditioner(payload);
  }
  if (topic == AWS_IOT_SUBSCRIBE_TOPIC_TEMP) {
    setTemperature(payload);
  }


}

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  // Create a message handler
  client.onMessage(messageHandler);

  Serial.print("Connecting to AWS IOT");

  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }

  if(!client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC_AIRCONDITIONER);
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC_TEMP);

  Serial.println("AWS IoT Connected!");
}

void setup() {
  Serial.begin(115200);
  connectAWS();
  pinMode(LED_air_on, OUTPUT);  /*LED set as Output*/
  pinMode(LED_temp_down, OUTPUT);  /*LED set as Output*/ 
  pinMode(LED_temp_up, OUTPUT);  /*LED set as Output*/ 

  dht.begin();
}

void publishMessage(StaticJsonDocument<200> message, String topic)
{
  char jsonBuffer[512];
  serializeJson(message, jsonBuffer); // print to client

  client.publish(topic, jsonBuffer);
}

void loop() {
  float temperature_Aout = dht.readTemperature();
  int sensor_Aout_mq2 = analogRead(Sensor_input_mq2);  /*Analog value read function*/
  int sensor_Aout_mq7 = analogRead(Sensor_input_mq7);

  StaticJsonDocument<200> message_air;
  message_air["Air quality Sensor:"] = sensor_Aout_mq2;

  Serial.print("Air quality Sensor: \t");  
  Serial.print(sensor_Aout_mq2);   /*Read value printed*/
  Serial.print("\t");
  if (sensor_Aout_mq2 > 350) {    /*if condition with threshold 1800*/
    message_air["status"] = "Gas";
    Serial.println("Gas");  
  }
  else {
    message_air["status"] = "No Gas";
    Serial.println("No Gas");
  }
  
  publishMessage(message_air, "sala07/set_QoA");
  client.loop();

  StaticJsonDocument<200> message_fire;
  message_fire["Fire Sensor:"] = sensor_Aout_mq7;

  Serial.print("Fire Sensor: \t \t");  
  Serial.print(sensor_Aout_mq7);   /*Read value printed*/
  Serial.print("\t");
  if (sensor_Aout_mq7 > 350) {    /*if condition with threshold 1800*/
    message_fire["status"] = "Fire";
    Serial.println("Fire");  
  }
  else {
    message_fire["status"] = "No Fire";
    Serial.println("No Fire");
  }
  publishMessage(message_fire, "sala07/set_fire");
  client.loop();

  StaticJsonDocument<200> message_temp;
  message_temp["Temperature Sensor:"] = temperature_Aout;
  Serial.print("Temperature Sensor: \t");  
  Serial.println(temperature_Aout);   /*Read value printed*/
  publishMessage(message_temp, "sala07/set_temperature");
  client.loop();

  delay(1000);

}