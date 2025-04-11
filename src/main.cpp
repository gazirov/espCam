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
#include "CameraConfig.h"
#include "SettingsManager.h"
#include "HtmlPage.h"


// Параметры для измерения напряжения батареи
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

//SMB

// Объекты для HTTPS соединения
WiFiClientSecure client;
int operationMode = 0;

// Файл для сохранения настроек
//const char* settingsPath = "/settings.json";

// Прототипы функций
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
void sendToTelegram(camera_fb_t *fb, const String& fileName);
void handle_jpg_stream(AsyncWebServerRequest *request); // Добавлено для потоковой передачи
void connectToMQTT();
void initMQTT();
void publishToMQTT(camera_fb_t *fb);

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!psramFound()) {
    Serial.println("PSRAM не найдена!");
  } else {
    Serial.printf("PSRAM найдена: %u байт\n", ESP.getFreePsram());
  }

  // Инициализация файловой системы
  if (!SPIFFS.begin(true)) {
    Serial.println("Ошибка монтирования SPIFFS");
    return;
  }
  
  loadSettings();
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


  initWiFi();


  CameraSettings camSettings;
  camSettings.frameSize = (framesize_t)settings.frameSize;
  camSettings.jpegQuality = settings.jpegQuality;
  camSettings.fbCount = settings.fbCount;
  camSettings.vflip = settings.vflip;
  camSettings.hmirror = settings.hmirror;
  camSettings.denoise = settings.denoise;
  camSettings.brightness = settings.brightness;
  camSettings.contrast = settings.contrast;
  camSettings.saturation = settings.saturation;
  camSettings.sharpness = settings.sharpness;
 
  if (!initCamera(camSettings)) {
    Serial.println("Ошибка инициализации камеры");
    while (true) delay(1000);
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

  
  
  //initCamera(camSettings);
  setupLED();
  initWebServer();
  initTime();

  operationMode = settings.savedOperationMode;
  // if not connect, going to stream mode
  if (WiFi.status() == WL_CONNECTED){
    // Условная инициализация MQTT
    if (settings.chanelMode == 2 && operationMode == 2) {
      Serial.println("Инициализация MQTT...");
      initMQTT();
    }
    else if (operationMode == 2) {
      // Режим интервальной отправки
      ledIndicator(2);
      captureAndSend();
      enterDeepSleep(settings.interval_min * 60); // Переходим в сон на заданный интервал
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
  if (currentMode == 2 && settings.chanelMode == 2) {
    // Если потоковая трансляция и используем MQTT
    if (!mqttClient.connected()) {
      connectToMQTT();
    }
    mqttClient.loop();
  }

ledIndicator(currentMode);
 delay(10);
}

// Функция инициализации Wi-Fi
void initWiFi() {
  WiFi.disconnect(true);
  delay(100);

  if (settings.ssid != "") {
    // Пытаемся подключиться к сохраненной сети
    WiFi.begin(settings.ssid.c_str(), settings.password.c_str());

    unsigned long startAttemptTime = millis();
    Serial.println("WIFI подключемся\n" );
    Serial.println(settings.ssid.c_str());
    Serial.println(settings.password.c_str());
    // Ждем подключения или истечения таймаута
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
      delay(100);
      if (WiFi.status() == WL_CONNECT_FAILED) {
        Serial.println("Неверный пароль или ошибка подключения");
        break;
    }
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

  server.on("/video", HTTP_GET, handleVideoPage);

  // Начало работы сервера конфигурации
  server.begin();
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
  if (settings.mqtt_server == "") {
    Serial.println("MQTT сервер не настроен");
    return;
  }
  
  mqttClient.setServer(settings.mqtt_server.c_str(), 1883); // Используем стандартный порт 1883 
  connectToMQTT();
}
void connectToMQTT() {
  const int maxAttempts = 5;
  int attempt = 0;

  while (!mqttClient.connected() && attempt < maxAttempts) {
    Serial.print("Подключаемся к MQTT-брокеру... (Попытка ");
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi не подключён, пропуск MQTT");
     // break;
    }
    Serial.print(attempt + 1);
    Serial.println(" из 5)");

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

 
      if (fb == NULL) {
          int64_t now = esp_timer_get_time();
          if (now - last_frame < 1000000 / 15) { // Ограничение до 15 FPS
              return 0; // Пропускаем кадр
          }
        

        // Получаем новый кадр
        fb = esp_camera_fb_get();
        last_frame = now;
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
  if (settings.chanelMode == 1) {
    Serial.println("Отправка изображения в Telegram...");
    // Отправка в Telegram-бот
    sendToTelegram(fb, fileName);
  } else if (settings.chanelMode == 2) {
    Serial.println("Отправка изображения в MQTT...");
    // Отправка в MQTT
     publishToMQTT(fb);
  } else if (settings.chanelMode == 3) {
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
  if (settings.tg_token == "" || settings.tg_chatId == "") {
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
  dataStart += settings.tg_chatId + "\r\n";

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
  requestHeader += "POST /bot" + settings.tg_token + "/sendPhoto HTTP/1.1\r\n";
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

  WiFi.disconnect(true); // Отключаем Wi-Fi полностью
  WiFi.mode(WIFI_OFF);     // Останавливаем Wi-Fi модуль
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
  if (settings.mqtt_server == "" || settings.mqtt_topic == "") {
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
  bool success = mqttClient.publish(settings.mqtt_topic.c_str(), (char*)encodedImage);

  if (success) {
    Serial.println("Изображение успешно отправлено по MQTT");
  } else {
    Serial.println("Ошибка отправки изображения по MQTT");
  }

  // Освобождаем выделенную память
  free(encodedImage);
}