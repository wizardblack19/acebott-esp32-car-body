#pragma once
#include <stddef.h>
typedef void *QueueHandle_t;
inline QueueHandle_t xQueueCreate(int, size_t) { return nullptr; }
inline int xQueueReceive(QueueHandle_t,void *,unsigned) { return 0; }
inline void xQueueOverwrite(QueueHandle_t,const void *) {}
