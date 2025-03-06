// main.cpp
#include "mbedtls/base64.h"
#include <WiFi.h>
#include <WebServer.h> // Добавлено для потоковой передачи
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h> // Для ESP32
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
#include <PubSubClient.h> //mqtt



// Параметры подключения камеры
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    15
#define XCLK_GPIO_NUM     27
#define SIOD_GPIO_NUM     25
#define SIOC_GPIO_NUM     23
#define D0_GPIO_NUM       32
#define D1_GPIO_NUM       35
#define D2_GPIO_NUM       34
#define D3_GPIO_NUM        5
#define D4_GPIO_NUM       39
#define D5_GPIO_NUM       18
#define D6_GPIO_NUM       36
#define D7_GPIO_NUM       19
#define VSYNC_GPIO_NUM    22
#define HREF_GPIO_NUM     26
#define PCLK_GPIO_NUM     21

// Определение CAM_PIN_
#define CAM_PIN_PWDN    PWDN_GPIO_NUM
#define CAM_PIN_RESET   RESET_GPIO_NUM
#define CAM_PIN_XCLK    XCLK_GPIO_NUM
#define CAM_PIN_SIOD    SIOD_GPIO_NUM
#define CAM_PIN_SIOC    SIOC_GPIO_NUM
#define CAM_PIN_D0      D0_GPIO_NUM
#define CAM_PIN_D1      D1_GPIO_NUM
#define CAM_PIN_D2      D2_GPIO_NUM
#define CAM_PIN_D3      D3_GPIO_NUM
#define CAM_PIN_D4      D4_GPIO_NUM
#define CAM_PIN_D5      D5_GPIO_NUM
#define CAM_PIN_D6      D6_GPIO_NUM
#define CAM_PIN_D7      D7_GPIO_NUM
#define CAM_PIN_VSYNC   VSYNC_GPIO_NUM
#define CAM_PIN_HREF    HREF_GPIO_NUM
#define CAM_PIN_PCLK    PCLK_GPIO_NUM

#define BASE_VOLATAGE     3600
#define SCALE             0.661
#define ADC_FILTER_SAMPLE 64


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
#define POWER_HOLD_PIN 33 // G33

// Пины для пробуждения по кнопке
//в документации указан не корректный номер пина
#define WAKEUP_PIN GPIO_NUM_38 // G38

// Глобальные переменные и объекты
AsyncWebServer server(80); // Основной веб-сервер для конфигурации
AsyncWebServer streamServer(81); // Сервер для потокового вещания на порту 81

RTC_PCF8563 rtc;
//mqtt
WiFiClient espClient;
PubSubClient mqttClient(espClient);

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
void sendToTelegram(camera_fb_t *fb, const String& fileName);
void handle_jpg_stream(AsyncWebServerRequest *request); // Добавлено для потоковой передачи
void connectToMQTT();
void initMQTT();
void publishToMQTT(camera_fb_t *fb);

void setup() {
  Serial.begin(115200);
  delay(1000);
/*
  pinMode(WAKEUP_PIN, INPUT_PULLUP);
  digitalWrite(WAKEUP_PIN, HIGH);

  int pinState = digitalRead(WAKEUP_PIN);
  Serial.print("Начальное состояние WAKEUP_PIN: ");
  Serial.println(pinState == HIGH ? "HIGH" : "LOW");
*/
  // Инициализируем RTC GPIO для WAKEUP_PIN
  rtc_gpio_init(WAKEUP_PIN);
  rtc_gpio_set_direction(WAKEUP_PIN, RTC_GPIO_MODE_INPUT_ONLY);

  // Отключаем внутренние подтяжки, так как они не работают на этих пинах
  rtc_gpio_pulldown_dis(WAKEUP_PIN);
  rtc_gpio_pullup_dis(WAKEUP_PIN);
  int pinState = rtc_gpio_get_level(WAKEUP_PIN);
  Serial.printf("Начальное состояние WAKEUP_PIN (GPIO%u): %s\n", WAKEUP_PIN, pinState ? "HIGH" : "LOW");

  // Пины для питания батареи
  pinMode(POWER_HOLD_PIN, OUTPUT);
  digitalWrite(POWER_HOLD_PIN, HIGH);


  // Инициализация файловой системы
  if (!SPIFFS.begin(true)) {
    Serial.println("Ошибка монтирования SPIFFS");
    return;
  }

  // Проверяем причину пробуждения
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Проснулся по кнопке");
  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Проснулся по таймеру");
  } else {
    Serial.println("Проснулся по неизвестной причине");
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
  // if not connect, going to stream mode
  if (WiFi.status() == WL_CONNECTED){
    // Условная инициализация MQTT
    if (chanelMode == 2 && operationMode == 2) {
      Serial.println("Инициализация MQTT...");
      initMQTT();
    }
    else if (operationMode == 2) {
      // Режим интервальной отправки
      ledIndicator(2);
      captureAndSend();
      enterDeepSleep(interval_min * 60); // Переходим в сон на заданный интервал
    }
  }
  //переход в стрим режим если не подключилось к вайфаю 
  //в случае если есть проблемы с вайфаем то надо заккоментировать чтоб не уходить 
  //operationMode = 1;

  if (operationMode == 1) {
    // Режим потоковой трансляции
    ledIndicator(1);
    startCameraServer();
  }   else {
    // Ожидание подключения
    ledIndicator(0);
  }
}

