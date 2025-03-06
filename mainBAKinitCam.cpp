// main.cpp

#include <WiFi.h>
#include <WebServer.h> // Добавлено для потоковой передачи
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <RTClib.h>
#include <driver/rtc_io.h>
#include "esp_camera.h"
#include <WiFiUdp.h>
#include "time.h"
#include <SPI.h>
#include <DNSServer.h>
#include <HTTPClient.h> // Для отправки HTTP-запросов
#include <WiFiClientSecure.h> // Для HTTPS соединения

// Параметры подключения камеры
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    15
#define XCLK_GPIO_NUM     27
#define SIOD_GPIO_NUM     25
#define SIOC_GPIO_NUM     23

#define Y9_GPIO_NUM       32
#define Y8_GPIO_NUM       35
#define Y7_GPIO_NUM       34
#define Y6_GPIO_NUM       5
#define Y5_GPIO_NUM       39
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       36
#define Y2_GPIO_NUM       19
#define VSYNC_GPIO_NUM    22
#define HREF_GPIO_NUM     26
#define PCLK_GPIO_NUM     21

// Настройки Wi-Fi точки доступа по умолчанию
const char* ap_ssid = "ESP32_Cam_AP";
const char* ap_password = "12345678";

// Настройки NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 5 * 3600; // Смещение по GMT в секундах
const int   daylightOffset_sec = 0;

// Пины RTC BM8563
#define RTC_SDA_PIN 12 // G12
#define RTC_SCL_PIN 14 // G14

// Пин светодиода
#define LED_PIN 2

// Пины для чтения напряжения батареи
#define BAT_ADC_PIN 38 // G38
#define BAT_HOLD_PIN 33 // G33

// Пины для пробуждения по кнопке
#define WAKEUP_PIN GPIO_NUM_37 // G37

// Глобальные переменные и объекты
AsyncWebServer server(80); // Основной веб-сервер для конфигурации
WebServer streamServer(81); // Сервер для потокового вещания на порту 81
RTC_PCF8563 rtc;

// Объекты для HTTPS соединения
WiFiClientSecure client;
int operationMode = 0;
// Переменные для хранения настроек
String ssid = "";
String password = "";
int savedOperationMode = 1; // Значение по умолчанию
int chanelMode = 1;
String tg_token = "";
String tg_chatId = "";
String mqtt_server = "";
String mqtt_topic = "";
String smb_server = "";
String smb_username = "";
String smb_password = "";
int interval_min = 0;

// Файл для сохранения настроек
const char* settingsPath = "/settings.json";

// Прототипы функций
void initCamera();
void initWiFi();
void initWebServer();
void initTime();
void startCameraServer();
void captureAndSend();
void enterDeepSleep(uint64_t sleepTimeS);
float readBatteryVoltage();
String getFileNameWithBatteryLevel();
void setupLED();
void ledIndicator(int mode);
bool loadSettings();
bool saveSettings();
void handleRoot(AsyncWebServerRequest *request);
void handleSaveConfig(AsyncWebServerRequest *request);
void sendToTelegram(camera_fb_t *fb);
void handle_jpg_stream(); // Добавлено для потоковой передачи

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(WAKEUP_PIN, INPUT_PULLUP);

  // Инициализация файловой системы
  if (!SPIFFS.begin(true)) {
    Serial.println("Ошибка монтирования SPIFFS");
    return;
  }

  // Загрузка настроек
  if (!loadSettings()) {
    Serial.println("Не удалось загрузить настройки, используем значения по умолчанию");
  }

  initCamera();
  setupLED();

  initWiFi();

  initWebServer();
  initTime();

  operationMode = savedOperationMode;

  if (operationMode == 1) {
    // Режим потоковой трансляции
    ledIndicator(1);
    startCameraServer();
  } else if (operationMode == 2) {
    // Режим интервальной отправки
    ledIndicator(2);
    captureAndSend();
    enterDeepSleep(interval_min * 60); // Переходим в сон на заданный интервал
  } else {
    // Ожидание подключения
    ledIndicator(0);
  }
}

void loop() {
  if (operationMode == 1) {
    streamServer.handleClient();
  }
  // В остальных режимах действия в loop() не требуются
}

