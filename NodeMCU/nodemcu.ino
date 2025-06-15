#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <WiFiClient.h>
#include <TridentTD_LineNotify.h>
#include <WiFiManager.h>

#define BLYNK_TEMPLATE_ID "TMPL6bD5_WOag"
#define BLYNK_TEMPLATE_NAME "Eldercare Project"
#define BLYNK_DEVICE_NAME "Select Mode"
#define BLYNK_AUTH_TOKEN "muRV7J4h8fzmf_f2sBIMs4lcFnWfZKOs"

String LINE_TOKEN = "qFcOGWcjUWRbxjMnCqTuakORly91d5i2dGU6mAZnt7H";
unsigned long lastNotificationTime = 0, lastSend = 0, lastTime = 0;
const unsigned long notificationInterval = 60000;

const char* server = "api.thingspeak.com";
const char* api_key = "E0EJPXHG4KLTO3TX";
WiFiClient client;

enum Mode {
  detection,
  readText
};

Mode deviceMode = detection;

BLYNK_WRITE(V0) {
  int pinValue = param.asInt();
  if (pinValue == 1) {
    deviceMode = detection;
    Serial.println("MODE:1");
    LINE.notify("โหมดตรวจจับการล้ม");
  }
}

BLYNK_WRITE(V1) {
  int pinValue = param.asInt();
  if (pinValue == 1) {
    deviceMode = readText;
    Serial.println("MODE:2");
    LINE.notify("โหมดอ่านฉลากยา");
  }
}

void sendDataToThingSpeak(String data) {
  if (client.connect(server, 80)) {
    String url = "/update?api_key=" + String(api_key);

    int cpu_index = data.indexOf("CPU:") + 4;    
    int ram_index = data.indexOf("RAM:") + 4;    
    int temp_index = data.indexOf("TEMP:") + 5;  

    String cpu_usage = data.substring(cpu_index, data.indexOf(",", cpu_index));  
    String ram_usage = data.substring(ram_index, data.indexOf(",", ram_index));  
    String cpu_temp = data.substring(temp_index);                                

    url += "&field1=" + cpu_usage + "&field2=" + ram_usage + "&field3=" + cpu_temp;


    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + server + "\r\n" + "Connection: close\r\n\r\n");

    client.stop();
  }
}

void processData(String data) {
  unsigned long currentTime = millis();
  
  if (data.startsWith("NOTIFICATION:")) {
    String action = data.substring(13);

    if (action == "Morning") {
      LINE.notify("แจ้งเตือนการรับประทานยาตอนเช้า");
    } else if (action == "Noon") {
      LINE.notify("แจ้งเตือนการรับประทานยาตอนเที่ยง");
    } else if (action == "Evening") {
      LINE.notify("แจ้งเตือนการรับประทานยาตอนเย็น");
    } else if (action == "Beforebed") {
      LINE.notify("แจ้งเตือนการรับประทานยาก่อนนอน");
    } else {
      LINE.notify("แจ้งเตือนการรับประทานยาเวลา " + action);
    }
  
  } else if (data == "MODE:1") {
    deviceMode = detection;
    LINE.notify("โหมดตรวจจับการล้ม");

  } else if (data == "MODE:2") {
    deviceMode = readText;
    LINE.notify("โหมดอ่านฉลากยา");

  } else if (data.startsWith("CPU:")) {
    if (millis() - lastSend > 20000) {
      sendDataToThingSpeak(data);

      lastSend = millis();
    }
  }
}


void setup() {

  Serial.begin(9600);

  WiFiManager wifiManager;
  wifiManager.autoConnect("ElderCare System");

  Blynk.config(BLYNK_AUTH_TOKEN);
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.connect();  
  }

  LINE.setToken(LINE_TOKEN);
  LINE.notify("Device Connected to WiFi and Blynk");
}

void loop() {
  Blynk.run();
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    processData(data);
  }

  if (millis() - lastTime > 60000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WIFI");
    } else {
      Serial.println("LOST");
    }
    lastTime = millis();
  }
}