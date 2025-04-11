#include "SettingsManager.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

static const char* settingsPath = "/settings.json";
AppSettings settings;

bool loadSettings() {
  if (!SPIFFS.exists(settingsPath)) return false;

  File file = SPIFFS.open(settingsPath, "r");
  if (!file) return false;

  StaticJsonDocument<1024> json;
  DeserializationError err = deserializeJson(json, file);
  file.close();
  if (err) return false;

  settings.ssid = json["ssid"] | "";
  settings.password = json["password"] | "";
  settings.savedOperationMode = json["mode"] | 1;
  settings.chanelMode = json["chanel"] | 1;
  settings.tg_token = json["tg_token"] | "";
  settings.tg_chatId = json["tg_chatId"] | "";
  settings.mqtt_server = json["mqtt_server"] | "";
  settings.mqtt_topic = json["mqtt_topic"] | "";
  settings.interval_min = json["interval_min"] | 0;

  settings.frameSize = json["frameSize"] | 9;
  settings.jpegQuality = json["jpegQuality"] | 12;
  settings.fbCount = json["fbCount"] | 2;
  settings.vflip = json["vflip"] | true;
  settings.hmirror = json["hmirror"] | false;
  settings.denoise = json["denoise"] | true;
  settings.brightness = json["brightness"] | 0;
  settings.contrast = json["contrast"] | 1;
  settings.saturation = json["saturation"] | 0;
  settings.sharpness = json["sharpness"] | 2;

  return true;
}

bool saveSettings() {
  StaticJsonDocument<1024> json;
  json["ssid"] = settings.ssid;
  json["password"] = settings.password;
  json["mode"] = settings.savedOperationMode;
  json["chanel"] = settings.chanelMode;
  json["tg_token"] = settings.tg_token;
  json["tg_chatId"] = settings.tg_chatId;
  json["mqtt_server"] = settings.mqtt_server;
  json["mqtt_topic"] = settings.mqtt_topic;
  json["interval_min"] = settings.interval_min;

  json["frameSize"] = settings.frameSize;
  json["jpegQuality"] = settings.jpegQuality;
  json["fbCount"] = settings.fbCount;
  json["vflip"] = settings.vflip;
  json["hmirror"] = settings.hmirror;
  json["denoise"] = settings.denoise;
  json["brightness"] = settings.brightness;
  json["contrast"] = settings.contrast;
  json["saturation"] = settings.saturation;
  json["sharpness"] = settings.sharpness;

  File file = SPIFFS.open(settingsPath, "w");
  if (!file) return false;

  serializeJson(json, file);
  file.close();
  return true;
}