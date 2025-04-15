// SeafileUploader.cpp
#include "SeafileUploader.h"

bool fetchSeafileToken() {
  if (settings.seafile_host == "" || settings.seafile_user == "" || settings.seafile_pass == "") {
    Serial.println("Не заданы параметры подключения к Seafile");
    return false;
  }

  String hostUrl = settings.seafile_host;
  bool useSSL = hostUrl.startsWith("https://");
  uint16_t port = useSSL ? 443 : 80;

  String host = hostUrl;
  host.replace("https://", "");
  host.replace("http://", "");
  int slashIndex = host.indexOf('/');
  if (slashIndex > 0) host = host.substring(0, slashIndex);

  String path = "/api2/auth-token/";
  String postData = "username=" + settings.seafile_user + "&password=" + settings.seafile_pass;

  WiFiClient *client;
  WiFiClientSecure secureClient;
  WiFiClient plainClient;

  if (useSSL) {
    secureClient.setInsecure();
    client = &secureClient;
  } else {
    client = &plainClient;
  }

  Serial.printf("[Seafile] Подключение к %s:%d\n", host.c_str(), port);
  if (!client->connect(host.c_str(), port)) {
    Serial.println("Ошибка подключения к Seafile");
    return false;
  }

  String request =
    "POST " + path + " HTTP/1.1\r\n" +
    "Host: " + host + "\r\n" +
    "Content-Type: application/x-www-form-urlencoded\r\n" +
    "Content-Length: " + String(postData.length()) + "\r\n" +
    "Connection: close\r\n\r\n" +
    postData;

  client->print(request);

  while (client->connected()) {
    String line = client->readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }

  String response = client->readString();
  client->stop();

  int start = response.indexOf(":\"") + 2;
  int end = response.indexOf("\"", start);
  if (start == 1 || end == -1) {
    Serial.println("Ошибка парсинга токена");
    return false;
  }

  settings.seafile_token = response.substring(start, end);
  saveSettings();
  Serial.println("Seafile токен получен и сохранён");
  return true;
}

void sendToSeafile(camera_fb_t *fb, const String& fileName) {
  if (!fb || fb->len == 0) {
    Serial.println("[Seafile] Пустой буфер изображения");
    return;
  }

  if (settings.seafile_token.isEmpty() && !settings.seafile_user.isEmpty() && !settings.seafile_host.isEmpty() && !settings.seafile_pass.isEmpty() && settings.chanelMode == 3) {
    Serial.println(settings.seafile_user);
    Serial.println("Seafile токен пустой, пробуем получить...");
    fetchSeafileToken();
  }

  String hostUrl = settings.seafile_host;
  bool useSSL = hostUrl.startsWith("https://");
  uint16_t port = useSSL ? 443 : 80;

  String host = hostUrl;
  host.replace("https://", "");
  host.replace("http://", "");
  host.trim();
  int slashIndex = host.indexOf('/');
  if (slashIndex > 0) host = host.substring(0, slashIndex);

  Serial.printf("[Seafile] Подключение к %s:%d (SSL: %s)\n", host.c_str(), port, useSSL ? "да" : "нет");

  String uploadLinkPath = "/api2/repos/" + settings.seafile_repo_id + "/upload-link/";
  String uploadLinkUrl = hostUrl + uploadLinkPath;

  HTTPClient http;
  if (useSSL) {
    WiFiClientSecure sclient;
    sclient.setInsecure();
    http.begin(sclient, uploadLinkUrl);
  } else {
    WiFiClient cclient;
    http.begin(cclient, uploadLinkUrl);
  }

  http.addHeader("Authorization", "Token " + String(settings.seafile_token));
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[Seafile] Ошибка получения upload link: %d\n", code);
    http.end();
    return;
  }

  String uploadUrl = http.getString();
  uploadUrl.replace("\"", "");
  http.end();

  String uploadPath;
  String uploadHost;
  bool uploadSSL = uploadUrl.startsWith("https://");
  uint16_t uploadPort = uploadSSL ? 443 : 80;

  uploadUrl.replace("https://", "");
  uploadUrl.replace("http://", "");
  int pathIndex = uploadUrl.indexOf('/');
  if (pathIndex > 0) {
    uploadHost = uploadUrl.substring(0, pathIndex);
    uploadPath = uploadUrl.substring(pathIndex);
  } else {
    Serial.println("[Seafile] Неверный upload URL");
    return;
  }

  Serial.printf("[Seafile] Отправка на %s:%d%s\n", uploadHost.c_str(), uploadPort, uploadPath.c_str());

  WiFiClient *client;
  WiFiClientSecure secureClient;
  WiFiClient plainClient;

  if (uploadSSL) {
    secureClient.setInsecure();
    client = &secureClient;
  } else {
    client = &plainClient;
  }

  if (!client->connect(uploadHost.c_str(), uploadPort)) {
    Serial.println("[Seafile] Ошибка подключения к серверу загрузки");
    return;
  }

  String boundary = "----ESP32Boundary";
  String bodyStart =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"parent_dir\"\r\n\r\n/\r\n" +
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"" + fileName + "\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";
  size_t contentLength = bodyStart.length() + fb->len + bodyEnd.length();

  String header =
    "POST " + uploadPath + " HTTP/1.1\r\n" +
    "Host: " + uploadHost + "\r\n" +
    "Authorization: Token " + String(settings.seafile_token) + "\r\n" +
    "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n" +
    "Content-Length: " + String(contentLength) + "\r\n" +
    "Connection: close\r\n\r\n";

  client->print(header);
  client->print(bodyStart);

  const size_t chunkSize = 1024;
  size_t bytesSent = 0;
  while (bytesSent < fb->len) {
    size_t len = min(chunkSize, fb->len - bytesSent);
    client->write(fb->buf + bytesSent, len);
    bytesSent += len;
    delay(1);
  }

  client->print(bodyEnd);

  Serial.println("[Seafile] Ожидаем ответ сервера...");
  while (client->connected()) {
    String line = client->readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") break;
  }

  String response = client->readString();
  Serial.println("[Seafile] Тело ответа:");
  Serial.println(response);

  client->stop();
}
