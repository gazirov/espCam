#pragma once
#include <Arduino.h>

struct AppSettings {
  String ssid;
  String password;
  int savedOperationMode;
  int chanelMode;
  String tg_token;
  String tg_chatId;
  String mqtt_server;
  String mqtt_topic;
  int interval_min;

  // Camera
  int frameSize;
  int jpegQuality;
  int fbCount;
  bool vflip;
  bool hmirror;
  bool denoise;
  int brightness;
  int contrast;
  int saturation;
  int sharpness;
};

extern AppSettings settings;

bool loadSettings();
bool saveSettings();