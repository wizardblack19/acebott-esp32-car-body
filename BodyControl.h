#pragma once
#include "CarHardware.h"
#include "CommandProtocol.h"
#include "Melodies.h"

CommandParser bodyParser;
uint8_t runMode = 0;
int speeds = 250;
uint32_t lastLink = 0, lastLight = 0, lastSensor = 0, lastTrack = 0;
bool linkActive = false;
int distanceCm = -1;
uint8_t lightDirection = 0;
uint8_t avoidStep = 0, avoidChoice = 0;
uint32_t avoidDeadline = 0, shotDeadline = 0;
bool shooting = false;
int song = -1, noteIndex = 0;
uint32_t noteDeadline = 0;
const int Black_Line = 2000, Off_Road = 4000;

bool due(uint32_t now, uint32_t deadline) { return int32_t(now - deadline) >= 0; }

void stopAll(bool center = true) {
  runMode = 0;
  avoidStep = 0;
  lightDirection = 0;
  moveCar(Stop, 0);
  song = -1;
  ledcWriteTone(6, 0);
  shooting = false;
  digitalWrite(32, LOW);
  if (center) { writeServo(4, 90); writeServo(5, 90); }
}

void handleBodyCommand(const CarCommand &cmd) {
  const uint32_t now = millis();
  if (cmd.action == CMD_HEARTBEAT) {
    if (linkActive) lastLink = now;
    return;
  }
  if (cmd.action == CMD_STANDBY) {
    stopAll(); linkActive = false;
    Serial.println("Parada general");
    return;
  }
  if (cmd.action == CMD_LIGHT_DIRECTION) {
    if (runMode == CMD_LIGHT_FOLLOW && cmd.hasValue && cmd.device == 12 && cmd.value <= 10) {
      lightDirection = cmd.value;
      lastLight = lastLink = now;
    }
    return;
  }
  if (cmd.action >= CMD_TRACK_1 && cmd.action <= CMD_LIGHT_FOLLOW) {
    stopAll();
    runMode = cmd.action;
    linkActive = true; lastLink = now;
    lastLight = now;
    lastSensor = now - 65;
    Serial.printf("Modo: %u\n", runMode);
    return;
  }
  if (cmd.action != CMD_RUN || !cmd.hasDevice) return;
  if (!cmd.hasValue && cmd.device != 8) return;
  switch (cmd.device) {
    case 12: {
      if (cmd.value > 10) return;
      stopAll(false);
      const uint8_t directions[] = {Stop, Forward, Backward, Move_Left, Move_Right,
        Top_Left, Bottom_Left, Top_Right, Bottom_Right, Contrarotate, Clockwise};
      moveCar(directions[cmd.value], speeds);
      break;
    }
    case 2:
    case 35:
      if (cmd.value > 180) return;
      stopAll(false);
      // Device 2 keeps the original combined-servo control; device 35 is an alias.
      writeServo(4, cmd.value); writeServo(5, cmd.value);
      break;
    case 3:
      if (cmd.value > 4) return;
      ledcWriteTone(6, 0);
      song = int(cmd.value) - 1; noteIndex = 0; noteDeadline = now;
      break;
    case 5:
      if (cmd.value > 1) return;
      digitalWrite(2, cmd.value); digitalWrite(12, cmd.value);
      break;
    case 8:
      shooting = true; shotDeadline = now + 200; digitalWrite(32, HIGH);
      break;
    case 13:
      speeds = cmd.value;
      break;
    default: return;
  }
  linkActive = true; lastLink = now;
}

void receiveCommands() {
  CarCommand cmd;
  for (int count = 0; count < 256 && Serial.available(); ++count) {
    if (bodyParser.feed(uint8_t(Serial.read()), millis(), cmd)) handleBodyCommand(cmd);
  }
}

void updateMusic(uint32_t now) {
  if (song < 0 || !due(now, noteDeadline)) return;
  const int *notes[] = {tune0, tune1, tune2, tune3};
  const float *durations[] = {durt0, durt1, durt2, durt3};
  const size_t lengths[] = {sizeof(tune0)/sizeof(int), sizeof(tune1)/sizeof(int),
    sizeof(tune2)/sizeof(int), sizeof(tune3)/sizeof(int)};
  if (size_t(noteIndex) >= lengths[song]) {
    song = -1; ledcWriteTone(6, 0); return;
  }
  ledcWriteTone(6, notes[song][noteIndex]);
  noteDeadline = now + uint32_t(durations[song][noteIndex] * (song == 3 ? 300 : 500));
  ++noteIndex;
}

