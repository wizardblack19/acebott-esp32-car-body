#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <lwip/sockets.h>
#include <lwip/tcp.h>
#include "CommandProtocol.h"
#include "LightTracker.h"
#include "ControlPage.h"

bool CameraWebServer_init();
WiFiServer server(100);
WiFiClient tcpClient;
WebServer webServer(81);
CommandParser tcpParser, usbParser;
enum Owner { NONE, TCP, WEB, USB };
Owner owner = NONE;
String webOwner;
bool cameraReady = false, lightWorkerReady = false, lightEnabled = false;
uint32_t generation = 0, lastWeb = 0, lastUsb = 0, lastHeartbeat = 0, lastRequest = 0;
uint32_t lastResult = 0;

void sendBody(uint8_t action, uint8_t device = 0, uint8_t value = 0) {
  uint8_t packet[13]; encodeCommand(packet, action, device, value);
  Serial2.write(packet, sizeof(packet));
}
void stopControl() {
  lightEnabled = false; ++generation; owner = NONE;
  sendBody(CMD_STANDBY);
}
bool validCommand(const CarCommand &cmd) {
  if (cmd.action >= CMD_STANDBY && cmd.action <= CMD_LIGHT_FOLLOW) return true;
  if (cmd.action != CMD_RUN || !cmd.hasDevice) return false;
  if (cmd.device == 8) return true; // old 11-byte shooting packet
  if (!cmd.hasValue) return false;
  switch (cmd.device) {
    case 12: return cmd.value <= 10;
    case 2: case 35: return cmd.value <= 180;
    case 3: return cmd.value <= 4;
    case 5: case 6: return cmd.value <= 1;
    case 13: return true;
    default: return false;
  }
}
bool dispatch(const CarCommand &cmd, Owner source) {
  if (!validCommand(cmd)) return false;
  if (cmd.action == CMD_STANDBY) { stopControl(); return true; }
  if (cmd.action == CMD_LIGHT_FOLLOW && (!cameraReady || !lightWorkerReady)) return false;
  if (owner != NONE && owner != source) stopControl();
  owner = source;
  if (source == WEB) lastWeb = millis();
  if (source == USB) lastUsb = millis();
  if (cmd.action >= CMD_TRACK_1 || (cmd.action == CMD_RUN &&
      (cmd.device == 12 || cmd.device == 2 || cmd.device == 35))) {
    lightEnabled = false; ++generation;
  }
  if (cmd.action == CMD_LIGHT_FOLLOW) {
    lightEnabled = true;
    lastResult = millis(); lastRequest = millis() - 200;
    // The body receives the mode once, then directions tagged with action 9.
  }
  if (cmd.action == CMD_RUN && cmd.device == 6) digitalWrite(4, cmd.value);
  else sendBody(cmd.action, cmd.device, cmd.value);
  return true;
}

