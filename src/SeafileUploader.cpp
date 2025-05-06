#include "SeafileUploader.h"

struct UrlInfo {
  String host;
  String path;
  bool useSSL;
  uint16_t port;
};

UrlInfo parseUrl(const String& url) {
  UrlInfo info;
  info.useSSL = url.startsWith("https://");
  info.port = info.useSSL ? 443 : 80;

  String tempUrl = url;
  tempUrl.replace("https://", "");
  tempUrl.replace("http://", "");
  tempUrl.trim();
  int pathIndex = tempUrl.indexOf('/');
  if (pathIndex > 0) {
    info.host = tempUrl.substring(0, pathIndex);
    info.path = tempUrl.substring(pathIndex);
  } else {
    info.host = tempUrl;
    info.path = "/";
  }

  return info;
}

WiFiClient* createClient(bool useSSL, HTTPClient& http, const String& url) {
  WiFiClientSecure* secureClient = new WiFiClientSecure();
  WiFiClient* plainClient = new WiFiClient();

  if (useSSL) {
    secureClient->setInsecure(); // Отключаем проверку сертификатов
    http.begin(*secureClient, url);
    return secureClient;
  } else {
    http.begin(*plainClient, url);
    return plainClient;
  }
}

bool fetchSeafileToken() {
  if (settings.seafile_host == "" || settings.seafile_user == "" || settings.seafile_pass == "") {
    Serial.println("Не заданы параметры подключения к Seafile");
    return false;
  }

  UrlInfo hostInfo = parseUrl(settings.seafile_host);

  String path = "/api2/auth-token/";
  String postData = "username=" + settings.seafile_user + "&password=" + settings.seafile_pass;

  HTTPClient http;
  WiFiClient* client = createClient(hostInfo.useSSL, http, settings.seafile_host + path);

  if (!client->connect(hostInfo.host.c_str(), hostInfo.port)) {
    Serial.println("Ошибка подключения к Seafile");
    delete client;
    return false;
  }

  String request =
    "POST " + path + " HTTP/1.1\r\n" +
    "Host: " + hostInfo.host + "\r\n" +
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
  delete client;

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

  UrlInfo hostInfo = parseUrl(settings.seafile_host);

  Serial.printf("[Seafile] Подключение к %s:%d (SSL: %s)\n", hostInfo.host.c_str(), hostInfo.port, hostInfo.useSSL ? "да" : "нет");

  String uploadLinkPath = "/api2/repos/" + settings.seafile_repo_id + "/upload-link/";
  String uploadLinkUrl = settings.seafile_host + uploadLinkPath;

  HTTPClient http;
  WiFiClient* client = createClient(hostInfo.useSSL, http, uploadLinkUrl);

  http.addHeader("Authorization", "Token " + String(settings.seafile_token));
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[Seafile] Ошибка получения upload link: %d\n", code);
    http.end();
    delete client;
    return;
  }

  String uploadUrl = http.getString();
  uploadUrl.replace("\"", "");
  http.end();

  UrlInfo uploadInfo = parseUrl(uploadUrl);

  Serial.printf("[Seafile] Отправка на %s:%d%s\n", uploadInfo.host.c_str(), uploadInfo.port, uploadInfo.path.c_str());

  client = createClient(uploadInfo.useSSL, http, uploadUrl);

  if (!client->connect(uploadInfo.host.c_str(), uploadInfo.port)) {
    Serial.println("[Seafile] Ошибка подключения к серверу загрузки");
    delete client;
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
    "POST " + uploadInfo.path + " HTTP/1.1\r\n" +
    "Host: " + uploadInfo.host + "\r\n" +
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
  delete client;
}
