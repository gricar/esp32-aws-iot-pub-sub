#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"
 
unsigned long lastMillis = 0;
unsigned long previousMillis = 0;
const long interval = 5000;
 
#define AWS_IOT_PUBLISH_TOPIC   "esp8266/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266/sub"
 
WiFiClientSecure net;
 
BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);
 
PubSubClient client(net);

// =======================================================================
// --- Variáveis Globais ---
const char* AP_SSID = "ESP_AcessPoint";       
const char* AP_PWD = "123456789"; 
time_t now;
time_t nowish = 1510592825;
bool ledStatus = false;
bool ledBuiltinStatus = false;


// =======================================================================
// --- Mapeamento de Hardware ---
#define  ledPin   D1
 
void setup()
{
  pinMode(ledPin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(ledPin, LOW);
  digitalWrite(LED_BUILTIN, HIGH);
  
  Serial.begin(115200);
  delay(10);
  
  wifiConnection();
  connectAWS();
}
 
 
void loop()
{
  Serial.println("ESP is running");

  delay(2000);
 
  now = time(nullptr);
 
  if (!client.connected())
  {
    connectAWS();
  }
  else
  {
    client.loop();
    if (millis() - lastMillis > 10000)  // a cada 10 s é enviado o dado JSON
    {
      lastMillis = millis();
      // publishMessage();
    }
  }
}


// =======================================================================
// --- Desenvolvimento das Funções ---
void wifiConnection()
{
  WiFiManager wm;

  //wm.resetSettings();

  bool res = wm.autoConnect(AP_SSID, AP_PWD);

  if(res) Serial.println("Successfully connected! :)");
  else    Serial.println("Failed to connect");
}


void NTPConnect(void)
{
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
 
 
void messageReceived(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]); //imprimir conteudo do payload
  }

  // Parse incoming JSON payload
  DynamicJsonDocument doc(256);
  deserializeJson(doc, payload, length);

  // Set LED status based on received payload
  ledStatus = doc["led_status"];
  ledBuiltinStatus = doc["LED_BUILTIN_status"];
  digitalWrite(ledPin, ledStatus);
  digitalWrite(LED_BUILTIN, ledBuiltinStatus);


  bool getStatus = doc["getStatus"];

  if(getStatus) publishMessage();
}

 
void publishMessage()
{
  ledStatus = digitalRead(ledPin);
  ledBuiltinStatus = digitalRead(LED_BUILTIN);

  // Create JSON payload
  DynamicJsonDocument doc(256);
  doc["led_status"] = ledStatus;
  doc["LED_BUILTIN_status"] = ledBuiltinStatus;


  // Serialize JSON payload
  String payload;
  serializeJson(doc, payload);

  client.publish(AWS_IOT_PUBLISH_TOPIC, payload.c_str());
  Serial.println("Dados publicados na AWS Console!");

/*  
  StaticJsonDocument<200> doc;
  doc["time"] = millis();
  doc["humidity"] = "testeee";
  doc["temperature"] = "temp";
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client
 
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
  */
}


void connectAWS()
{
  NTPConnect();
 
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);
 
  client.setServer(MQTT_HOST, 8883);
  client.setCallback(messageReceived);
 
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(1000);
  }
 
  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}