bool integerArg(const char *key, int low, int high, int &value) {
  const String text = webServer.arg(key);
  if (!text.length()) return false;
  value = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    value = value * 10 + text[i] - '0';
    if (value > high) return false;
  }
  return value >= low;
}
void handleWebControl() {
  const String cmd = webServer.arg("cmd");
  if (cmd == "ping") {
    if (owner == WEB && webServer.arg("session") == webOwner) lastWeb = millis();
    webServer.send(200, "text/plain", "ok"); return;
  }
  CarCommand command = {CMD_RUN, 0, 0, true, true};
  int value = 0;
  bool valid = true;
  if (cmd == "car") {
    const String dir = webServer.arg("direction");
    const char *names[] = {"stop", "Forward", "Backward", "Left", "Right", "LeftUp",
      "LeftDown", "RightUp", "RightDown", "Anticlockwise", "Clockwise"};
    valid = false; command.device = 12;
    for (uint8_t i = 0; i < 11; ++i) if (dir == names[i]) { command.value = i; valid = true; }
  } else if (cmd == "speed") {
    valid = integerArg("value", 1, 5, value); command.device = 13;
    const uint8_t values[] = {0, 130, 160, 190, 220, 255};
    if (valid) command.value = values[value];
  } else if (cmd == "servo") {
    valid = integerArg("angle", 0, 180, value); command.device = 2; command.value = value;
  } else if (cmd == "LED" || cmd == "CAM_LED") {
    valid = integerArg("value", 0, 1, value);
    command.device = cmd == "LED" ? 5 : 6; command.value = value;
  } else if (cmd == "Buzzer") {
    valid = integerArg("value", 0, 4, value); command.device = 3; command.value = value;
  } else if (cmd == "Track") {
    valid = integerArg("value", 1, 2, value); command.action = value == 1 ? CMD_TRACK_1 : CMD_TRACK_2;
  } else if (cmd == "Avoidance") command.action = CMD_AVOID;
  else if (cmd == "Follow") command.action = CMD_FOLLOW;
  else if (cmd == "Light") command.action = CMD_LIGHT_FOLLOW;
  else if (cmd == "Shooting") command.device = 8;
  else if (cmd == "stopA") command.action = CMD_STANDBY;
  else valid = false;
  if (!valid) { webServer.send(400, "text/plain", "Orden o valor invalido"); return; }
  if (!dispatch(command, WEB)) { webServer.send(503, "text/plain", "Camara no disponible"); return; }
  webOwner = webServer.arg("session");
  webServer.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 14, 13);
  pinMode(4, OUTPUT); digitalWrite(4, LOW);
  sendBody(CMD_STANDBY);
  cameraReady = CameraWebServer_init();
  if (cameraReady) lightWorkerReady = startLightWorker();
  server.begin();
  // Match the initial speed slider (3 / 190) even before it is moved.
  sendBody(CMD_RUN, 13, 190);
  webServer.on("/", []() { webServer.send_P(200, "text/html", html); });
  webServer.on("/control", handleWebControl);
  webServer.begin();
  Serial.println("Control: http://192.168.4.1:81; TCP: 100; video: 82/stream");
}

void loop() {
  webServer.handleClient();
  if (tcpClient && !tcpClient.connected()) {
    if (owner == TCP) stopControl();
    tcpClient.stop(); tcpParser.reset();
  }
  if (!tcpClient || !tcpClient.connected()) {
    WiFiClient incoming = server.available();
    if (incoming) {
      tcpClient = incoming; tcpClient.setNoDelay(true); tcpParser.reset();
      // Detect a vanished TCP controller even if another station remains on the AP.
      int enabled = 1, idle = 2, interval = 1, count = 2;
      tcpClient.setSocketOption(SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled));
      tcpClient.setOption(TCP_KEEPIDLE, &idle);
      tcpClient.setOption(TCP_KEEPINTVL, &interval);
      tcpClient.setOption(TCP_KEEPCNT, &count);
    }
  }
  CarCommand cmd;
  for (int n = 0; n < 256 && tcpClient.available(); ++n) {
    if (tcpParser.feed(uint8_t(tcpClient.read()), millis(), cmd)) dispatch(cmd, TCP);
  }
  for (int n = 0; n < 128 && Serial.available(); ++n) {
    if (usbParser.feed(uint8_t(Serial.read()), millis(), cmd)) dispatch(cmd, USB);
  }
  // Body diagnostics are text: never feed them back into the command stream.
  for (int n = 0; n < 128 && Serial2.available(); ++n) Serial.write(Serial2.read());
  uint32_t now = millis();
  if ((owner == TCP && (!tcpClient.connected() || WiFi.softAPgetStationNum() == 0)) ||
      (owner == WEB && (uint32_t(now - lastWeb) > 2000 || WiFi.softAPgetStationNum() == 0)) ||
      (owner == USB && uint32_t(now - lastUsb) > 1500)) stopControl();
  if (owner != NONE && uint32_t(now - lastHeartbeat) >= 250) {
    lastHeartbeat = now; sendBody(CMD_HEARTBEAT);
  }
  if (lightEnabled) {
    LightResult result;
    if (xQueueReceive(lightResults, &result, 0) == pdTRUE && result.generation == generation &&
        uint32_t(now - result.requestedAt) <= 600) {
      lastResult = now;
      sendBody(CMD_LIGHT_DIRECTION, 12, result.valid ? result.direction : 0);
    }
    if (uint32_t(now - lastResult) > 700) {
      sendBody(CMD_LIGHT_DIRECTION, 12, 0); lastResult = now;
    }
    if (uint32_t(now - lastRequest) >= 200) {
      lastRequest = now;
      const LightRequest request = {generation, now};
      xQueueOverwrite(lightRequests, &request);
    }
  }
  delay(1);
}
