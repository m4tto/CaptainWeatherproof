#pragma once
#include <PubSubClient.h>
#include <ArduinoJson.h>

void publishDiscovery(PubSubClient& mqtt);
void publishState(PubSubClient& mqtt, const char *state_topic, JsonDocument& jsondata);