#include <ESP8266WiFi.h>
#include <ESP8266Firebase.h>
#include "string.h"
#include "ThingSpeak.h"
#define PROJECT_ID "room-d43f6-default-rtdb"

WiFiClient client;

Firebase firebase(PROJECT_ID);

char Frame[28]; // for incoming serial data
char* lux = "0";
char* hum = "0";
char* tem = "0";
char* aqi = "0";
int i = 0;

int error = 0;
unsigned long success_cnt = 0;
unsigned long fail_cnt = 0;
unsigned long null_cnt = 0;

unsigned long start_time = 0;
unsigned long stop_time = 0;

unsigned long delay_time = 30000;

unsigned long baud = 115200;

char* WIFI_SSID = "Huy Hoang";
char* WIFI_PASSWORD = "thanhhoang7222";

char* apiKey = "19INDU82WWFZL0ZH";
unsigned long thingspeak_ID = 1628530;

String restart_esp = "no";

void setup() {
  ThingSpeak.begin(client);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode(D1, OUTPUT); //interrupt stm32 sleep mode
  pinMode(D2, OUTPUT); //ready signal
  pinMode(D3, OUTPUT); //data not ok signal
  pinMode(D4, OUTPUT); //send data not ok signal
  Serial.begin(baud);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  delay(500);
  delay_time = firebase.getInt("Monitor time");
  baud = firebase.getInt("Serial Baud Rate");

  String ssid = firebase.getString("Wifi SSID");
  ssid.toCharArray(WIFI_SSID, ssid.length()+1);
  String password = firebase.getString("Wifi PW");
  password.toCharArray(WIFI_PASSWORD, password.length()+1);

  String api = firebase.getString("Thingspeak API");
  api.toCharArray(apiKey, api.length()+1);
  thingspeak_ID = firebase.getInt("Thingspeak ID");
  thingspeak_ID = (unsigned long) thingspeak_ID;

  restart_esp = firebase.getString("Reset");
  
  Serial.println();
  Serial.print("Wifi SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("Wifi password: ");
  Serial.println(WIFI_PASSWORD);
  Serial.print("Thingspeak API key: ");
  Serial.println(apiKey);
  Serial.print("Thingspeak ID: ");
  Serial.println(thingspeak_ID);
  Serial.print("Delay time: ");
  Serial.println(delay_time);
  Serial.print("Serial baud rate: ");
  Serial.println(baud);
  Serial.println();

  if(restart_esp == "yes"){
    Serial.begin(baud);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("connecting");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
    firebase.setString("Reset", "no");
  }
  
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  start_time = millis();
  digitalWrite(D1, HIGH);
  digitalWrite(D2, LOW);
  digitalWrite(D1, LOW);
  if (Serial.available() > 0) {
    //Read the data frame
    Serial.readBytesUntil('\n', Frame, 28);
    digitalWrite(D1, LOW);
    digitalWrite(D2, HIGH);
    delay_time = firebase.getInt("Monitor time");
    Serial.print("Data frame: ");
    Serial.println(Frame);

    //Split the frame into sub-data
    char* data = strtok(Frame, "&");
    while (data != NULL){
      if (i == 0){
        i = i + 1;
        lux = data;
      }
      else if (i == 1){
        i = i + 1;
        hum = data; 
      }
      else if (i == 2){
        i = i + 1;
        tem = data;
      }
      else if (i == 3){
        i = 0;
        aqi = data;
      }
      data = strtok (NULL, "&");
    }
    Serial.print("Light intensity: ");
    Serial.print(lux);
    Serial.println(" lux");
    Serial.print("Humidity: ");
    Serial.print(hum);
    Serial.println("%");
    Serial.print("Temperature: ");
    Serial.print(tem);
    Serial.println("C");
    Serial.print("Air quality index: ");
    Serial.print(aqi);
    Serial.println(" PPM");

    //Convert the sub-data to float points
    float Lux = atof(lux);
    float Hum = atof(hum);
    float Tem = atof(tem);
    float Aqi = atof(aqi);

    //Check if any error in data
    if(String(lux).indexOf("NULL") != -1 || String(hum).indexOf("NULL") != -1 || String(tem).indexOf("NULL") != -1 || String(aqi).indexOf("NULL") != -1){
      null_cnt += 1;
      Serial.println("Error data");
      digitalWrite(D3, LOW);
    }
    else if(lux == 0 && hum == 0 && tem == 0 && aqi == 0){
      null_cnt += 1;
      Serial.println("Error data");
      digitalWrite(D3, LOW);
    }
    //If there are no error, send the data to thingspeak
    else{
      digitalWrite(D3, HIGH);
      ThingSpeak.setField(1, Lux);
      ThingSpeak.setField(2, Hum);
      ThingSpeak.setField(3, Tem);
      ThingSpeak.setField(4, Aqi);
      int x = ThingSpeak.writeFields(thingspeak_ID, apiKey);
      if(x == 200){
        success_cnt += 1;
        error = 0;
        Serial.println("Update success.");
        digitalWrite(LED_BUILTIN, HIGH);
        digitalWrite(D4, HIGH);
      }
      else{
        error += 1;
        fail_cnt += 1;
        Serial.println("HTTP error code " + String(x));
        digitalWrite(LED_BUILTIN, LOW);
        while(error != 0){
          int x = ThingSpeak.writeFields(thingspeak_ID, apiKey);
          if(x == 200){
            success_cnt += 1;
            error = 0;
            Serial.println("Re-update success.");
            digitalWrite(LED_BUILTIN, HIGH);
            digitalWrite(D4, HIGH);
          }
          else{
            error += 1;
            fail_cnt += 1;
            if(error == 3){
              error = 0;
              Serial.println("Update fail 3 times");
              digitalWrite(D4, LOW);
            }
          }
        }
      }
    }
    Serial.print("Send success counter: ");
    Serial.println(success_cnt);
    Serial.print("Send fail counter: ");
    Serial.println(fail_cnt);
    Serial.print("NULL data counter: ");
    Serial.println(null_cnt);
    Serial.print("Delay time: ");
    Serial.print(delay_time);
    Serial.println("ms");
    Serial.println("Waiting...");
    Serial.println();
  }
  restart_esp = firebase.getString("Reset");
  stop_time = millis();
  if(restart_esp != "yes"){
      if(delay_time > (stop_time - start_time)){
        delay(delay_time - (stop_time - start_time));
      }
      else{
        Serial.println("Delay time lower than process time!");
        Serial.println();
      }
  }
  else{
      Serial.println("Restart ESP8266!");
      ESP.reset();
  }
}