void loop() {
  // В остальных режимах действия в loop() не требуются
int currentMode = operationMode;

  // Обработка MQTT-подключения и сообщений только в режиме интервальной отправки
  if (currentMode == 2 && chanelMode == 2) {
    // Если потоковая трансляция и используем MQTT
    if (!mqttClient.connected()) {
      connectToMQTT();
    }
    mqttClient.loop();
  }

ledIndicator(currentMode);
 delay(10);
}

// Функции инициализации камеры
void initCamera() {
  // Настройка конфигурации камеры
  camera_config_t camera_config = {
    .pin_pwdn     = CAM_PIN_PWDN,
    .pin_reset    = CAM_PIN_RESET,
    .pin_xclk     = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,
    .pin_d7       = CAM_PIN_D7,
    .pin_d6       = CAM_PIN_D6,
    .pin_d5       = CAM_PIN_D5,
    .pin_d4       = CAM_PIN_D4,
    .pin_d3       = CAM_PIN_D3,
    .pin_d2       = CAM_PIN_D2,
    .pin_d1       = CAM_PIN_D1,
    .pin_d0       = CAM_PIN_D0,
    .pin_vsync    = CAM_PIN_VSYNC,
    .pin_href     = CAM_PIN_HREF,
    .pin_pclk     = CAM_PIN_PCLK,

    // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 10000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, // YUV422, GRAYSCALE, RGB565, JPEG
    .frame_size   = FRAMESIZE_VGA, //FRAMESIZE_XGA,  // QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 14, // 0-63 lower number means higher quality
    .fb_count     = 3,  // if more than one, i2s runs in continuous mode. Use only with JPEG
    .grab_mode    = CAMERA_GRAB_LATEST //CAMERA_GRAB_WHEN_EMPTY,
  };

  Serial.println("Инициализация камеры...");

  // Инициализация камеры
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.printf("Ошибка инициализации камеры: %s\n", esp_err_to_name(err));
    // Обработайте ошибку инициализации
    while (true) {
      delay(1000); // Задержка, чтобы не перегружать цикл
    }
  } else {
    Serial.println("Камера успешно инициализирована");
  }

  // Получение информации о сенсоре камеры
  sensor_t * s = esp_camera_sensor_get();
  //s->set_hmirror(s, 1);
  s->set_vflip(s, 1);
  
  s->set_exposure_ctrl(s, 1);
  s->set_aec2(s, 1);
  s->set_ae_level(s, 0); 
 // s->set_auto_gain_ctrl(s, 1);
 // s->set_gainceiling(s, (gainceiling_t)6);
  s->set_awb_gain(s, 1);
  s->set_whitebal(s, 1);
  s->set_contrast(s, 1);
  s->set_brightness(s, 0);
  s->set_saturation(s, 0);
  s->set_sharpness(s, 2);
  s->set_denoise(s, 1);    

  if (s != nullptr) {
    Serial.printf("Модель сенсора камеры: %s\n", (s->id.PID == OV3660_PID) ? "OV3660" :
                                                (s->id.PID == OV2640_PID) ? "OV2640" :
                                                (s->id.PID == OV7725_PID) ? "OV7725" :
                                                "Неизвестная");
  } else {
    Serial.println("Ошибка получения информации о сенсоре камеры");
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
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
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
String html = "<!DOCTYPE html><html lang='en'><head>";
html += "<meta charset='UTF-8'>";
html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
html += "<title>Config ESP32</title>";
html += "<style>";
html += "body { font-family: Arial, sans-serif; margin: 40px; }";
html += "h1 { text-align: center; color: #333; }";
html += "form { max-width: 600px; margin: auto; background: #f9f9f9; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }";
html += "fieldset { border: 1px solid #ccc; padding: 20px; margin-bottom: 20px; border-radius: 5px; }";
html += "legend { font-weight: bold; color: #333; }";
html += "label { display: block; margin: 10px 0 5px; }";
html += "input[type='text'], input[type='password'], input[type='number'], select { width: 100%; padding: 10px; margin: 5px 0 20px 0; border: 1px solid #ccc; border-radius: 5px; }";
html += "input[type='submit'] { background-color: #4CAF50; color: white; padding: 15px 20px; border: none; border-radius: 5px; cursor: pointer; }";
html += "input[type='submit']:hover { background-color: #45a049; }";
html += "</style>";
html += "</head><body>";
html += "<h1>Config Wi-Fi and work mode</h1>";
html += "<form action='/saveConfig' method='POST'>";

html += "<fieldset>";
html += "<legend>Wi-Fi Settings</legend>";
html += "<label for='ssid'>SSID:</label><input type='text' id='ssid' name='ssid' value='" + ssid + "'>";
html += "<label for='password'>Password:</label><input type='password' id='password' name='password' value='" + password + "'>";
html += "</fieldset>";

html += "<fieldset>";
html += "<legend>Work Mode Settings</legend>";
html += "<label for='mode'>Work mode:</label>";
html += "<select name='mode' id='mode'>";
html += "<option value='1'" + String(savedOperationMode == 1 ? " selected" : "") + ">Stream mode</option>";
html += "<option value='2'" + String(savedOperationMode == 2 ? " selected" : "") + ">Interval</option>";
html += "</select>";
html += "<label for='chanel'>Channel:</label>";
html += "<select name='chanel' id='chanel'>";
html += "<option value='1'" + String(chanelMode == 1 ? " selected" : "") + ">Telegram</option>";
html += "<option value='2'" + String(chanelMode == 2 ? " selected" : "") + ">MQTT</option>";
html += "<option value='3'" + String(chanelMode == 3 ? " selected" : "") + ">SMB</option>";
html += "</select>";
html += "<legend>Interval Settings</legend>";
html += "<label for='interval_min'>Interval (min):</label><input type='number' id='interval_min' name='interval_min' value='" + String(interval_min) + "'>";
html += "</fieldset>";


html += "<fieldset>";
html += "<legend>Telegram Settings</legend>";
html += "<label for='tg_token'>Telegram token:</label><input type='text' id='tg_token' name='tg_token' value='" + String(tg_token) + "'>";
html += "<label for='tg_chatId'>Telegram Chat ID:</label><input type='text' id='tg_chatId' name='tg_chatId' value='" + String(tg_chatId) + "'>";
html += "</fieldset>";

html += "<fieldset>";
html += "<legend>MQTT Settings</legend>";
html += "<label for='mqtt_server'>MQTT server:</label><input type='text' id='mqtt_server' name='mqtt_server' value='" + String(mqtt_server) + "'>";
html += "<label for='mqtt_topic'>MQTT topic:</label><input type='text' id='mqtt_topic' name='mqtt_topic' value='" + String(mqtt_topic) + "'>";
html += "</fieldset>";

html += "<fieldset>";
html += "<legend>SMB Settings NOT Realized</legend>";
html += "<label for='smb_server'>SMB server:</label><input type='text' id='smb_server' name='smb_server' value='" + String(smb_server) + "'>";
html += "<label for='smb_username'>SMB username:</label><input type='text' id='smb_username' name='smb_username' value='" + String(smb_username) + "'>";
html += "<label for='smb_password'>SMB password:</label><input type='password' id='smb_password' name='smb_password' value='" + String(smb_password) + "'>";
html += "</fieldset>";

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
    if (request->hasParam("mqtt_server", true)) {
    mqtt_server = request->getParam("mqtt_server", true)->value();
    hasChanged = true;
  }
    if (request->hasParam("mqtt_topic", true)) {
    mqtt_topic = request->getParam("mqtt_topic", true)->value();
    hasChanged = true;
  }

  // save settings
  if (hasChanged && saveSettings()) {
    request->send(200, "text/plain", "Setting saved. Reloading...");
    delay(1000);
    ESP.restart();
  } else {
    request->send(500, "text/plain", "Failed to save settings. Please try again.");
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
  mqtt_server = json["mqtt_server"].as<String>();
  mqtt_topic = json["mqtt_topic"].as<String>();

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
  json["mqtt_server"] = mqtt_server;
  json["mqtt_topic"] = mqtt_topic;

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

void initMQTT() {
  if (mqtt_server == "") {
    Serial.println("MQTT сервер не настроен");
    return;
  }
  
  mqttClient.setServer(mqtt_server.c_str(), 1883); // Используем стандартный порт 1883
  connectToMQTT();
}
void connectToMQTT() {
  const int maxAttempts = 15;
  int attempt = 0;

  while (!mqttClient.connected() && attempt < maxAttempts) {
    Serial.print("Подключаемся к MQTT-брокеру... (Попытка ");
    Serial.print(attempt + 1);
    Serial.println(" из 15)");

    String clientId = "ESP32Cam_" + String(random(0xffff), HEX);

    // Если требуется аутентификация, используйте:
    // if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println(" Подключено к MQTT-брокеру");
      // Если нужно подписаться на топики, делаем это здесь
      // mqttClient.subscribe("your/client/topic");
      break; // Выходим из цикла, поскольку подключение успешно
    } else {
      Serial.print("Ошибка подключения, код ошибки = ");
      Serial.println(mqttClient.state());
      attempt++;
      if (attempt < maxAttempts) {
        Serial.println(" Попробуем снова через 10 секунд");
        delay(10000);
      } else {
        Serial.println(" Достигнуто максимальное количество попыток подключения. Подключение не удалось.");
      }
    }
  }
}

// Функция запуска сервера потоковой трансляции
void startCameraServer() {
  // Обработчик для корневого пути
  streamServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "Пожалуйста, перейдите по пути /stream для просмотра видео.");
  });

  // Обработчик для /stream
  streamServer.on("/stream", HTTP_GET, handle_jpg_stream);

  streamServer.begin();
  Serial.println("Сервер потоковой трансляции запущен на порту 81");
}

