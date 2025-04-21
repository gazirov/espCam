// Utils.h
#pragma once
#include <RTClib.h>
#include <Arduino.h>

// Пины
#define LED_PIN       2
#define BAT_ADC_PIN   38// G38
#define POWER_HOLD_PIN 33 // G33

void setupLED();
void ledIndicator(int mode);
float readBatteryVoltage();
String getFileNameWithBatteryLevel();