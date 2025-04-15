// Utils.cpp
#include <Arduino.h>
#include "Utils.h"

extern RTC_PCF8563 rtc; // Объявляем внешний объект RTC

// Инициализация светодиода
void setupLED() {
  pinMode(LED_PIN, OUTPUT);
}

// Индикация состояния
void ledIndicator(int mode) {
  switch(mode) {
    case 0:
      digitalWrite(LED_PIN, HIGH); // Постоянно горит
      break;
    case 1:
      // Мигание 2 раза в секунду
      digitalWrite(LED_PIN, (millis() % 500 < 250) ? HIGH : LOW);
      break;
    case 2:
      // Мигание 4 раза в секунду
      digitalWrite(LED_PIN, (millis() % 250 < 125) ? HIGH : LOW);
      break;
  }
}

// Чтение напряжения батареи
float readBatteryVoltage() {
  uint16_t raw = analogRead(BAT_ADC_PIN);
  float voltage = ((float)raw / 4095.0) * 3.3 / 0.661;
  Serial.println("Напряжение батареи: " + String(voltage));
  return voltage;
}

String getFileNameWithBatteryLevel() {
  DateTime now = rtc.now();
  float voltage = readBatteryVoltage();

  char buf[50];
  snprintf(buf, sizeof(buf), "%02d%02d%02d_%02d%02d_%.2fV.jpg",
           now.day(), now.month(), now.year() % 100,
           now.hour(), now.minute(), voltage);

  return String(buf);
}