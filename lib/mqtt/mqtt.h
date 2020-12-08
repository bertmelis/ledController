#ifdef ESP8266
#include <ESP8266WiFi.h>
#else  // ESP32
#include <WiFi.h>
#endif
#include <Ticker.h>
#include <AsyncMqttClient.hpp>

#pragma once

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

#ifndef DEVICENAME
#define DEVICENAME esp-12345
#endif
#ifndef BASETOPIC
#define BASETOPIC vhs100
#endif

#define _MQTT_TOPIC(x) BASETOPIC/DEVICENAME/x
#define MQTT_TOPIC(x) STRINGIFY(_MQTT_TOPIC(x))
#define _MQTT_WILL_TOPIC BASETOPIC/DEVICENAME/system/status
#define MQTT_WILL_TOPIC STRINGIFY(_MQTT_WILL_TOPIC)
#define _MQTT_SUB_TOPIC BASETOPIC/DEVICENAME/+/set
#define MQTT_SUB_TOPIC STRINGIFY(_MQTT_SUB_TOPIC)

void setupWifi();
void setupMqtt();
void onMqttMessage(char* topic,
                   char* payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len,
                   size_t index,
                   size_t total) __attribute__((weak));

extern AsyncMqttClient mqttClient;
