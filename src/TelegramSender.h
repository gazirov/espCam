// TelegramSender.h
#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "SettingsManager.h"

void sendToTelegram(camera_fb_t *fb, const String& fileName);