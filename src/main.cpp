#include <map>
#include <string>

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Update.h>

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <esp32WS2811.h>

#include <mqtt.h>
#include "html.h"

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

bool shouldReboot = false;
SemaphoreHandle_t smphr;
AsyncWebServer webserver(80);
AsyncWebSocket websocket("/ws");
constexpr size_t jsonCapacity = JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(3) + 36;
WS2811 leds(18, 50);
struct CstrCmp {
  bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) < 0;
  }
};
std::map<const char*, WS2811Effect*, CstrCmp> effects;
std::map<const char*, WS2811Effect*, CstrCmp>::iterator currentEffect = effects.begin();
Colour currentColour(255,255,255);
bool status = false;

void updateLeds() {
  if (!status) {
    leds.stopEffect();
    leds.setAll(0, 0, 0);
    leds.show();
  } else {
    if (currentEffect == effects.find("singleColour")) {
      leds.stopEffect();
      leds.setAll(currentColour);
      leds.show();
    } else {
      leds.startEffect(currentEffect->second);  // this also stops any running effect
    }
  }
  websocket.textAll("leds updated\n");
  Serial.print("leds updated:\n");
  Serial.printf("status: %s\n", status ? "on" : "off");
  Serial.printf("effect: %s\n", currentEffect->first);
}

void sendCurrent(AsyncWebSocketClient* client = nullptr) {
  StaticJsonDocument<jsonCapacity> json;
  json["status"] = status;
  json["effect"] = currentEffect->first;
  JsonArray value = json.createNestedArray("colour");
  value.add(currentColour.red);
  value.add(currentColour.green);
  value.add(currentColour.blue);
  size_t len = measureJson(json);
  AsyncWebSocketMessageBuffer * buffer = websocket.makeBuffer(len);  // buffer will be len + 1
  if (buffer) {
    serializeJson(json, buffer->get(), len + 1);
    if (client) {
      client->text(buffer);
    } else {
      websocket.textAll(buffer);
    }
  }
}

void handleCommand(StaticJsonDocument<jsonCapacity>& json) {
  bool jsonStatus = json["status"];
  const char* jsonEffect = json["effect"];
  JsonArray jsonColour = json["colour"];
  bool succes = true;
  
  if (json.containsKey("status")) {
    status = jsonStatus;
  } else {
    succes = false;
  }
  if (jsonEffect) {
    auto it = effects.find(jsonEffect);
    if (it != effects.end()) {
      currentEffect = it;
    } else {
      succes = false;
    }
  } else {
    succes = false;
  }
  if (!jsonColour.isNull()) {
    currentColour.red = jsonColour[0];
    currentColour.green = jsonColour[1];
    currentColour.blue = jsonColour[2];
  } else {
    succes = false;
  }

  if (succes) {
    updateLeds();
    sendCurrent();
  } else {
    Serial.print("Json content error\n");
    websocket.textAll("Json content error\n");
  }
}

void onMqttMessage(char* topic,
                   char* payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len,
                   size_t index,
                   size_t total) {
  Serial.printf("topic: %s\npayload: ", topic);
  for (size_t i = 0; i < len; ++i) {
    Serial.print(payload[i]);
  }
  Serial.print("\n");
  
}

void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (xSemaphoreTake(smphr, 50) == pdTRUE) {
    if (type == WS_EVT_DATA) {
      AwsFrameInfo * info = reinterpret_cast<AwsFrameInfo*>(arg);
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        // the whole message is in a single frame and we got all of it's data
        StaticJsonDocument<jsonCapacity> doc;
        DeserializationError error = deserializeJson(doc, data, len);
        if (!error) {
          handleCommand(doc);
        } else {
          Serial.printf("command error: %s\n", error.c_str());
          websocket.printfAll("command error: %s\n", error.c_str());
        }
      }
    } else if (type == WS_EVT_CONNECT) {
      Serial.print("new ws client\n");
      sendCurrent(client);
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.print("client disconnected\n");
    }
    xSemaphoreGive(smphr);
  }
}

void setupWebserver() {
  webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  webserver.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/css", style_css);
  });
  webserver.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/javascript", script_js);
  });
  webserver.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse* response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      Update.abort();
      bool begin = Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
      if (!begin) {
        Update.printError(Serial);
      } else {
        Serial.print("Updating\n");
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
    }
    if (final) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %uB\n", index+len);
      } else {
        Update.printError(Serial);
      }
    }
  });
  webserver.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404);
  });
  websocket.onEvent(onWsEvent);
  webserver.addHandler(&websocket);
}

void onWiFiConnected() {
  setupWebserver();
  webserver.begin();
}

void onWiFiDisconnected() {
  webserver.reset();
  webserver.end();
}

void setup() {
  Serial.begin(115200);

  setupWifi();
  setupMqtt();

  // setup available effects
  effects.emplace("singleColour", nullptr);
  effects.emplace("circus", new Circus(1000));
  effects.emplace("snow", new SnowSparkle({82, 56, 13}, 5, 100, 500));
  effects.emplace("aurora", new Aurora);
  currentEffect = effects.find("singleColour");

  // give semaphore for first use
  smphr = xSemaphoreCreateBinary();
  xSemaphoreGive(smphr);

  // output enable level shifter
  pinMode(23, OUTPUT);
  digitalWrite(23, HIGH);
  leds.begin();
  leds.clearAll();
  leds.show();
}

void loop() {
  static uint32_t lastMillis = 0;
  if (millis() - lastMillis > 120000UL) {
    lastMillis = millis();
    Serial.printf("%d: free heap: %d\n", lastMillis, ESP.getFreeHeap());
  }
  if (shouldReboot) {
    leds.clearAll();
    leds.show();
    delay(1000);
    ESP.restart();
  }
  delay(1);
}
