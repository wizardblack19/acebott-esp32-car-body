#pragma once
#include <Arduino.h>
#include "esp_camera.h"
#include "esp_jpg_decode.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

struct LightRequest { uint32_t generation, requestedAt; };
struct LightResult { uint32_t generation, requestedAt; uint8_t direction; bool valid; };
QueueHandle_t lightRequests = nullptr, lightResults = nullptr;

struct LightSamples {
  camera_fb_t *frame;
  uint32_t sums[3] = {}, counts[3] = {}, bright[3] = {};
  uint16_t width = 0;
};
size_t readJpeg(void *arg, size_t index, uint8_t *buffer, size_t length) {
  auto &s = *static_cast<LightSamples *>(arg);
  if (index >= s.frame->len) return 0;
  length = min(length, s.frame->len - index);
  if (buffer) memcpy(buffer, s.frame->buf + index, length);
  return length;
}
bool sampleJpeg(void *arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *rgb) {
  auto &s = *static_cast<LightSamples *>(arg);
  if (!rgb) { if (w) s.width = w; return true; }
  if (!s.width) return false;
  for (uint16_t row = 0; row < h; ++row) {
    for (uint16_t col = 0; col < w; ++col) {
      const uint8_t *pixel = rgb + (size_t(row) * w + col) * 3;
      // Approximate luminance. R/B weighting symmetric to tolerate decoder byte order.
      const uint16_t luma = (pixel[0] + 2 * pixel[1] + pixel[2]) / 4;
      const uint8_t region = min(2, int((x + col) * 3 / s.width));
      s.sums[region] += luma; ++s.counts[region];
      if (luma >= 210) ++s.bright[region];
    }
  }
  return true;
}

uint8_t chooseLightDirection(const LightSamples &s) {
  uint32_t mean[3], score[3];
  for (int i = 0; i < 3; ++i) {
    if (!s.counts[i]) return 0;
    mean[i] = s.sums[i] / s.counts[i];
    score[i] = 255 * s.bright[i] / s.counts[i];
  }
  const uint32_t darkest = min(mean[0], min(mean[1], mean[2]));
  int best = 1;
  for (int i : {0, 2}) if (score[i] > score[best]) best = i;
  // Uniform daylight/darkness is not a target. Require a bright contrasting region.
  if (score[best] < 5 || mean[best] < darkest + 15) return 0;
  if (best == 1 || abs(int(score[0]) - int(score[2])) <= 5) return 1;
  return best == 0 ? 9 : 10;
}

void lightTask(void *) {
  LightRequest request;
  for (;;) {
    if (xQueueReceive(lightRequests, &request, portMAX_DELAY) != pdTRUE) continue;
    LightResult result = {request.generation, request.requestedAt, 0, false};
    camera_fb_t *frame = esp_camera_fb_get();
    if (frame) {
      LightSamples samples = {};
      samples.frame = frame;
      if (frame->format == PIXFORMAT_JPEG &&
          esp_jpg_decode(frame->len, JPG_SCALE_4X, readJpeg, sampleJpeg, &samples) == ESP_OK) {
        result.valid = true;
        result.direction = chooseLightDirection(samples);
      }
      esp_camera_fb_return(frame);
    }
    xQueueOverwrite(lightResults, &result);
  }
}
bool startLightWorker() {
  lightRequests = xQueueCreate(1, sizeof(LightRequest));
  lightResults = xQueueCreate(1, sizeof(LightResult));
  return lightRequests && lightResults &&
    xTaskCreatePinnedToCore(lightTask, "light", 6144, nullptr, 1, nullptr, 1) == pdPASS;
}
