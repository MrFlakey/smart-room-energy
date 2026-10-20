// Wi-Fi, MQTT and publishing. Everything here runs beside the room control:
// without Wi-Fi or a broker the room keeps working, the messages still go to
// the serial monitor, and the board reconnects in the background.
#pragma once

#include <stdbool.h>

void net_start(void);

// Queue one event (JSON) for the event topic. Never blocks. Events made while
// offline are dropped, except keep=true ones (faults), which are sent on
// the next connect.
void net_post_event(const char *json, bool keep);
