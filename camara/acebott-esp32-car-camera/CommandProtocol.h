#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// FF 55, payload length, six reserved bytes, action, device, reserved, value.
// Legacy mode packets end at action (10 bytes); app packets may be 17 bytes.
struct CarCommand {
  uint8_t action;
  uint8_t device;
  uint8_t value;
  bool hasDevice;
  bool hasValue;
};

class CommandParser {
 public:
  bool feed(uint8_t value, uint32_t now, CarCommand &command) {
    if (used && uint32_t(now - lastByte) > 250) reset();
    lastByte = now;
    if (used == 0) {
      if (value == 0xff) bytes[used++] = value;
      return false;
    }
    if (used == 1) {
      if (value == 0x55) bytes[used++] = value;
      else used = value == 0xff ? 1 : 0;
      return false;
    }
    if (used == 2) {
      if (value < 7 || value > sizeof(bytes) - 3) {
        reset();
        if (value == 0xff) bytes[used++] = value;
        return false;
      }
      expected = value + 3;
    }
    bytes[used++] = value;
    if (used != expected) return false;
    command = {bytes[9], uint8_t(used > 10 ? bytes[10] : 0),
               uint8_t(used > 12 ? bytes[12] : 0), used > 10, used > 12};
    reset();
    return true;
  }
  void reset() { used = expected = 0; }
 private:
  uint8_t bytes[52] = {};
  size_t used = 0, expected = 0;
  uint32_t lastByte = 0;
};

inline void encodeCommand(uint8_t *packet, uint8_t action, uint8_t device, uint8_t value) {
  memset(packet, 0, 13);
  packet[0] = 0xff; packet[1] = 0x55; packet[2] = 10;
  packet[9] = action; packet[10] = device; packet[12] = value;
}

// 9 = camera light direction; 10 = camera link heartbeat (private extension).
enum : uint8_t { CMD_RUN = 1, CMD_STANDBY = 3, CMD_TRACK_1 = 4,
  CMD_TRACK_2 = 5, CMD_AVOID = 6, CMD_FOLLOW = 7, CMD_LIGHT_FOLLOW = 8,
  CMD_LIGHT_DIRECTION = 9, CMD_HEARTBEAT = 10 };
