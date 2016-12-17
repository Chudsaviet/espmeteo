#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>
#include <PubSubClient.h>

/* Loop delay should not be less than 2000 ms due to DHT22 restrictions */
#define LOOP_DELAY_MS 30000
#define REBOOT_LOOPS 1000
#define DHTPIN 5
#define DHTTYPE DHT22
#define STANDART_SHORT_DELAY 500 //ms
#define WIFI_MAX_ATTEMPTS 100
#define FLOAT_MAX_CHARS 64
#define INT_MAX_CHARS 64
#define MQTT_MESSAGE_MAX_SIZE 256

typedef struct  {
    float dht22_humidity;
    float dht22_temperature;
    int   bmp180_pressure;
    float bmp180_temperature;
    int rssi;
} Measures;

void module_panic(const char* message);
void reboot_module();
void assert(int err, const char* name);
Measures do_measures();
void print_measures(Measures m);
void connect_wifi();
void setup();
void loop();
void reconnect_mqtt();

#endif
