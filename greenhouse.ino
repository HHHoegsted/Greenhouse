
#include "DHT.h"
#include "WiFi.h"
#include "PubSubClient.h"

#define DHTPIN 25
#define DHTTYPE 22
#define MQTT_BROKER "192.168.1.124"
#define MQTT_PORT 1883
#define MQTT_HEALTH "greenhouse/health"
#define MQTT_TEMP "greenhouse/temp"
#define MQTT_HUM "greenhouse/hum"
#define MQTT_MOIST "greenhouse/moisture"

const char* ssid        = "*****";
const char* password    = "*****";

//Time between readings/actions in milliseconds
unsigned long soilmoistureTime = 120000;    // 2min
unsigned long temperatureTime = 60000;      // 1min
unsigned long wateringTime = 10000;         // 10sec
unsigned long fanTime = 10000;              // 10sec

// Desired values of temperature and soilmoisture
float maxTemperature = 25.0;    //degrees Celsius
float minSoilmoisture = 40.0;   //percent soil moisture

//Keeping track of time
unsigned long currentTime;
unsigned long upTime;

//Variables regarding relays
unsigned long fanOnTime, pumpOnTime;
bool fanOn = false;
bool pumpOn = false;

//Variables regarding temperature and air humidity
DHT dht(DHTPIN, DHTTYPE);
const int fanPin = 16;
unsigned long lastTemperature = 0;
float temperature, humidity;
bool tooHot = false;

//Variables regarding soil moisture
const int soilPin_1 = 27;
const int pumpPin = 17;
int soilValue = 0;
int airValue = 2630;
int waterValue = 1120;
float soilPercent;
unsigned long lastSoilMoisture = 0;
bool soilDry = false;

//Variables regarding MQTT
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {

  // pinmodes
  pinMode(soilPin_1, INPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(pumpPin, OUTPUT);

  //Serial connection for debugging
  Serial.begin(115200);

  //Wifi and MQTT
  connectToWifi();
  client.setServer(MQTT_BROKER, MQTT_PORT);

  //Initiate sensors
  dht.begin();
}

void connectToWifi(){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(2000);

  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
  }
}

void reconnectMQTT(){
  int counter = 0;
  
  while(!client.connected()){
    if(counter == 5){
      ESP.restart();
    }
  
    counter+=1;
  
    if(!client.connect("greenhouse")){
      delay(500);
    }
  }
  client.publish(MQTT_HEALTH, "Connected sensors", true);
}

float getSoilPercent() {
  soilValue = analogRead(soilPin_1);
  Serial.println(soilValue);
  return map(soilValue, airValue, waterValue, 0, 100);
}

void turnOnPump(){
  digitalWrite(pumpPin, HIGH);
  pumpOn = true;
}

void turnOffPump(){
  soilDry = false;
  digitalWrite(pumpPin, LOW);
  pumpOn = false;
}

void adjustSoilmoisture(){
  if(soilDry && !pumpOn){
    turnOnPump();
    pumpOnTime = currentTime;
  }

  if(pumpOn && (currentTime > pumpOnTime + wateringTime)){
    turnOffPump();
  }
}

void displaySoilReadings(float soilPercent) {
  Serial.print(F("soil moisture: "));
  Serial.println(soilPercent);

  client.publish(MQTT_MOIST, String(soilPercent).c_str(),true);
}

void runSoilMoistureReading(unsigned long time) {
  if (time - lastSoilMoisture > soilmoistureTime) {
    soilPercent = getSoilPercent();
    displaySoilReadings(soilPercent);
    if (soilPercent < minSoilmoisture) {
      soilDry = true;
    }
    lastSoilMoisture = time;
  }
}

void turnOnFan(){
  digitalWrite(fanPin, HIGH);
  fanOn = true;
}

void turnOffFan(){
  tooHot = false;
  digitalWrite(fanPin, LOW);
  fanOn = false;
}

void adjustTemperature(){
    if(tooHot && !fanOn){
    turnOnFan();
    fanOnTime = currentTime;
  }
  
  if(fanOn && (currentTime > fanOnTime + fanTime)){
    turnOffFan();
  }
}

void displayTemperatureReadings(float temperature, float humidity){
    Serial.print(F("temperature: "));
    Serial.println(temperature);

    Serial.print(F("humidity: "));
    Serial.println(humidity);

    client.publish(MQTT_TEMP, String(temperature).c_str(),true);
    client.publish(MQTT_HUM, String(humidity).c_str(),true);
}

void runTemperatureReading(unsigned long time) {
  if (time - lastTemperature > temperatureTime) {
    
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    
    displayTemperatureReadings(temperature, humidity);
    
    if (temperature > maxTemperature) {
      tooHot = true;
    } 
    lastTemperature = time;
  }
}

void loop() {

  if(!client.connected()){
    reconnectMQTT();
  }
  
  currentTime = millis();

  runSoilMoistureReading(currentTime);
  runTemperatureReading(currentTime);
  adjustTemperature();
  adjustSoilmoisture();
  client.loop();
  
}
