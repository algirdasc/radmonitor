/*
J305 Geiger Tube Specification:
Tin oxide Cathode, Coaxial cylindrical thin shell structure(Wall density 50±10cg/cm2),Application of pulse type halogen tube
application temperature:-40°C~55°C
Could be used for :γRay 20mR/h~120mR/h  
               and β Ray in range  100~1800 ChangingIndex/minutes·CM2 soft β Ray
               (Both beta and gamma radiation detetion)
Working Voltage: 380-450V 
Working Current: 0,015-0,02 mA
Sensivity to Gamma Radiation: 0.1 MeV
Own Background: 0,2 Pulses/s or 0,2 * 60 = 12 Pulses/min
 */

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"

volatile int IRQCount;
static unsigned long cpm = 0;
static unsigned long lastTimeReported = 0;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// LED ON function
void LEDOn()
{
  digitalWrite(LED_BUILTIN, LOW);
}

// LED OFF function
void LEDOff()
{
  digitalWrite(LED_BUILTIN, HIGH);
}

// IRQ Counter
ICACHE_RAM_ATTR void IRQcounter() 
{    
  IRQCount++; 
  Serial.println(IRQCount);
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{

}

// Setup
void setup() {  
  Serial.begin(115200);
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  WiFi.mode(WIFI_STA);
  WiFi.hostname("radiation.lan");    
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  
  Serial.println();
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");      
    LEDOn();      
    delay(500);
    LEDOff();    
    delay(500);
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println();
  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(mqttCallback);

  attachInterrupt(digitalPinToInterrupt(D7), IRQcounter, FALLING);

  // OTA stuff
  ArduinoOTA.setHostname("radiation.lan");
  ArduinoOTA.setPassword("pripyat2019");

  ArduinoOTA.onStart([] () {
    LEDOn();
    Serial.println("Start OTA");
  });

  ArduinoOTA.onEnd([] () {
    LEDOff();
    Serial.println("\nEnd OTA");
  });

  ArduinoOTA.onProgress([] (unsigned int progress, unsigned int total) {    
    LEDOn();
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    LEDOff();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}

void mqttReconnect()
{  
  Serial.println("Attempting MQTT connection");
  const char* clientId = "radmonitor";
  const char* willTopic = "radiation/availability";
  const char* willPayload = "ON";

  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientId, mqttUser, mqttPassword, willTopic, 0, 0, willPayload)) {
      Serial.println("MQTT connected!");      
    } else {      
      Serial.print('.');
      LEDOn();      
      delay(1000);
      LEDOff();    
      delay(1000);
    }
  }
}

void loop() {
  unsigned long currentMillis = millis();

  ArduinoOTA.handle();  

  if (!mqttClient.connected())
  {
    mqttReconnect();
  }

  mqttClient.loop();

  if (currentMillis - lastTimeReported >= LOG_PERIOD) 
  {       
    lastTimeReported = millis();
    unsigned long origcpm = IRQCount * MINUTE_PERIOD / LOG_PERIOD;

    // Subtract own background
    cpm = origcpm - CPM_OFFSET * (LOG_PERIOD / 10000);

    IRQCount = 0;

    DynamicJsonDocument JSONDoc(1024);    
 
    // Light sensor
    int lightSensorValue = analogRead(A0);

    JSONDoc["cpm"] = cpm;
    JSONDoc["origcpm"] = origcpm;
    JSONDoc["usvph"] = cpm * CONV_FACTOR;
    JSONDoc["light"] = lightSensorValue * (5.0 / 1023.0); // Voltage
    JSONDoc["rssi"] = WiFi.RSSI();

    String payload;

    serializeJson(JSONDoc, payload);

    mqttClient.publish("radiation", (char*) payload.c_str());    
    Serial.println(payload);    

    LEDOn();
    delay(500);
    LEDOff();
  }
}