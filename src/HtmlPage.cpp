#include "HtmlPage.h"
#include "SettingsManager.h"

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
  html += "<label for='ssid'>SSID:</label><input type='text' id='ssid' name='ssid' value='" + settings.ssid + "'>";
  html += "<label for='password'>Password:</label><input type='password' id='password' name='password' value='" + settings.password + "'>";
  html += "</fieldset>";
  
  html += "<fieldset>";
  html += "<legend>Work Mode Settings</legend>";
  html += "<label for='mode'>Work mode:</label>";
  html += "<select name='mode' id='mode'>";
  html += "<option value='1'" + String(settings.savedOperationMode == 1 ? " selected" : "") + ">Stream mode</option>";
  html += "<option value='2'" + String(settings.savedOperationMode == 2 ? " selected" : "") + ">Interval</option>";
  html += "</select>";
  html += "<label for='interval_min'>Interval (min):</label><input type='number' id='interval_min' name='interval_min' value='" + String(settings.interval_min) + "'>";
  html += "<label for='chanel'>Channel:</label>";
  html += "<select name='chanel' id='chanel'>";
  html += "<option value='1'" + String(settings.chanelMode == 1 ? " selected" : "") + ">Telegram</option>";
  html += "<option value='2'" + String(settings.chanelMode == 2 ? " selected" : "") + ">MQTT</option>";
  html += "<option value='3'" + String(settings.chanelMode == 3 ? " selected" : "") + ">seaFile</option>";
  html += "</select>";
  html += "</fieldset>";

  html += "<details>";
  html += "<summary><strong>Camera Settings</strong></summary>";
  html += "<fieldset>";
  //html += "<legend>Camera Settings</legend>";
  // Frame size
  html += "<label for='frameSize'>Frame Size:</label>";
  html += "<select name='frameSize' id='frameSize'>";
  html += "<option value='10'" + String(settings.frameSize == 10 ? " selected" : "") + ">SVGA (800x600)</option>";
  html += "<option value='9'" + String(settings.frameSize == 9 ? " selected" : "") + ">VGA (640x480)</option>";
  html += "<option value='8'" + String(settings.frameSize == 8 ? " selected" : "") + ">CIF (352x288)</option>";
  html += "<option value='7'" + String(settings.frameSize == 7 ? " selected" : "") + ">QVGA (320x240)</option>";
  html += "<option value='6'" + String(settings.frameSize == 6 ? " selected" : "") + ">HQVGA (240x160)</option>";
  html += "<option value='5'" + String(settings.frameSize == 5 ? " selected" : "") + ">QQVGA (160x120)</option>";
  html += "</select><small>Размер кадра (чем больше — тем ниже FPS)</small>";

  // JPEG quality
  html += "<label for='jpegQuality'>JPEG Quality (0-63):</label>";
  html += "<input type='number' id='jpegQuality' name='jpegQuality' min='0' max='63' value='" + String(settings.jpegQuality) + "'>";
  html += "<small>0 — лучшее качество, 63 — худшее</small>";

  // Framebuffer count
  html += "<label for='fbCount'>Frame Buffers (1-3):</label>";
  html += "<input type='number' id='fbCount' name='fbCount' min='1' max='3' value='" + String(settings.fbCount) + "'>";
  html += "<small>1 — меньше памяти, 2-3 — стабильнее</small>";

  // vflip
  html += "<label for='vflip'>Vertical Flip:</label>";
  html += "<select name='vflip' id='vflip'>";
  html += "<option value='1'" + String(settings.vflip ? " selected" : "") + ">On</option>";
  html += "<option value='0'" + String(!settings.vflip ? " selected" : "") + ">Off</option>";
  html += "</select>";

  // hmirror
  html += "<label for='hmirror'>Horizontal Mirror:</label>";
  html += "<select name='hmirror' id='hmirror'>";
  html += "<option value='1'" + String(settings.hmirror ? " selected" : "") + ">On</option>";
  html += "<option value='0'" + String(!settings.hmirror ? " selected" : "") + ">Off</option>";
  html += "</select>";

  // denoise
  html += "<label for='denoise'>Denoise:</label>";
  html += "<select name='denoise' id='denoise'>";
  html += "<option value='1'" + String(settings.denoise ? " selected" : "") + ">On</option>";
  html += "<option value='0'" + String(!settings.denoise ? " selected" : "") + ">Off</option>";
  html += "</select>";

  // brightness
  html += "<label for='brightness'>Brightness (-2 to 2):</label>";
  html += "<input type='number' id='brightness' name='brightness' min='-2' max='2' value='" + String(settings.brightness) + "'>";

  // contrast
  html += "<label for='contrast'>Contrast (-2 to 2):</label>";
  html += "<input type='number' id='contrast' name='contrast' min='-2' max='2' value='" + String(settings.contrast) + "'>";

  // saturation
  html += "<label for='saturation'>Saturation (-2 to 2):</label>";
  html += "<input type='number' id='saturation' name='saturation' min='-2' max='2' value='" + String(settings.saturation) + "'>";

  // sharpness
  html += "<label for='sharpness'>Sharpness (0 to 2):</label>";
  html += "<input type='number' id='sharpness' name='sharpness' min='0' max='2' value='" + String(settings.sharpness) + "'>";
  html += "</fieldset>";
  html += "</details>";

  html += "<details>";
  html += "<summary><strong>Telegram Settings</strong></summary>";    
  html += "<fieldset>";
 // html += "<legend>Telegram Settings</legend>";
  html += "<label for='tg_token'>Telegram token:</label><input type='text' id='tg_token' name='tg_token' value='" + String(settings.tg_token) + "'>";
  html += "<label for='tg_chatId'>Telegram Chat ID:</label><input type='text' id='tg_chatId' name='tg_chatId' value='" + String(settings.tg_chatId) + "'>";
  html += "</fieldset>";
  html += "</details>";
  
  html += "<details>";
  html += "<summary><strong>MQTT Settings</strong></summary>";
  html += "<fieldset>";
  //html += "<legend>MQTT Settings</legend>";
  html += "<label for='mqtt_server'>MQTT server:</label><input type='text' id='mqtt_server' name='mqtt_server' value='" + String(settings.mqtt_server) + "'>";
  html += "<label for='mqtt_topic'>MQTT topic:</label><input type='text' id='mqtt_topic' name='mqtt_topic' value='" + String(settings.mqtt_topic) + "'>";
  html += "</fieldset>";
  html += "</details>";

  html += "<details>";
  html += "<summary><strong>Seafile settings</strong></summary>";
  html += "<fieldset style='margin-top: 10px;'>";
  html += "<label>Host:<br>";
  html += "<input type='text' name='seafile_host'>";
  html += "</label><br><br>";

  html += "<label>login:<br>";
  html += "<input type='text' name='seafile_user'>";
  html += "</label><br><br>";

  html += "<label>password:<br>";
  html += "<input type='password' name='seafile_pass'>";
  html += "</label><br><br>";

  html += "<label>token:<br>";
  html += "<input type='text' name='seafile_token'>";
  html += "</label><br><br>";

  html += "<label>Repo ID:<br>";
  html += "<input type='text' name='seafile_repo_id'>";
  html += "</label>";
  html += "</fieldset>";
  html += "</details>";


    /* 
  html += "<fieldset>";
  html += "<legend>SMB Settings</legend>";
  html += "<label for='smb_server'>SMB server:</label><input type='text' id='smb_server' name='smb_server' value='" + String(settings.smb_server) + "'>";
  html += "<label for='smb_username'>SMB username:</label><input type='text' id='smb_username' name='smb_username' value='" + String(settings.smb_username) + "'>";
  html += "<label for='smb_password'>SMB password:</label><input type='password' id='smb_password' name='smb_password' value='" + String(settings.smb_password) + "'>";
  html += "</fieldset>";
   
  html += "<fieldset>";
  html += "<legend>Interval Settings</legend>";
  html += "<label for='interval_min'>Interval (min):</label><input type='number' id='interval_min' name='interval_min' value='" + String(settings.interval_min) + "'>";
  html += "</fieldset>";
    */
  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "<a href='/video'>View Camera Stream</a>";
  html += "</body></html>";
  request->send(200, "text/html", html);
  
}