// Функция обработки потоковой передачи
void handle_jpg_stream(AsyncWebServerRequest *request) {
  Serial.println("Клиент подключился для потоковой передачи");

  // Используем статические переменные для хранения состояния между вызовами
  static camera_fb_t * fb = NULL;
  static size_t fb_pos = 0;
  static String header = "";
  static size_t header_pos = 0;
  static int64_t last_frame = 0;

  AsyncWebServerResponse *response = request->beginChunkedResponse("multipart/x-mixed-replace; boundary=frame",
    [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      size_t len = 0;

      if(fb == NULL) {
        // Получаем новый кадр
        fb = esp_camera_fb_get();
        if (!fb) {
          Serial.println("Ошибка захвата кадра");
          return 0;
        }
        // Формируем заголовок для кадра
        header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + String(fb->len) + "\r\n\r\n";
        header_pos = 0;
        fb_pos = 0;
      }

      // Сначала отправляем часть заголовка, если он еще не весь отправлен
      if(header_pos < header.length()) {
        len = header.length() - header_pos;
        if(len > maxLen) len = maxLen;
        memcpy(buffer, header.c_str() + header_pos, len);
        header_pos += len;
        return len;
      }

      // Затем отправляем часть данных кадра
      if(fb_pos < fb->len) {
        len = fb->len - fb_pos;
        if(len > maxLen) len = maxLen;
        memcpy(buffer, fb->buf + fb_pos, len);
        fb_pos += len;
        return len;
      }

      // После кадра отправляем "\r\n"
      if(fb_pos == fb->len) {
        const char* footer = "\r\n";
        size_t footer_len = 2;
        size_t footer_sent = index - header.length() - fb->len;
        if(footer_sent < footer_len) {
          len = footer_len - footer_sent;
          if(len > maxLen) len = maxLen;
          memcpy(buffer, footer + footer_sent, len);
          return len;
        } else {
          // Кадр полностью отправлен, возвращаем fb и обнуляем переменные
          esp_camera_fb_return(fb);
          fb = NULL;
          fb_pos = 0;
          header = "";
          header_pos = 0;
          return 0; // Начинаем следующий кадр
        }
      }

      return 0;
    });

  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);

  Serial.println("Запущена потоковая передача");
}

