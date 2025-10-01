#pragma once

#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 1024
#endif

#include <PubSubClient.h>

void setupMQTT();
void handleMQTT();

// Publish command for a specific channel (0 = A, 1 = B)
void sendStateCh(int ch, bool force = false);

bool mqttIsConnected();
bool mqttPublishTest();

void haDiscoveryPublish(bool remove = false);