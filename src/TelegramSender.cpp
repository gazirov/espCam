// TelegramSender.cpp
#include "TelegramSender.h"

void sendToTelegram(camera_fb_t *fb, const String& fileName) {
  if (settings.tg_token == "" || settings.tg_chatId == "") {
    Serial.println("Telegram: токен или chat_id не указаны");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("Telegram: ошибка подключения");
    return;
  }

  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String dataStart =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
    settings.tg_chatId + "\r\n" +
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
    fileName + "\r\n" +
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String dataEnd = "\r\n--" + boundary + "--\r\n";
  size_t contentLength = dataStart.length() + fb->len + dataEnd.length();

  String requestHeader =
    "POST /bot" + settings.tg_token + "/sendPhoto HTTP/1.1\r\n" +
    "Host: api.telegram.org\r\n" +
    "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n" +
    "Content-Length: " + String(contentLength) + "\r\n\r\n";

  client.print(requestHeader);
  client.print(dataStart);

  const size_t bufferSize = 1024;
  size_t bytesSent = 0;
  while (bytesSent < fb->len) {
    size_t chunkSize = min(bufferSize, fb->len - bytesSent);
    client.write(fb->buf + bytesSent, chunkSize);
    bytesSent += chunkSize;
  }

  client.print(dataEnd);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = client.readString();
  //Serial.println("Ответ Telegram: " + response);

  client.stop();
}