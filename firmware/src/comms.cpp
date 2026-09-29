#include "comms.h"
#include "board.h"
#include "config.h"

#if ENABLE_MQTT
#include <PubSubClient.h>
static WiFiClient net;
static PubSubClient mqtt(net);
static unsigned long lastWifiTry = 0;
static unsigned long lastMqttTry = 0;
static bool wifiTried = false;
#endif

static CommandHandler handler = nullptr;
static char serialLine[160];
static uint8_t serialLen = 0;

static void handlePayload(const char* data, size_t len) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len) || !doc.is<JsonObject>()) {
    Serial.println(F("error {\"msg\":\"invalid command json\"}"));
    return;
  }
  if (handler) handler(doc);
}

#if ENABLE_MQTT
static void onMqttMessage(char* /*topic*/, byte* payload, unsigned int len) {
  handlePayload((const char*)payload, len);
}

// Returns true when it just (re)connected to the broker.
static bool maintainMqtt() {
  unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    // WiFi.begin() blocks for a few seconds on the Rev2, so retry sparingly
    if (!wifiTried || now - lastWifiTry >= WIFI_RETRY_MS) {
      wifiTried = true;
      lastWifiTry = now;
      Serial.println(F("info {\"msg\":\"connecting wifi\"}"));
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    return false;
  }
  if (mqtt.connected()) {
    mqtt.loop();
    return false;
  }
  if (now - lastMqttTry < MQTT_RETRY_MS) return false;
  lastMqttTry = now;

  const char* user = strlen(MQTT_USER) ? MQTT_USER : nullptr;
  const char* pass = strlen(MQTT_PASSWORD) ? MQTT_PASSWORD : nullptr;
  // Last Will: the broker marks the room offline if the board disappears
  if (!mqtt.connect("smartroom-" ROOM_ID, user, pass, TOPIC_BASE "status", 1, true, "offline")) {
    Serial.print(F("info {\"msg\":\"mqtt connect failed\",\"rc\":"));
    Serial.print(mqtt.state());
    Serial.println('}');
    return false;
  }
  mqtt.publish(TOPIC_BASE "status", "online", true);
  mqtt.subscribe(TOPIC_BASE "cmd");
  Serial.println(F("info {\"msg\":\"mqtt connected\"}"));
  return true;
}
#endif

void commsBegin(CommandHandler onCommand) {
  handler = onCommand;
#if ENABLE_MQTT
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(512);
#endif
}

bool commsLoop() {
  // Serial accepts the same JSON commands as MQTT, one per line
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialLen) handlePayload(serialLine, serialLen);
      serialLen = 0;
    } else if (serialLen < sizeof(serialLine) - 1) {
      serialLine[serialLen++] = c;
    }
  }
#if ENABLE_MQTT
  return maintainMqtt();
#else
  return false;
#endif
}

void commsPublish(const char* subtopic, JsonDocument& doc, bool retained) {
  char buf[400];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  Serial.print(subtopic);
  Serial.print(' ');
  Serial.println(buf);
#if ENABLE_MQTT
  if (mqtt.connected()) {
    char topic[64];
    snprintf(topic, sizeof(topic), "%s%s", TOPIC_BASE, subtopic);
    mqtt.publish(topic, (const uint8_t*)buf, n, retained);
  }
#endif
}

bool commsOnline() {
#if ENABLE_MQTT
  return mqtt.connected();
#else
  return false;
#endif
}
