
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Servo.h>

#include "index.h"

#define LED 2
#define ServoPin D5 


const char *ssid = "NodeMCU2";
const char *password = "12345678";
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);



Servo myservo; 

ESP8266WebServer server(80);


void handleServo(){
 String POS = server.arg("servoPOS");
 int pos = POS.toInt();
 myservo.write(pos+180); 
 delay(15);
 Serial.print("Servo Angle:");
 Serial.println(pos+180);
 digitalWrite(LED,!(digitalRead(LED))); 
 server.send(200, "text/plane","");
}

void handleRoot() {
 String s = MAIN_page; 
 server.send(200, "text/html", s); 
}

void handle_NotFound()
{
  server.send(404, "text/plain", "Not found");
}

void setup() {
 Serial.begin(115200);
 Serial.println("Connecting to ");
 
 Serial.println(ssid);


 pinMode(LED,OUTPUT);
 myservo.attach(ServoPin); 
 
 
 WiFi.begin(ssid, password); 
 WiFi.softAP(ssid, password);
 WiFi.softAPConfig(local_ip, gateway, subnet);
 delay(100);
 
 server.on("/",handleRoot);
 server.on("/setPOS",handleServo); 
 server.onNotFound(handle_NotFound);
  
 server.begin();
 Serial.println("HTTP server started");
}

void loop() {
 server.handleClient();
}

