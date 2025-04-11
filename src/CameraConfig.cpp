#include "CameraConfig.h"
#include <Arduino.h>

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

bool initCamera(const CameraSettings& settings) {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = D0_GPIO_NUM;
  config.pin_d1 = D1_GPIO_NUM;
  config.pin_d2 = D2_GPIO_NUM;
  config.pin_d3 = D3_GPIO_NUM;
  config.pin_d4 = D4_GPIO_NUM;
  config.pin_d5 = D5_GPIO_NUM;
  config.pin_d6 = D6_GPIO_NUM;
  config.pin_d7 = D7_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = settings.frameSize;
  config.jpeg_quality = settings.jpegQuality;
  config.fb_count = settings.fbCount;
  config.grab_mode = CAMERA_GRAB_LATEST;

  Serial.println("Инициализация камеры...");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Ошибка инициализации камеры: %s\n", esp_err_to_name(err));
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, settings.vflip);
    s->set_hmirror(s, settings.hmirror);
    s->set_denoise(s, settings.denoise);
    s->set_brightness(s, settings.brightness);
    s->set_contrast(s, settings.contrast);
    s->set_saturation(s, settings.saturation);
    s->set_sharpness(s, settings.sharpness);
  }

  return true;
}

void deinitCamera() {
  esp_camera_deinit();
}

camera_fb_t* captureFrame() {
  return esp_camera_fb_get();
}

void returnFrame(camera_fb_t* fb) {
  esp_camera_fb_return(fb);
}