// Функции инициализации камеры
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Настройки разрешения и качества
  config.frame_size = FRAMESIZE_SVGA; // Пример разрешения
  config.jpeg_quality = 12;           // Качество JPEG (0-63), меньше число - выше качество
  config.fb_count = 1;

  // Инициализация камеры
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Ошибка инициализации камеры: %s", esp_err_to_name(err));
    // Обработайте ошибку инициализации
    while (true);
  }
}

// Функция инициализации Wi-Fi
void initWiFi() {
  WiFi.disconnect(true);
  delay(100);

  if (ssid != "") {
    // Пытаемся подключиться к сохраненной сети
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startAttemptTime = millis();

    // Ждем подключения или истечения таймаута
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
      delay(100);
      Serial.print(".");
    }
  }

  // Если не удалось подключиться, запускаем точку доступа
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println("\nТочка доступа запущена");
    Serial.print("IP адрес: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("\nПодключено к Wi-Fi сети");
    Serial.print("IP адрес: ");
    Serial.println(WiFi.localIP());
  }
}

// Функция инициализации веб-сервера
void initWebServer() {
  // Отображение страницы конфигурации при обращении к корню
  server.on("/", HTTP_GET, handleRoot);

  // Обработка сохранения настроек
  server.on("/saveConfig", HTTP_POST, handleSaveConfig);

  // Начало работы сервера конфигурации
  server.begin();
}

// Функции обработки запросов веб-сервера

void handleRoot(AsyncWebServerRequest *request) {
  String html = "<!DOCTYPE html><html><head><title>Config ESP32</title></head><body>";
  html += "<h1>Config Wi-Fi and work mode</h1>";
  html += "<form action='/saveConfig' method='POST'>";
  html += "SSID: <input type='text' name='ssid' value='" + ssid + "'><br>";
  html += "Password: <input type='password' name='password' value='" + password + "'><br>";
  html += "Work mode: ";
  html += "<select name='mode'>";
  html += "<option value='1'" + String(savedOperationMode == 1 ? " selected" : "") + ">Stream mode</option>";
  html += "<option value='2'" + String(savedOperationMode == 2 ? " selected" : "") + ">Interval</option>";
  html += "</select><br>";
  html += "Channel: ";
  html += "<select name='chanel'>";
  html += "<option value='1'" + String(chanelMode == 1 ? " selected" : "") + ">Telegram</option>";
  html += "<option value='2'" + String(chanelMode == 2 ? " selected" : "") + ">MQTT</option>";
  html += "<option value='3'" + String(chanelMode == 3 ? " selected" : "") + ">SMB</option>";
  html += "</select><br>";
  html += "Telegram token: <input type='text' name='tg_token' value='" + String(tg_token) + "'><br>";
  html += "Telegram Chat ID: <input type='text' name='tg_chatId' value='" + String(tg_chatId) + "'><br>";
  html += "MQTT server: <input type='text' name='mqtt_server' value='" + String(mqtt_server) + "'><br>";
  html += "MQTT topic: <input type='text' name='mqtt_topic' value='" + String(mqtt_topic) + "'><br>";
  html += "SMB server: <input type='text' name='smb_server' value='" + String(smb_server) + "'><br>";
  html += "SMB username: <input type='text' name='smb_username' value='" + String(smb_username) + "'><br>";
  html += "SMB password: <input type='password' name='smb_password' value='" + String(smb_password) + "'><br>";
  html += "Interval (min): <input type='number' name='interval_min' value='" + String(interval_min) + "'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "</body></html>";
  request->send(200, "text/html", html);
}

