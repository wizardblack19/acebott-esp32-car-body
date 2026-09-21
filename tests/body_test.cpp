#include "Arduino.h"
#include "BodyControl.h"
#include "camara/acebott-esp32-car-camera/LightTracker.h"
uint32_t clockMs = 0, echoUs = 5800, pwm[16] = {};
int pins[40] = {}, analogPins[40] = {}, motorDirection = -1;
FakeSerial Serial;
extern "C" void *memset(void *destination, int value, size_t length) {
  auto bytes = static_cast<unsigned char *>(destination);
  for (size_t i = 0; i < length; ++i) bytes[i] = value;
  return destination;
}
#define CHECK(condition) if (!(condition)) return __LINE__
void command(uint8_t action, uint8_t device = 0, uint8_t value = 0) {
  uint8_t bytes[13]; encodeCommand(bytes, action, device, value);
  Serial.used = Serial.pos = 0;
  for (auto byte : bytes) Serial.input[Serial.used++] = byte;
  receiveCommands();
}
extern "C" int runTests() {
  CommandParser parser; CarCommand cmd = {};
  uint8_t frame[13]; encodeCommand(frame, CMD_RUN, 12, 1);
  int count = 0;
  for (uint8_t b : frame) if (parser.feed(b, 0, cmd)) ++count;
  CHECK(count == 1 && cmd.action == 1 && cmd.device == 12 && cmd.value == 1);
  // Legacy short modes must not reuse the previous device/value.
  frame[2] = 7; frame[9] = 6;
  for (int i = 0; i < 10; ++i) parser.feed(frame[i], 0, cmd);
  CHECK(cmd.action == 6 && !cmd.hasDevice && !cmd.hasValue && cmd.value == 0);
  // Legacy 17-byte action 3 with device 8/value 240 still means STOP.
  uint8_t legacy[17] = {255,85,14,0,0,0,0,0,0,3,8,0,240,0,0,0,0};
  for (auto b : legacy) parser.feed(b, 0, cmd);
  CHECK(cmd.action == 3 && cmd.device == 8 && cmd.value == 240);
  // Malformed lengths and truncated packets recover without writing past the buffer.
  for (int length = 0; length < 256; ++length) {
    parser.reset(); parser.feed(255,0,cmd); parser.feed(85,0,cmd); parser.feed(length,0,cmd);
    for (int i = 0; i < 300; ++i) parser.feed(0,0,cmd);
    encodeCommand(frame,1,12,0);
    bool complete = false;
    for (auto b : frame) complete |= parser.feed(b,300,cmd);
    CHECK(complete && cmd.device == 12);
  }
  parser.reset(); parser.feed(255,0,cmd); parser.feed(85,0,cmd); parser.feed(10,0,cmd);
  count = 0;
  for (auto b : frame) if (parser.feed(b,300,cmd)) ++count;
  CHECK(count == 1);

  setup(); CHECK(motorDirection == Stop);
  command(CMD_AVOID); distanceCm = 15; updateAvoid(clockMs);
  clockMs += 100; updateAvoid(clockMs);
  const uint32_t vertical45 = pwm[4];
  CHECK(avoidStep == 2 && vertical45 != pwm[5]);
  command(CMD_STANDBY,8,240); clockMs += 500; loop();
  CHECK(runMode == 0 && avoidStep == 0 && motorDirection == Stop && pwm[4] == pwm[5]);
  command(CMD_AVOID); command(CMD_RUN,12,1); loop();
  CHECK(runMode == 0 && motorDirection == Forward);
  command(CMD_RUN,12,0); CHECK(motorDirection == Stop);
  command(CMD_RUN,2,90); uint32_t middle = pwm[4];
  command(CMD_RUN,2,100); CHECK(pwm[4] > middle);
  uint32_t at100 = pwm[4]; command(CMD_RUN,2,101); CHECK(pwm[4] > at100);
  command(CMD_RUN,3,1); updateMusic(clockMs); CHECK(pwm[6] != 0);
  command(CMD_RUN,8,0); CHECK(pins[32] == HIGH);
  command(CMD_STANDBY); CHECK(pwm[6] == 0 && pins[32] == LOW);
  command(CMD_RUN,8,0); clockMs += 201; loop(); CHECK(pins[32] == LOW);
  command(CMD_RUN,12,1); clockMs += 1501; loop(); CHECK(motorDirection == Stop && !linkActive);
  command(CMD_RUN,12,1); clockMs += 1000; command(CMD_HEARTBEAT); clockMs += 1000; loop();
  CHECK(motorDirection == Forward);
  command(CMD_FOLLOW); echoUs = 0; loop(); CHECK(motorDirection == Stop);
  command(CMD_LIGHT_FOLLOW); echoUs = 5800; clockMs += 65;
  command(CMD_LIGHT_DIRECTION,12,1); loop(); CHECK(motorDirection == Forward);
  clockMs += 701; loop(); CHECK(motorDirection == Stop);
  command(CMD_LIGHT_DIRECTION,12,9); loop(); CHECK(motorDirection == Contrarotate);
  command(CMD_STANDBY); command(CMD_LIGHT_DIRECTION,12,1); loop(); CHECK(motorDirection == Stop);
  command(CMD_TRACK_2); analogPins[35] = 0; analogPins[36] = 3000; analogPins[39] = 0;
  clockMs += 10; loop(); CHECK(motorDirection == Forward);
  analogPins[36] = 0; clockMs += 10; loop(); CHECK(motorDirection == Stop);
  LightSamples sample = {};
  // Sample decoded RGB blocks, not the compressed JPEG byte stream.
  uint8_t rgb[27] = {};
  for (int region = 0; region < 3; ++region) {
    sample = {}; sampleJpeg(&sample,0,0,9,1,nullptr);
    for (int i = 0; i < 27; ++i) rgb[i] = (i / 9 == region) ? 250 : 30;
    CHECK(sampleJpeg(&sample,0,0,9,1,rgb));
    CHECK(chooseLightDirection(sample) == (region == 0 ? 9 : region == 1 ? 1 : 10));
  }
  sample = {}; sampleJpeg(&sample,0,0,9,1,nullptr);
  for (int i = 0; i < 27; ++i) rgb[i] = 250;
  sampleJpeg(&sample,0,0,9,1,rgb); CHECK(chooseLightDirection(sample) == 0);
  sample = {}; CHECK(chooseLightDirection(sample) == 0);
  return 0;
}
