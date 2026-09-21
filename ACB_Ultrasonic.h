#ifndef ACB_Ultrasonic_H
#define ACB_Ultrasonic_H

#include <Arduino.h>

class ACB_Ultrasonic {
 public:
  ACB_Ultrasonic(uint8_t trig, uint8_t echo) { setpin(trig, echo); }

  void setpin(uint8_t trig, uint8_t echo) {
    Trig_PIN = trig;
    Echo_PIN = echo;
    pinMode(Trig_PIN, OUTPUT);
    pinMode(Echo_PIN, INPUT);
    digitalWrite(Trig_PIN, LOW);
  }

  // Centimetros, o -1 si no hay eco. El tiempo de espera esta limitado a 25 ms.
  float getData() {
    unsigned long echo = ultrasonic_time();
    if (echo == 0) return -1;
    return echo / 58.0f;
  }

  void ultrasonic_send() {
    digitalWrite(Trig_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(Trig_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(Trig_PIN, LOW);
  }

  float ultrasonic_time() {
    ultrasonic_send();
    return pulseIn(Echo_PIN, HIGH, 25000);
  }

 private:
  uint8_t Trig_PIN = 0;
  uint8_t Echo_PIN = 0;
};

#endif