void readDistance(uint32_t now) {
  if (uint32_t(now - lastSensor) < 65) return;
  lastSensor = now;
  digitalWrite(13, LOW); delayMicroseconds(2);
  digitalWrite(13, HIGH); delayMicroseconds(10); digitalWrite(13, LOW);
  // Explicit 25 ms bound: missing echo must not block serial reception for a second.
  const uint32_t echo = pulseIn(14, HIGH, 25000);
  distanceCm = echo ? int(echo / 58) : -1;
}

void updateAvoid(uint32_t now) {
  if (avoidStep) {
    if (!due(now, avoidDeadline)) return;
    switch (avoidStep) {
      case 1: writeServo(4, 45); avoidDeadline = now + 200; avoidStep = 2; break;
      case 2: writeServo(4, 135); avoidDeadline = now + 200; avoidStep = 3; break;
      case 3:
        writeServo(4, 90);
        avoidChoice = random(0, 4);
        moveCar(avoidChoice < 2 ? Backward : (avoidChoice == 2 ? Clockwise : Contrarotate), 180);
        avoidDeadline = now + (avoidChoice < 2 ? 500 : 1000); avoidStep = 4;
        break;
      case 4:
        if (avoidChoice < 2) {
          moveCar(avoidChoice == 0 ? Move_Left : Move_Right, 180);
          avoidDeadline = now + 500; avoidStep = 5;
        } else { moveCar(Stop, 0); avoidStep = 0; }
        break;
      default: moveCar(Stop, 0); avoidStep = 0; break;
    }
    return;
  }
  if (distanceCm < 0) { moveCar(Stop, 0); return; }
  if (distanceCm <= 25) {
    moveCar(Stop, 0); avoidStep = 1; avoidDeadline = now + 100;
  } else moveCar(Forward, 180);
}

void updateTracking(uint32_t now) {
  if (uint32_t(now - lastTrack) < 10) return;
  lastTrack = now;
  const int left = analogRead(35), middle = analogRead(36), right = analogRead(39);
  if (left >= Off_Road && right >= Off_Road) { moveCar(Stop, 0); return; }
  if (runMode == CMD_TRACK_1) {
    if (left < Black_Line && right < Black_Line) moveCar(Forward, 130);
    else if (left >= Black_Line && right < Black_Line) moveCar(Contrarotate, 150);
    else if (right >= Black_Line && left < Black_Line) moveCar(Clockwise, 150);
    else moveCar(Stop, 0);
  } else {
    if (middle >= Black_Line && (left < Black_Line || right < Black_Line)) moveCar(Forward, 180);
    else if (left >= Black_Line && middle < Black_Line && right < Black_Line) moveCar(Contrarotate, 220);
    else if (right >= Black_Line && middle < Black_Line && left < Black_Line) moveCar(Clockwise, 220);
    else moveCar(Stop, 0);
  }
}

void setup() {
  Serial.begin(115200);
  initMotors(); initServos();
  for (int pin : {2, 12, 32, 13}) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  for (int pin : {14, 35, 36, 39}) pinMode(pin, INPUT);
  ledcSetup(6, 2000, 10); ledcAttachPin(33, 6);
  stopAll();
  Serial.println("ACEBOTT carro listo: UART 115200; parada de enlace tras 1500 ms.");
}

void loop() {
  receiveCommands();
  uint32_t now = millis();
  if (linkActive && uint32_t(now - lastLink) > 1500) {
    stopAll(); linkActive = false; Serial.println("Parada: enlace perdido");
  }
  if (shooting && due(now, shotDeadline)) { digitalWrite(32, LOW); shooting = false; }
  updateMusic(now);
  if (runMode == CMD_AVOID || runMode == CMD_FOLLOW || runMode == CMD_LIGHT_FOLLOW) {
    readDistance(now);
    // Process a stop received during the bounded ultrasonic measurement before moving.
    receiveCommands(); now = millis();
  }
  switch (runMode) {
    case CMD_TRACK_1: case CMD_TRACK_2: updateTracking(now); break;
    case CMD_AVOID: updateAvoid(now); break;
    case CMD_FOLLOW:
      if (distanceCm < 0 || distanceCm > 50) moveCar(Stop, 0);
      else if (distanceCm < 15) moveCar(Backward, 200);
      else if (distanceCm <= 20) moveCar(Stop, 0);
      else moveCar(Forward, distanceCm <= 25 ? max(0, speeds - 70) : 220);
      break;
    case CMD_LIGHT_FOLLOW:
      if (uint32_t(now - lastLight) > 700 || distanceCm < 0 || distanceCm <= 25) moveCar(Stop, 0);
      else if (lightDirection == 1) moveCar(Forward, 160);
      else if (lightDirection == 9) moveCar(Contrarotate, 140);
      else if (lightDirection == 10) moveCar(Clockwise, 140);
      else moveCar(Stop, 0);
      break;
  }
  delay(1);
}
