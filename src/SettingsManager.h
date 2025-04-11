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
  String seafile_host;
  String seafile_user;
  String seafile_pass;
  String seafile_token;
  String seafile_repo_id;
  //String smb_server;
  //String smb_username;
  //String smb_password;
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