void handleSaveConfig(AsyncWebServerRequest *request) {
  bool hasChanged = false;
  if (request->hasParam("ssid", true)) {
    ssid = request->getParam("ssid", true)->value();
    hasChanged = true;
  }
  if (request->hasParam("password", true)) {
    password = request->getParam("password", true)->value();
    hasChanged = true;
  }
  if (request->hasParam("mode", true)) {
    savedOperationMode = request->getParam("mode", true)->value().toInt();
    hasChanged = true;
  }
  if (request->hasParam("chanel", true)) {
    chanelMode = request->getParam("chanel", true)->value().toInt();
    hasChanged = true;
  }
  if (request->hasParam("tg_token", true)) {
    tg_token = request->getParam("tg_token", true)->value();
    hasChanged = true;
  }
  if (request->hasParam("tg_chatId", true)) {
    tg_chatId = request->getParam("tg_chatId", true)->value();
    hasChanged = true;
  }
  if (request->hasParam("interval_min", true)) {
    interval_min = request->getParam("interval_min", true)->value().toInt();
    hasChanged = true;
  }

  // Сохраняем настройки
  if (hasChanged && saveSettings()) {
    request->send(200, "text/plain", "Настройки сохранены. Перезагрузка...");
    delay(1000);
    ESP.restart();
  } else {
    request->send(500, "text/plain", "Ошибка сохранения настроек");
  }
}

// Функция загрузки настроек из SPIFFS
bool loadSettings() {
  if (!SPIFFS.exists(settingsPath)) {
    Serial.println("Файл настроек не найден");
    return false;
  }

  File file = SPIFFS.open(settingsPath, "r");
  if (!file) {
    Serial.println("Не удалось открыть файл настроек для чтения");
    return false;
  }

  size_t size = file.size();
  if (size > 2048) {
    Serial.println("Файл настроек слишком большой");
    file.close();
    return false;
  }

  // Чтение файла в буфер
  std::unique_ptr<char[]> buf(new char[size + 1]);
  file.readBytes(buf.get(), size);
  file.close();

  // Разбор JSON
  StaticJsonDocument<1024> json;
  auto error = deserializeJson(json, buf.get());
  if (error) {
    Serial.println("Ошибка разбора файла настроек");
    return false;
  }

  // Получение значений
  ssid = json["ssid"].as<String>();
  password = json["password"].as<String>();
  savedOperationMode = json["mode"].as<int>();
  chanelMode = json["chanel"].as<int>();
  tg_token = json["tg_token"].as<String>();
  tg_chatId = json["tg_chatId"].as<String>();
  interval_min = json["interval_min"].as<int>();

  Serial.println("Настройки загружены");
  return true;
}

// Функция сохранения настроек в SPIFFS
bool saveSettings() {
  StaticJsonDocument<1024> json;
  json["ssid"] = ssid;
  json["password"] = password;
  json["mode"] = savedOperationMode;
  json["chanel"] = chanelMode;
  json["tg_token"] = tg_token;
  json["tg_chatId"] = tg_chatId;
  json["interval_min"] = interval_min;

  File file = SPIFFS.open(settingsPath, "w");
  if (!file) {
    Serial.println("Ошибка открытия файла настроек для записи");
    return false;
  }

  if (serializeJson(json, file) == 0) {
    Serial.println("Ошибка записи настроек в файл");
    file.close();
    return false;
  }

  file.close();
  Serial.println("Настройки сохранены");
  return true;
}

// Функция инициализации времени через NTP и RTC
void initTime() {
  // Инициализация RTC
  Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN); // SDA на G12, SCL на G14
  if (!rtc.begin()) {
    Serial.println("RTC не найден");
  }

  // Синхронизация времени через NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {
    // Установка времени на RTC
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    Serial.println("Время синхронизировано с NTP");
  } else {
    Serial.println("Ошибка получения времени через NTP");
  }
}

// Функция запуска сервера потоковой трансляции
void startCameraServer() {
  streamServer.on("/stream", handle_jpg_stream);
  streamServer.begin();
  Serial.println("Сервер потоковой трансляции запущен на порту 81");
}

// Функция обработки потоковой передачи
void handle_jpg_stream() {
  WiFiClient client = streamServer.client();

  String response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
    "\r\n";
  client.print(response);

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Ошибка захвата кадра");
      break;
    }

    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.write("\r\n");

    esp_camera_fb_return(fb);

    if (!client.connected()) {
      break;
    }

    delay(50); // Настройте задержку для контроля частоты кадров
  }

  client.stop();
  Serial.println("Клиент отключился");
}

