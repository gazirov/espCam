#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

void handleRoot(AsyncWebServerRequest *request);
void handleSaveConfig(AsyncWebServerRequest *request);
void handleVideoPage(AsyncWebServerRequest *request);