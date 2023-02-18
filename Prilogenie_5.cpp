#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include "DHT.h"
#include <DHT_U.h>
# define DHTTYPE DHT22



/* Установите здесь свои SSID и пароль */
const char* ssid = "NodeMCU";       // SSID
const char* password = "12345678";  // пароль

/* Настройки IP адреса */
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

ESP8266WebServer server(80);
uint8_t DHTPin = D8; 
               
DHT dht(DHTPin, DHTTYPE);

float Temperature=0;
 
float Humidity=0;

uint8_t LED1pin = D7;
bool LED1status = LOW;

uint8_t LED2pin = D6;
bool LED2status = LOW;

String SendHTML(uint8_t led1stat,uint8_t led2stat,float Temperature,float Humidity)
{
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<title>Экран управления</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr +=".button {display: block;width: 80px;background-color: #1abc9c;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr +=".button-on {background-color: #e544d2;}\n";
  ptr +=".button-on:active {background-color: #e01f60;}\n";
  ptr +=".button-off {background-color: #34495e;}\n";
  ptr +=".button-off:active {background-color: #2c3e50;}\n";
  ptr +="p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<h1>ESP8266 Web Server</h1>\n";
  ptr +="<h3>Using Access Point(AP) Mode</h3>\n";



  if(led1stat)
    ptr +="<p>Irrigation system Status: ON</p><a class=\"button button-off\" href=\"/led1off\">OFF</a>\n";
  else
    ptr +="<p>Irrigation system Status: OFF</p><a class=\"button button-on\" href=\"/led1on\">ON</a>\n";

  if(led2stat)
    ptr +="<p>Humidification system Status: ON</p><a class=\"button button-off\" href=\"/led2off\">OFF</a>\n";
  else
    ptr +="<p>Humidification system Status: OFF</p><a class=\"button button-on\" href=\"/led2on\">ON</a>\n";

  ptr +="<p>Temperature: ";
 
  ptr +=(int)Temperature;
 
  ptr +="°C</p>";
 
  ptr +="<p>Humidity: ";
 
  ptr +=(int)Humidity;
 
  ptr +="%</p>";
 
  ptr +="</div>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}

void handle_OnConnect() 
{ 
  Serial.print("GPIO7 Status: ");
  if(LED1status)
    Serial.print("ON");
  else
    Serial.print("OFF");

  Serial.print(" | GPIO6 Status: ");
  if(LED2status)
    Serial.print("ON");
  else
    Serial.print("OFF");

  Serial.println("");

  Temperature = dht.readTemperature(); // Получает значение температуры
 
  Humidity = dht.readHumidity(); // Получаем показания влажности

  server.send(200, "text/html", SendHTML(LED1status,LED2status,Temperature,Humidity)); 
  
}

void handle_led1on() 
{
  LED1status = HIGH;
  Serial.println("GPIO7 Status: ON");
  server.send(200, "text/html", SendHTML(true,LED2status,Temperature,Humidity)); 
}

void handle_led1off() 
{
  LED1status = LOW;
  Serial.println("GPIO7 Status: OFF");
  server.send(200, "text/html", SendHTML(false,LED2status,Temperature,Humidity)); 
}

void handle_led2on() 
{
  LED2status = HIGH;
  Serial.println("GPIO6 Status: ON");
  server.send(200, "text/html", SendHTML(LED1status,true,Temperature,Humidity)); 
}

void handle_led2off() 
{
  LED2status = LOW;
  Serial.println("GPIO6 Status: OFF");
  server.send(200, "text/html", SendHTML(LED1status,false,Temperature,Humidity)); 
}

void handle_NotFound()
{
  server.send(404, "text/plain", "Not found");
}
void setup() 
{
  Serial.begin(115200);
  pinMode(LED1pin, OUTPUT);
  pinMode(LED2pin, OUTPUT);
  pinMode(DHTPin, INPUT);
  dht.begin();
  Serial.println("Connecting to ");
 
  Serial.println(ssid);

  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(100);
  
  server.on("/", handle_OnConnect);
  server.on("/led1on", handle_led1on);
  server.on("/led1off", handle_led1off);
  server.on("/led2on", handle_led2on);
  server.on("/led2off", handle_led2off);
  
  server.onNotFound(handle_NotFound);
  
  server.begin();
  Serial.println("HTTP server started");
}

void loop() 
{
  server.handleClient();
  if(LED1status)
    digitalWrite(LED1pin, HIGH);
  else
    digitalWrite(LED1pin, LOW);
  
  if(LED2status)
    digitalWrite(LED2pin, HIGH);
  else
    digitalWrite(LED2pin, LOW);
}