// Функция захвата и отправки снимка
void captureAndSend() {
  Serial.println("Начало захвата изображения...");

  // Захват изображения
  camera_fb_t * fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Ошибка получения кадра: esp_camera_fb_get() вернул NULL");
    return;
  } else {
    Serial.println("Кадр успешно захвачен");
    Serial.printf("Размер кадра: %u байт\n", fb->len);
    Serial.printf("Разрешение кадра: %u x %u\n", fb->width, fb->height);
    Serial.printf("Формат кадра: %d\n", fb->format); // Выводим формат пикселей
  }

  // Формируем имя файла с учетом даты, времени и напряжения батареи
  String fileName = getFileNameWithBatteryLevel();
  Serial.printf("Имя файла: %s\n", fileName.c_str());

  // Отправка файла в соответствии с выбранным каналом
  if (chanelMode == 1) {
    Serial.println("Отправка изображения в Telegram...");
    // Отправка в Telegram-бот
    sendToTelegram(fb, fileName);
  } else if (chanelMode == 2) {
    Serial.println("Отправка изображения в MQTT...");
    // Отправка в MQTT
     publishToMQTT(fb);
  } else if (chanelMode == 3) {
    Serial.println("Отправка изображения по Samba...");
    // Отправка по Samba
    // sendToSamba(fb, fileName);
  } else {
    Serial.println("Неизвестный канал отправки");
  }

  // Освобождаем буфер
  esp_camera_fb_return(fb);

  Serial.println("Снимок обработан");
}