// Функция захвата и отправки снимка
void captureAndSend() {
  // Захват изображения
  camera_fb_t * fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Ошибка получения кадра");
    return;
  }

  // Формируем имя файла с учетом даты, времени и напряжения батареи
  String fileName = getFileNameWithBatteryLevel();

  // Отправка файла в соответствии с выбранным каналом
  if (chanelMode == 1) {
    // Отправка в Telegram-бот
    sendToTelegram(fb);
  } else if (chanelMode == 2) {
    // Отправка в MQTT
    // publishToMQTT(fb);
  } else if (chanelMode == 3) {
    // Отправка по Samba
    // sendToSamba(fb, fileName);
  }

  // Освобождаем буфер
  esp_camera_fb_return(fb);

  Serial.println("Снимок обработан");
}

// Функция отправки изображения в Telegram
void sendToTelegram(camera_fb_t *fb) {
  if (tg_token == "" || tg_chatId == "") {
    Serial.println("Токен бота или ID чата не указаны");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Отключаем проверку сертификата

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("Ошибка подключения к Telegram API");
    return;
  }

  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String dataStart = "";
  dataStart += "--" + boundary + "\r\n";
  dataStart += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  dataStart += tg_chatId + "\r\n";
  dataStart += "--" + boundary + "\r\n";
  dataStart += "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n";
  dataStart += "Content-Type: image/jpeg\r\n\r\n";

  String dataEnd = "\r\n--" + boundary + "--\r\n";

  // Формируем HTTP-запрос
  String requestHeader = "";
  requestHeader += "POST /bot" + tg_token + "/sendPhoto HTTP/1.1\r\n";
  requestHeader += "Host: api.telegram.org\r\n";
  requestHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  requestHeader += "Content-Length: " + String(dataStart.length() + fb->len + dataEnd.length()) + "\r\n";
  requestHeader += "\r\n";

  // Отправляем запрос
  client.print(requestHeader + dataStart);
  client.write(fb->buf, fb->len);
  client.print(dataEnd);

  // Читаем ответ сервера
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  String response = client.readString();
  Serial.println("Ответ сервера Telegram:");
  Serial.println(response);
}

// Функция перехода в глубокий сон
void enterDeepSleep(uint64_t sleepTimeS) {
  // Настраиваем пробуждение по таймеру
  esp_sleep_enable_timer_wakeup(sleepTimeS * 1000000);

  // Пробуждение по кнопке на G37
  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 0); // Будем просыпаться при низком уровне сигнала

  Serial.println("Уходим в глубокий сон...");

  // Отключаем питание камеры для экономии энергии
  esp_camera_deinit();

  // Переходим в глубокий сон
  esp_deep_sleep_start();
}

// Функция чтения напряжения батареи
float readBatteryVoltage() {
  // Предполагаем, что напряжение батареи делится перед подачей на ADC
  uint16_t raw = analogRead(BAT_ADC_PIN);
  float voltage = ((float)raw / 4095.0) * 3.3 * 2; // Учитываем делитель напряжения
  return voltage;
}

// Функция получения имени файла со временем и напряжением батареи
String getFileNameWithBatteryLevel() {
  DateTime now = rtc.now();
  float voltage = readBatteryVoltage();
  char buf[50];
  sprintf(buf, "%02d%02d%02d_%02d%02d_%.2fV.jpg",
          now.day(), now.month(), now.year() % 100,
          now.hour(), now.minute(),
          voltage);
  return String(buf);
}

// Функции управления светодиодом
void setupLED() {
  pinMode(LED_PIN, OUTPUT);
}

void ledIndicator(int mode) {
  // mode: 0 - ожидание подключения, 1 - потоковая трансляция, 2 - интервальная отправка
  switch(mode) {
    case 0:
      digitalWrite(LED_PIN, HIGH); // Горит постоянно
      break;
    case 1:
      // Мигание 2 раза в секунду
      digitalWrite(LED_PIN, millis() % 500 < 250);
      break;
    case 2:
      // Мигание 4 раза в секунду
      digitalWrite(LED_PIN, millis() % 250 < 125);
      break;
  }
}

// Дополнительные функции отправки данных
// void sendToSamba(camera_fb_t *fb, String fileName) {
//   // Реализуйте отправку файла по Samba
// }

// void publishToMQTT(camera_fb_t *fb) {
//   // Реализуйте публикацию данных в MQTT
// }
