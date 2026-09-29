// Messages in and out: JSON lines on USB serial always, MQTT over Wi-Fi when enabled.
// Topic layout is documented in docs/mqtt-contract.md.
#pragma once
#include <ArduinoJson.h>

typedef void (*CommandHandler)(JsonDocument& cmd);

void commsBegin(CommandHandler onCommand);

// Call every loop. Returns true right after (re)connecting to the MQTT broker,
// so the caller can re-send its state.
bool commsLoop();

// subtopic: "telemetry", "state", "event" (the serial line is prefixed with it)
void commsPublish(const char* subtopic, JsonDocument& doc, bool retained = false);

bool commsOnline();