// Функция отправки изображения в Telegram
void sendToTelegram(camera_fb_t *fb, const String& fileName) {
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

  // Объявляем переменные
  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String dataStart = "";
  dataStart += "--" + boundary + "\r\n";
  dataStart += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
  dataStart += tg_chatId + "\r\n";

  // Добавляем поле caption с именем файла
  dataStart += "--" + boundary + "\r\n";
  dataStart += "Content-Disposition: form-data; name=\"caption\"\r\n\r\n";
  dataStart += fileName + "\r\n";

  dataStart += "--" + boundary + "\r\n";
  dataStart += "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n";
  dataStart += "Content-Type: image/jpeg\r\n\r\n";

  String dataEnd = "\r\n--" + boundary + "--\r\n";

  // Вычисляем длину тела запроса
  size_t contentLength = dataStart.length() + fb->len + dataEnd.length();

  // Формируем заголовки HTTP-запроса
  String requestHeader = "";
  requestHeader += "POST /bot" + tg_token + "/sendPhoto HTTP/1.1\r\n";
  requestHeader += "Host: api.telegram.org\r\n";
  requestHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
  requestHeader += "Content-Length: " + String(contentLength) + "\r\n";
  requestHeader += "\r\n"; // Пустая строка разделяет заголовки и тело

  // Отправляем заголовки
  client.print(requestHeader);

  // Отправляем тело запроса
  client.print(dataStart);

  // Отправляем данные изображения по частям, чтобы избежать проблем с памятью
  const size_t bufferSize = 1024;
  size_t bytesSent = 0;
  while (bytesSent < fb->len) {
    size_t chunkSize = min(bufferSize, fb->len - bytesSent);
    client.write(fb->buf + bytesSent, chunkSize);
    bytesSent += chunkSize;
  }

  client.print(dataEnd);

  // Читаем ответ сервера
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  String response = client.readString();
  //Serial.println("Ответ сервера Telegram:");
  //Serial.println(response);

  client.stop(); // Закрываем соединение
}


