// MQTTClient.cpp
#include "MQTTClient.h"
#include "mbedtls/base64.h"

void initMQTT(PubSubClient& client) {
  if (settings.mqtt_server == "") {
    Serial.println("MQTT сервер не настроен");
    return;
  }
  client.setServer(settings.mqtt_server.c_str(), 1883);
  connectToMQTT(client);
}

void connectToMQTT(PubSubClient& client) {
  const int maxAttempts = 5;
  int attempt = 0;

  while (!client.connected() && attempt < maxAttempts) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi не подключён, MQTT пропущен");
      return;
    }

    String clientId = "ESP32Cam_" + String(random(0xffff), HEX);
    Serial.printf("MQTT подключение (попытка %d из %d)...\n", attempt + 1, maxAttempts);

    if (client.connect(clientId.c_str())) {
      Serial.println("MQTT подключено");
      break;
    } else {
      Serial.printf("Ошибка подключения: %d\n", client.state());
      attempt++;
      delay(5000);
    }
  }
}

void publishToMQTT(PubSubClient& client, camera_fb_t *fb) {
  if (settings.mqtt_server == "" || settings.mqtt_topic == "") {
    Serial.println("MQTT сервер или топик не настроены");
    return;
  }

  if (!client.connected()) {
    connectToMQTT(client);
  }

  client.loop();

  size_t encodedLen = 4 * ((fb->len + 2) / 3);
  unsigned char* encodedImage = (unsigned char*)malloc(encodedLen + 1);
  if (!encodedImage) {
    Serial.println("Ошибка выделения памяти");
    return;
  }

  size_t outputLen = 0;
  int ret = mbedtls_base64_encode(encodedImage, encodedLen, &outputLen, fb->buf, fb->len);
  if (ret != 0) {
    Serial.printf("Ошибка Base64: -0x%04x\n", -ret);
    free(encodedImage);
    return;
  }

  encodedImage[outputLen] = '\0';
  bool success = client.publish(settings.mqtt_topic.c_str(), (char*)encodedImage);
  free(encodedImage);

  Serial.println(success ? "MQTT отправлено" : "Ошибка отправки MQTT");
}