void handleSaveConfig(AsyncWebServerRequest *request) {
  if (request->hasParam("ssid", true)) settings.ssid = request->getParam("ssid", true)->value();
  if (request->hasParam("password", true)) settings.password = request->getParam("password", true)->value();
  if (request->hasParam("mode", true)) settings.savedOperationMode = request->getParam("mode", true)->value().toInt();
  if (request->hasParam("chanel", true)) settings.chanelMode = request->getParam("chanel", true)->value().toInt();
  if (request->hasParam("tg_token", true)) settings.tg_token = request->getParam("tg_token", true)->value();
  if (request->hasParam("tg_chatId", true)) settings.tg_chatId = request->getParam("tg_chatId", true)->value();
  if (request->hasParam("interval_min", true)) settings.interval_min = request->getParam("interval_min", true)->value().toInt();
  if (request->hasParam("mqtt_server", true)) settings.mqtt_server = request->getParam("mqtt_server", true)->value();
  if (request->hasParam("mqtt_topic", true)) settings.mqtt_topic = request->getParam("mqtt_topic", true)->value();
  if (request->hasParam("frameSize", true)) settings.frameSize = request->getParam("frameSize", true)->value().toInt();
  if (request->hasParam("jpegQuality", true)) settings.jpegQuality = request->getParam("jpegQuality", true)->value().toInt();
  if (request->hasParam("fbCount", true)) settings.fbCount = request->getParam("fbCount", true)->value().toInt();
  if (request->hasParam("vflip", true)) settings.vflip = request->getParam("vflip", true)->value().toInt();
  if (request->hasParam("hmirror", true)) settings.hmirror = request->getParam("hmirror", true)->value().toInt();
  if (request->hasParam("denoise", true)) settings.denoise = request->getParam("denoise", true)->value().toInt();
  if (request->hasParam("brightness", true)) settings.brightness = request->getParam("brightness", true)->value().toInt();
  if (request->hasParam("contrast", true)) settings.contrast = request->getParam("contrast", true)->value().toInt();
  if (request->hasParam("saturation", true)) settings.saturation = request->getParam("saturation", true)->value().toInt();
  if (request->hasParam("sharpness", true)) settings.sharpness = request->getParam("sharpness", true)->value().toInt();
  if (request->hasParam("seafile_host", true)) settings.seafile_host = request->getParam("seafile_host", true)->value();
  if (request->hasParam("seafile_user", true)) settings.seafile_user = request->getParam("seafile_user", true)->value();
  if (request->hasParam("seafile_pass", true)) settings.seafile_pass = request->getParam("seafile_pass", true)->value();
  if (request->hasParam("seafile_token", true)) settings.seafile_token = request->getParam("seafile_token", true)->value();
  if (request->hasParam("seafile_repo_id", true)) settings.seafile_repo_id = request->getParam("seafile_repo_id", true)->value();
  
  if (saveSettings()) {
    request->send(200, "text/plain", "Saved. Rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    request->send(500, "text/plain", "Save failed.");
  }
}

void handleVideoPage(AsyncWebServerRequest *request) {
  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Camera Stream</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; }";
  html += "h1 { color: #333; }";
  html += "img { width: 640px; height: 480px; border: 2px solid #333; }";
  html += "</style>";
  html += "</head><body>";
  html += "<h1>Camera Stream</h1>";
  html += "<img src='http://" + WiFi.localIP().toString() + ":81/stream' alt='Camera Stream'>";
  html += "</body></html>";
  request->send(200, "text/html", html);
}
