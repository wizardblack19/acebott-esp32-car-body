#pragma once
#include <stdint.h>
#include <stddef.h>
enum {PIXFORMAT_JPEG};
struct camera_fb_t { uint8_t *buf; size_t len; int format; };
inline camera_fb_t *esp_camera_fb_get() { return nullptr; }
inline void esp_camera_fb_return(camera_fb_t *) {}
