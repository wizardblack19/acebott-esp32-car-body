#pragma once
inline int xTaskCreatePinnedToCore(void (*)(void *),const char *,int,void *,int,void *,int) { return 0; }
