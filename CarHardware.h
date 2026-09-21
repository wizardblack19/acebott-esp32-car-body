#pragma once
#include <Arduino.h>

// ACEBOTT QD002 wiring from the supplied vehicle.h. Arduino-ESP32 2.0.17.
// Reserve separate LEDC timers for motors, servos and buzzer.
enum : uint8_t { Stop = 0, Forward = 163, Backward = 92, Move_Left = 106,
  Move_Right = 149, Top_Left = 34, Bottom_Left = 72, Top_Right = 129,
  Bottom_Right = 20, Contrarotate = 83, Clockwise = 172 };

inline void initMotors() {
  for (int pin : {18, 16, 5, 17}) pinMode(pin, OUTPUT);
  digitalWrite(16, HIGH);
  ledcSetup(2, 1000, 8); ledcAttachPin(19, 2);
  ledcSetup(3, 1000, 8); ledcAttachPin(23, 3);
}
inline void moveCar(uint8_t direction, int speed) {
  speed = constrain(speed, 0, 255);
  ledcWrite(2, direction == Stop ? 0 : speed);
  ledcWrite(3, direction == Stop ? 0 : speed);
  digitalWrite(17, LOW);
  shiftOut(5, 18, MSBFIRST, direction);
  digitalWrite(17, HIGH);
  digitalWrite(16, LOW);
}
inline void writeServo(uint8_t channel, int degrees) {
  const uint32_t pulse = map(constrain(degrees, 0, 180), 0, 180, 544, 2400);
  ledcWrite(channel, pulse * 65535UL / 20000UL);
}
inline void initServos() {
  ledcSetup(4, 50, 16); ledcAttachPin(25, 4); // vertical camera servo
  ledcSetup(5, 50, 16); ledcAttachPin(26, 5);
  writeServo(4, 90); writeServo(5, 90);
}
