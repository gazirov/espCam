#ifndef CAMERA_CONFIG_H
#define CAMERA_CONFIG_H

#include "esp_camera.h"

// Структура с параметрами камеры
struct CameraSettings {
  framesize_t frameSize = FRAMESIZE_VGA;
  int jpegQuality = 14;      // 0 (макс. качество) ... 63 (мин. качество)
  int fbCount = 3;           // Кол-во фреймбуферов
  bool vflip = true;         // Вертикальный флип
  bool hmirror = false;      // Горизонтальный флип
  bool denoise = true;       // Уменьшение шума
  int brightness = 0;        // -2..2
  int contrast = 1;          // -2..2
  int saturation = 0;        // -2..2
  int sharpness = 2;         // 0..2
};

// Инициализация камеры с параметрами
bool initCamera(const CameraSettings& settings);

// Деинициализация
void deinitCamera();

// Получение кадра
camera_fb_t* captureFrame();

// Освобождение кадра
void returnFrame(camera_fb_t* fb);

#endif // CAMERA_CONFIG_H