#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoHttpClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Ver src/config_template.h
#include "config_home.h"

// Constants
#define SERIALBPS 115200 // Debug
#define SERIAL2BPS 19200 // SIM800L (GSM) - GPIO 16/17
#define BATPIN 34        // (mide vbat)
#define VCCPIN 35        //(mide vcc de la fuente)
#define VAC1 27          // Mide fase 1
#define VAC2 32          // Mide fase 2
#define VAC3 33          // Mide fase 3
#define LINE1 22        // Linea de DS18b20 1 (cerca?)
#define LINE2 23        // Linea de DS18b20 2 (lejos?)

WiFiClient wifi_client;
// VER - deberia ser GSM client?
WiFiClient gprs_client;

StaticJsonDocument<600> doc;
PubSubClient mqttClient;

OneWire line1(LINE1);
OneWire line2(LINE2);
DallasTemperature tline1(&line1);
DallasTemperature tline2(&line2);

// Funciones
#include "functions.h"

void setup() {
  double sleep;

  // init general (serial, wifi, gprs, etc)
  general_init();
  Serial.printf("\n\n::: General init %ld\n", millis());

  // json init
  doc["power"]["wakeup"] = nullptr;
  doc["power"]["sleep"] = nullptr;
  doc["power"]["vbat"] = nullptr;
  doc["power"]["pbat"] = nullptr;
  doc["power"]["vcc"] = nullptr;
  doc["sensor"]["tline1"]["quantity"] = nullptr;
  doc["sensor"]["tline2"]["quantity"] = nullptr;
  doc["sensor"]["mains"]["vac1"] = nullptr;
  doc["sensor"]["mains"]["vac2"] = nullptr;
  doc["sensor"]["mains"]["vac3"] = nullptr;
  doc["connectivity"]["wifi"] = nullptr;
  doc["connectivity"]["gprs"] = nullptr;
  doc["connectivity"]["gsm"] = nullptr;
  doc["connectivity"]["internet"] = nullptr;
  doc["connectivity"]["mqtt"] = nullptr;
  doc["connectivity"]["mqtt_sent"] = nullptr;
  doc["connectivity"]["sms_sent"] = nullptr;


  // wakeup
  doc["power"]["wakeup"] = esp_sleep_get_wakeup_cause();

  // temperatura
  temp(&tline1, doc, "sensor", "tline1");
  temp(&tline2, doc, "sensor", "tline2");
  Serial.printf("\n\n::: Temp sensors %ld\n", millis());

  // power del supervisor
  double vbat = 0;
  int pbat = 0;
  double vcc = 0;
  vbat = voltage(BATPIN);
  pbat = bat_percentage(vbat);
  doc["power"]["vbat"] = vbat;
  doc["power"]["pbat"] = pbat;
  Serial.printf("\n\n::: Bat sensor %ld\n", millis());
  vcc = voltage(VCCPIN);
  doc["power"]["vcc"] = vcc;
  Serial.printf("\n\n::: VCC sensor %ld\n", millis());

  // sleep calc
  sleep = sleep_calc(vcc, pbat);
  doc["power"]["sleep"] = sleep;

  // energia de red
  doc["sensor"]["mains"]["vac1"] = vac_presence(VAC1);
  doc["sensor"]["mains"]["vac2"] = vac_presence(VAC2);
  doc["sensor"]["mains"]["vac3"] = vac_presence(VAC3);
  Serial.printf("\n\n::: Mains sensors %ld\n", millis());

  // wifi?
  boolean wifi_conn = wifi_check();
  doc["connectivity"]["wifi"] = wifi_conn;
  Serial.printf("\n\n::: Wifi check %ld\n", millis());

  // gprs?
  boolean gprs_conn = gprs_check();
  doc["connectivity"]["gprs"] = gprs_conn;
  Serial.printf("\n\n::: Gprs check %ld\n", millis());

  // internet?
  boolean internet_conn = false;

  if(wifi_conn) {
    internet_conn = internet_check(wifi_client, INTERNET1, PORT1);
    if(!internet_conn) {
      internet_conn = internet_check(wifi_client, INTERNET2, PORT2);
    }
    if(internet_conn) {
      doc["connectivity"]["internet"] = "wifi";
    }
    Serial.printf("\n\n::: Internet (Wifi) check %ld\n", millis());
  }
  else if (gprs_conn)
  {
    internet_conn = internet_check(gprs_client, INTERNET1, PORT1);
    if (!internet_conn)
    {
      internet_conn = internet_check(gprs_client, INTERNET2, PORT2);
    }
    if (internet_conn)
    {
      doc["connectivity"]["internet"] = "gprs";
    }
    Serial.printf("\n\n::: Internet (Gprs) check %ld\n", millis());
  }

  // MQTT?
  boolean mqtt_conn = false;
  if (internet_conn)
  {
    if(wifi_conn){
      mqtt_conn = mqtt_check(wifi_client);
    } 
    else if (gprs_conn)
    {
      mqtt_conn = mqtt_check(gprs_client);
    }
    doc["connectivity"]["mqtt"] = mqtt_conn;
  }
  Serial.printf("\n\n::: Mqtt check %ld\n", millis());

  // send json
  boolean mqtt_sent = false;
  boolean sms_sent = false;

  if(mqtt_conn) {
    // envio json por mqtt x internet
    mqttClient.beginPublish(MQTT_USER "/" HOSTNAME, measureJson(doc), false);
    serializeJson(doc, mqttClient);
    mqtt_sent = mqttClient.endPublish();    
    doc["connectivity"]["mqtt_sent"] = mqtt_sent;
    mqttClient.disconnect();
  }
  Serial.printf("\n\n::: Mqtt send %ld\n", millis());

  // salio?
  if(!mqtt_sent){
    // si no se puede enviar mqtt, aviso por sms...
    // armo un mensaje a partir del json
    //  temperaturas
    //  mains
    //  power o bateria?
    //  % bateria
    //  wifi, gprs, internet, mqtt
    Serial.println("no salio");
  }

  // para debug a serial
  serializeJsonPretty(doc, Serial);
  
  // deep sleep
  // wakeup por pin 35 si se corta o vuelve VCC
  set_wakeup(vcc);
  // wakeup por tiempo
  esp_sleep_enable_timer_wakeup(sleep * 60e6);
  Serial.printf("\n\n::: Deep Sleep %ld\n", millis());
  esp_deep_sleep_start();
}

void loop() { 
  // Aca nada, porque se va a deep sleep
}