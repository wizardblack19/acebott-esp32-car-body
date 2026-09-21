#pragma once
#include <stdint.h>
#include <stddef.h>
#define ESP_OK 0
#define JPG_SCALE_4X 2
inline int esp_jpg_decode(size_t, int, size_t (*)(void *,size_t,uint8_t *,size_t),
                         bool (*)(void *,uint16_t,uint16_t,uint16_t,uint16_t,uint8_t *),void *) { return -1; }
