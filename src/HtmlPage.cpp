#include "HtmlPage.h"
#include "SettingsManager.h"

void handleRoot(AsyncWebServerRequest *request) {
  String html = "<html><body><h1>Config Page</h1><form method='POST' action='/saveConfig'>";
  html += "<label>SSID:</label><input name='ssid' value='" + settings.ssid + "'><br>";
  html += "<label>Password:</label><input name='password' value='" + settings.password + "'><br>";
  html += "<label>FrameSize:</label><select name='frameSize'>";
  html += "<option value='9'" + String(settings.frameSize == 9 ? " selected" : "") + ">VGA</option>";
  html += "<option value='10'" + String(settings.frameSize == 10 ? " selected" : "") + ">SVGA</option>";
  html += "</select><br>";
  // Добавь остальные поля по аналогии
  html += "<input type='submit' value='Save'>";
  html += "</form></body></html>";
  request->send(200, "text/html", html);
}

void handleSaveConfig(AsyncWebServerRequest *request) {
  if (request->hasParam("ssid", true)) settings.ssid = request->getParam("ssid", true)->value();
  if (request->hasParam("password", true)) settings.password = request->getParam("password", true)->value();
  if (request->hasParam("frameSize", true)) settings.frameSize = request->getParam("frameSize", true)->value().toInt();
  // Остальные поля...

  if (saveSettings()) {
    request->send(200, "text/plain", "Saved. Rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    request->send(500, "text/plain", "Save failed.");
  }
}