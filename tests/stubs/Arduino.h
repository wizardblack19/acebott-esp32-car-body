#pragma once
#include <stdint.h>
#include <stddef.h>
#include <initializer_list>
#include <string.h>
inline int abs(int x) { return x < 0 ? -x : x; }
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define MSBFIRST 1
extern uint32_t clockMs, echoUs, pwm[16];
extern int pins[40], analogPins[40], motorDirection;
inline uint32_t millis() { return clockMs; }
inline void delay(uint32_t value) { clockMs += value; }
inline void delayMicroseconds(uint32_t) {}
inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int value) { pins[pin] = value; }
inline int analogRead(int pin) { return analogPins[pin]; }
inline uint32_t pulseIn(int, int, uint32_t timeout) { clockMs += echoUs ? 1 : timeout / 1000; return echoUs; }
inline void ledcSetup(int, int, int) {}
inline void ledcAttachPin(int, int) {}
inline void ledcWrite(int channel, uint32_t value) { pwm[channel] = value; }
inline void ledcWriteTone(int channel, uint32_t value) { pwm[channel] = value; }
inline void shiftOut(int, int, int, int value) { motorDirection = value; }
template<class T> T constrain(T x, T low, T high) { return x < low ? low : x > high ? high : x; }
template<class T> T max(T a, T b) { return a > b ? a : b; }
template<class T> T min(T a, T b) { return a < b ? a : b; }
inline long map(long x, long a, long b, long c, long d) { return (x-a)*(d-c)/(b-a)+c; }
inline long random(long low, long) { return low; }
struct FakeSerial {
  uint8_t input[512] = {}; int used = 0, pos = 0;
  void begin(int) {}
  int available() { return used - pos; }
  int read() { return input[pos++]; }
  void println(const char *) {}
  void printf(const char *, ...) {}
};
extern FakeSerial Serial;
