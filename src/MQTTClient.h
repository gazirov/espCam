// MQTTClient.h
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_camera.h"
#include "SettingsManager.h"

void initMQTT(PubSubClient& client);
void connectToMQTT(PubSubClient& client);
void publishToMQTT(PubSubClient& client, camera_fb_t *fb);