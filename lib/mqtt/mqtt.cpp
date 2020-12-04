#include "mqtt.h"

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
uint32_t nextReconnectInterval = 2;

uint32_t getNextReconnectInterval() {
  nextReconnectInterval *= 2;
  if (nextReconnectInterval > 60) {
    nextReconnectInterval = 60;
  }
  return nextReconnectInterval;
}

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(STRINGIFY(WIFI_SSID), STRINGIFY(WIFI_PASS));
}

void onWifiDisconnect() {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onWifiConnect() {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void setupWifi() {
#ifdef ESP8266
  static wifiConnectHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
    onWifiConnect();
  });
  static wifiDisconnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    onWifiDisconnect();
  });
#else  // ESP32
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    onWifiConnect();
  }, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    onWifiDisconnect();
  }, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
#endif

  connectToWifi();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  nextReconnectInterval = 2;
  mqttClient.publish(MQTT_WILL_TOPIC, 0, true, "online");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(getNextReconnectInterval(), connectToMqtt);
  }
}

void setupMqtt() {
  mqttClient.setServer(STRINGIFY(MQTT_HOST), MQTT_PORT )
            .setCleanSession(true)
            .setKeepAlive(60)
            .setWill(MQTT_WILL_TOPIC, 0, true, "offline")
            .onConnect(onMqttConnect)
            .onDisconnect(onMqttDisconnect);
}
