#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoHttpClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// The ESP8266 RTC memory is arranged into blocks of 4 bytes. The access methods read and write 4 bytes at a time,
// so the RTC data structure should be padded to a 4-byte multiple.
// struct
// {
//   uint32_t crc32;   // 4 bytes
//   uint8_t channel;  // 1 byte,   5 in total
//   uint8_t bssid[6]; // 6 bytes, 11 in total
//   uint8_t gsmcheck;  // 1 byte,  12 in total
// } rtcData;

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
StaticJsonDocument<400> doc;
PubSubClient mqttClient;

OneWire line1(LINE1);
OneWire line2(LINE2);
DallasTemperature tline1(&line1);
DallasTemperature tline2(&line2);

// Funciones
#include "functions.h"

void setup() {

  // init general (serial, wifi, gprs, etc)
  serial_init();
  Serial.printf("\n\n::: Serial init %ld\n", millis());
  wifi_init();
  Serial.printf("\n\n::: Wifi init %ld\n", millis());
  mqtt_init();
  Serial.printf("\n\n::: Mqtt init %ld\n", millis());

  // wakeup
  doc["power"]["wakeup"] = esp_sleep_get_wakeup_cause();

  // temperatura
  temp(&tline1, doc, "sensor", "tline1");
  temp(&tline2, doc, "sensor", "tline2");
  Serial.printf("\n\n::: Temp sensors %ld\n", millis());

  // energia
  doc["sensor"]["mains"]["vac1"] = vac_presence(VAC1);
  doc["sensor"]["mains"]["vac2"] = vac_presence(VAC2);
  doc["sensor"]["mains"]["vac3"] = vac_presence(VAC3);
  Serial.printf("\n\n::: Mains sensors %ld\n", millis());

  // power del supervisor
  double vbat = 0;
  double vcc = 0;
  vbat = voltage(BATPIN);
  doc["power"]["vbat"] = vbat;
  doc["power"]["pbat"] = bat_percentage(vbat);
  Serial.printf("\n\n::: Bat sensor %ld\n", millis());
  vcc = voltage(VCCPIN);
  doc["power"]["vcc"] = vcc;
  Serial.printf("\n\n::: VCC sensor %ld\n", millis());

  // Si vcc es menor a 4 (diria que es cercano a 0), deberia haber problemas... posiblemente estemos andando en bateria
  // asi que aca debemos pasarnos a modo deepsleep... ?

  // wifi?
  boolean wifi_conn = wifi_check();
  doc["connectivity"]["wifi"] = wifi_conn;
  Serial.printf("\n\n::: Wifi check %ld\n", millis());

  // conexion gprs
  boolean gprs_conn = false;
  // doc["connectivity"]["gsmcheck"] = rtcData.gsmcheck;
  doc["connectivity"]["gsmcheck"] = false;

  // if (rtcData.gsmcheck == 1)
  // {
  //   // Aca hay que chequear el gsm/gprs
  //   doc["connectivity"]["gprs"] = gprs_conn;
  //   doc["connectivity"]["gprs tested"] = true;
  // }
  // else
  // {
    doc["connectivity"]["gprs"] = gprs_conn;
    doc["connectivity"]["gprs tested"] = false;
  // }


  // internet?
  boolean internet_conn = false;
  doc["connectivity"]["internet"] = internet_conn;

  if(wifi_conn) {
    internet_conn = internet_check(wifi_client, INTERNET1, PORT1);
    if(!internet_conn) {
      internet_conn = internet_check(wifi_client, INTERNET2, PORT2);
    }
    doc["connectivity"]["internet"] = internet_conn;
  }
  else if (gprs_conn)
  {
    /* Mandar por aca la verificacion de internet*/
    Serial.println("No hay wifi, pruebo internet por GPRS");
  }
  Serial.printf("\n\n::: Internet check %ld\n", millis());

  // MQTT?
  boolean mqtt_conn = false;
  if (internet_conn)
  {
    if(wifi_conn){
      mqtt_conn = mqtt_check(wifi_client);
    } 
    else if (gprs_conn)
    {
      // mqtt_conn = mqtt_check(gprs_client);
    }

    doc["connectivity"]["mqtt"] = mqtt_conn;
  }
  Serial.printf("\n\n::: Mqtt check %ld\n", millis());

  // send json
  boolean mqtt_sent = false;
  boolean sms_sent = false;

  doc["connectivity"]["mqtt sent"] = mqtt_sent;
  doc["connectivity"]["sms sent"] = sms_sent;

  if(mqtt_conn) {
    // envio json por mqtt x internet
    mqttClient.beginPublish(MQTT_USER "/" HOSTNAME, measureJson(doc), false);
    serializeJson(doc, mqttClient);
    mqtt_sent = mqttClient.endPublish();

    // mqttClient.beginPublish(MQTT_USER "/" HOSTNAME, measureJson(doc), false);
    // BufferingPrint bufferedClient(mqttClient, 32);
    // serializeJson(doc, bufferedClient);
    // bufferedClient.flush();
    // mqtt_sent = mqttClient.endPublish();
    // https: // arduinojson.org/v6/how-to/use-arduinojson-with-pubsubclient/
    
    doc["connectivity"]["mqtt sent"] = mqtt_sent;
    mqttClient.disconnect();
  }
  Serial.printf("\n\n::: Mqtt send %ld\n", millis());

  if(!mqtt_sent){
    // si no se puede enviar mqtt, aviso por sms...
    // hay que setear sms_sent a true si salio
    Serial.println("no salio");
  }


  serializeJsonPretty(doc, Serial);

  // deep sleep
  Serial.printf("\n\n::: Deep Sleep %ld\n", millis());
  if(vcc < 4) {
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 1); // wakeup cuando vuelve VCC
  } else
  {
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0); // wakeup cuando se va VCC
  }

  esp_sleep_enable_timer_wakeup(SLEEPTIME);
  esp_deep_sleep_start();
}

void loop() { 
  // Aca nada, porque se va a deep sleep
}