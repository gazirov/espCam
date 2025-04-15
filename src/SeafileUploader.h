// SeafileUploader.h
#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include "esp_camera.h"
#include "SettingsManager.h"
#include <HTTPClient.h> // Для отправки HTTP-запросов

bool fetchSeafileToken();
void sendToSeafile(camera_fb_t *fb, const String& fileName);