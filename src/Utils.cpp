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
float readBatteryVoltage(uint8_t samples = 5) {
  uint32_t sum = 0;

  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(BAT_ADC_PIN);
    delay(5); // короткая задержка для стабильности
  }

  float avgRaw = sum / (float)samples;
  float voltage = (avgRaw / 4095.0) * 3.3 / 0.661;

  Serial.printf("Напряжение батареи: %.2f В (avg raw: %.0f)\n", voltage, avgRaw);
  return voltage;
}

String getFileNameWithBatteryLevel() {
  DateTime now = rtc.now();
  float voltage = readBatteryVoltage(10);

  char buf[50];
  snprintf(buf, sizeof(buf), "%02d%02d%02d_%02d%02d_%.2fV.jpg",
           now.day(), now.month(), now.year() % 100,
           now.hour(), now.minute(), voltage);

  return String(buf);
}