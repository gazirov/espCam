// Utils.h
#pragma once
#include <RTClib.h>
#include <Arduino.h>

// Пины
#define LED_PIN       2
#define BAT_ADC_PIN   38

void setupLED();
void ledIndicator(int mode);
float readBatteryVoltage();
String getFileNameWithBatteryLevel();