// Функция перехода в глубокий сон
void enterDeepSleep(uint64_t sleepTimeS) {
  // Настраиваем пробуждение по таймеру
  esp_sleep_enable_timer_wakeup(sleepTimeS * 1000000);

  // Пробуждение по кнопке на G37
  //не работает в случае если значенин 0 то тут же просыпается
  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, LOW); // Будем просыпаться при низком уровне сигнала

  // Отключаем ADC на GPIO38
 // adc_power_off();

  // Переинициализируем GPIO38 как RTC GPIO
  rtc_gpio_deinit(WAKEUP_PIN);
  rtc_gpio_init(WAKEUP_PIN);
  rtc_gpio_set_direction(WAKEUP_PIN, RTC_GPIO_MODE_INPUT_ONLY);

  rtc_gpio_pulldown_dis(WAKEUP_PIN);
  rtc_gpio_pullup_dis(WAKEUP_PIN);

  // Определяем битовую маску для GPIO38 (RTC_GPIO2)
  uint64_t BUTTON_PIN_BITMASK = 1ULL << 2; // RTC_GPIO2 соответствует биту 2

  // Настраиваем пробуждение по внешнему сигналу на GPIO38
  // Устройство просыпается, когда GPIO38 переходит в LOW
  //esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);

   int pinState = rtc_gpio_get_level(WAKEUP_PIN);
  Serial.printf("Текущее состояние WAKEUP_PIN (GPIO%u): %s\n", WAKEUP_PIN, pinState ? "HIGH" : "LOW");
  //Serial.printf(" WAKEUP_PIN: GPIO%u (RTC_GPIO2), Битовая маска: 0x%016llX\n", WAKEUP_PIN, BUTTON_PIN_BITMASK);


  Serial.println("Уходим в глубокий сон...");

  // Отключаем питание камеры для экономии энергии
  esp_camera_deinit();

  // Удерживаем состояние BAT_HOLD_PIN
  gpio_hold_en((gpio_num_t)POWER_HOLD_PIN);
 // gpio_deep_sleep_hold_en();


  // Переходим в глубокий сон
  delay(15000);
  esp_deep_sleep_start();
}

// Функция чтения напряжения батареи
float readBatteryVoltage() {
  uint16_t raw = analogRead(BAT_ADC_PIN);
  float voltage = ((float)raw / 4095.0) * 3.3 / 0.661; // Учитываем делитель напряжения
 
  Serial.println("Напряжение батареи: " + String(voltage));

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
      digitalWrite(LED_PIN, HIGH); // Светодиод горит постоянно
      break;
    case 1:
      // Мигание 2 раза в секунду
      if (millis() % 500 < 250) {
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }
      break;
    case 2:
      // Мигание 4 раза в секунду
      if (millis() % 250 < 125) {
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }
      break;
  }
}

// Дополнительные функции отправки данных
// void sendToSamba(camera_fb_t *fb, String fileName) {
//   // Реализуйте отправку файла по Samba
// }

void publishToMQTT(camera_fb_t *fb) {
  if (mqtt_server == "" || mqtt_topic == "") {
    Serial.println("MQTT сервер или топик не настроены");
    return;
  }

  if (!mqttClient.connected()) {
    connectToMQTT();
  }

  mqttClient.loop();

  // Вычисляем необходимый размер для закодированных данных
  size_t encodedLen = 4 * ((fb->len + 2) / 3);

  // Выделяем память для закодированных данных (+1 для завершающего нуля)
  unsigned char* encodedImage = (unsigned char*)malloc(encodedLen + 1);
  if (encodedImage == nullptr) {
    Serial.println("Ошибка выделения памяти для закодированного изображения");
    return;
  }

  size_t outputLen = 0;

  // Кодируем изображения в Base64
  int ret = mbedtls_base64_encode(encodedImage, encodedLen, &outputLen, fb->buf, fb->len);
  if (ret != 0) {
    Serial.printf("Ошибка кодирования изображения в Base64: -0x%04x\n", -ret);
    free(encodedImage);
    return;
  }

  // Добавляем завершающий нуль-терминатор
  encodedImage[outputLen] = '\0';

  // Отправляем закодированное изображение по MQTT
  bool success = mqttClient.publish(mqtt_topic.c_str(), (char*)encodedImage);

  if (success) {
    Serial.println("Изображение успешно отправлено по MQTT");
  } else {
    Serial.println("Ошибка отправки изображения по MQTT");
  }

  // Освобождаем выделенную память
  free(encodedImage);
}