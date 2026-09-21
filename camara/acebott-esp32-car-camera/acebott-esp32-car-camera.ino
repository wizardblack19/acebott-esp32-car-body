#include <WiFi.h>
#include "esp_camera.h"
#include <WebServer.h>
#include "html.h"
#include "LightTracker.h"
#include <vector>

WiFiServer server(100);
WiFiClient tcpClient;
WebServer webServer(81);

String Version = "1.0.1";
byte dataLen, index_a = 0, frameLen = 0;
char buffer[52];
unsigned char prevc = 0;
bool isStart = false;

bool cameraReady = false, lightReady = false, lightEnabled = false, linkActive = false;
uint32_t generation = 0, lastHeartbeat = 0, lastResult = 0, lastRequest = 0;

unsigned char readBuffer(int index_r) {
  if (index_r < 0 || index_r >= (int)sizeof(buffer)) return 0;
  return buffer[index_r];
}
void writeBuffer(int index_w, unsigned char c) {
  if (index_w < 0 || index_w >= (int)sizeof(buffer)) return;
  buffer[index_w] = c;
}
void resetParser() {
  index_a = 0;
  dataLen = 0;
  frameLen = 0;
  isStart = false;
}

#define gpLED 4
#define RXD2 14
#define TXD2 13
bool CameraWebServer_init();

void sendAction(uint8_t action) {
  uint8_t packet[10] = { 0xff, 0x55, 0x07, 0, 0, 0, 0, 0, 0, action };
  Serial2.write(packet, sizeof(packet));
}
void sendBody(uint8_t action, uint8_t device, uint8_t value) {
  uint8_t packet[13] = {};
  packet[0] = 0xff;
  packet[1] = 0x55;
  packet[2] = 10;
  packet[9] = action;
  packet[10] = device;
  packet[12] = value;
  Serial2.write(packet, sizeof(packet));
}
void sendStandby() {
  lightEnabled = false;
  linkActive = false;
  sendAction(3);
}
void noteCommand(int action, int device) {
  if (action == 3) {
    lightEnabled = false;
    linkActive = false;
    return;
  }
  linkActive = true;
  if (action == 8 && lightReady) {
    lightEnabled = true;
    ++generation;
    lastResult = millis();
    lastRequest = millis() - 200;
  } else if ((action >= 4 && action <= 7) || (action == 1 && device == 0x0C)) {
    lightEnabled = false;
  }
}

void handleWebControl() {
  const String cmd = webServer.arg("cmd");
  Serial.println(cmd);
  std::vector<uint8_t> data;
  bool localOnly = false;

  if (cmd == "car") {
    const String direction = webServer.arg("direction");
    int code = -1;
    if (direction == "stop") code = 0x00;
    else if (direction == "Forward") code = 0x01;
    else if (direction == "Backward") code = 0x02;
    else if (direction == "Left") code = 0x03;
    else if (direction == "Right") code = 0x04;
    else if (direction == "LeftUp") code = 0x05;
    else if (direction == "LeftDown") code = 0x06;
    else if (direction == "RightUp") code = 0x07;
    else if (direction == "RightDown") code = 0x08;
    else if (direction == "Anticlockwise") code = 0x09;
    else if (direction == "Clockwise") code = 0x0A;
    if (code < 0) {
      webServer.send(400, "text/plain", "Orden invalida");
      return;
    }
    data = { 0xFF, 0x55, 0x0A, 0, 0, 0, 0, 0, 0, 0x01, 0x0C, 0, (uint8_t)code };
    noteCommand(1, 0x0C);
  } else if (cmd == "speed") {
    int value = webServer.arg("value").toInt();
    const uint8_t speeds[] = { 0, 0x82, 0xA0, 0xBE, 0xDC, 0xFF };
    if (value < 1 || value > 5) {
      webServer.send(400, "text/plain", "Orden invalida");
      return;
    }
    data = { 0xFF, 0x55, 0x0A, 0, 0, 0, 0, 0, 0, 0x01, 0x0D, 0, speeds[value] };
    noteCommand(1, 0x0D);
  } else if (cmd == "servo") {
    int angle = constrain(webServer.arg("angle").toInt(), 0, 180);
    data = { 0xFF, 0x55, 0x0A, 0, 0, 0, 0, 0, 0, 0x01, 0x02, 0, (uint8_t)angle };
    noteCommand(1, 0x02);
  } else if (cmd == "LED") {
    const String value = webServer.arg("value");
    if (value != "0" && value != "1") {
      webServer.send(400, "text/plain", "Orden invalida");
      return;
    }
    data = { 0xFF, 0x55, 0x0A, 0, 0, 0, 0, 0, 0, 0x01, 0x05, 0, (uint8_t)(value == "1") };
    noteCommand(1, 0x05);
  } else if (cmd == "CAM_LED") {
    const String value = webServer.arg("value");
    if (value == "1") digitalWrite(gpLED, HIGH);
    else if (value == "0") digitalWrite(gpLED, LOW);
    else {
      webServer.send(400, "text/plain", "Orden invalida");
      return;
    }
    localOnly = true;
  } else if (cmd == "Buzzer") {
    const String value = webServer.arg("value");
    if (value != "0" && value != "1" && value != "2" && value != "3" && value != "4") {
      webServer.send(400, "text/plain", "Orden invalida");
      return;
    }
    data = { 0xFF, 0x55, 0x0A, 0, 0, 0, 0, 0, 0, 0x01, 0x03, 0, (uint8_t)value.toInt() };
    noteCommand(1, 0x03);
  } else if (cmd == "Track") {
    const String value = webServer.arg("value");
    if (value != "1" && value != "2") {
      webServer.send(400, "text/plain", "Orden invalida");
      return;
    }
    data = { 0xFF, 0x55, 0x07, 0, 0, 0, 0, 0, 0, (uint8_t)(value == "1" ? 0x04 : 0x05) };
    noteCommand(value == "1" ? 4 : 5, 0);
  } else if (cmd == "Avoidance") {
    data = { 0xFF, 0x55, 0x07, 0, 0, 0, 0, 0, 0, 0x06 };
    noteCommand(6, 0);
  } else if (cmd == "Follow") {
    data = { 0xFF, 0x55, 0x07, 0, 0, 0, 0, 0, 0, 0x07 };
    noteCommand(7, 0);
  } else if (cmd == "Light") {
    if (!lightReady) {
      webServer.send(503, "text/plain", "Camara no disponible");
      return;
    }
    data = { 0xFF, 0x55, 0x07, 0, 0, 0, 0, 0, 0, 0x08 };
    noteCommand(8, 0);
  } else if (cmd == "Shooting") {
    data = { 0xFF, 0x55, 0x08, 0, 0, 0, 0, 0, 0, 0x01, 0x08 };
    noteCommand(1, 0x08);
  } else if (cmd == "stopA") {
    data = { 0xFF, 0x55, 0x07, 0, 0, 0, 0, 0, 0, 0x03 };
    noteCommand(3, 0);
  } else {
    webServer.send(400, "text/plain", "Orden invalida");
    return;
  }

  if (!localOnly) {
    if (data.empty()) {
      webServer.send(400, "text/plain", "Orden invalida");
      return;
    }
    Serial2.write(data.data(), data.size());
  }
  webServer.send(200, "text/plain", "ok");
}

void feedTcp(uint8_t c) {
  if (c == 0x55 && isStart == false) {
    if (prevc == 0xff) {
      index_a = 1;
      isStart = true;
    }
  } else {
    prevc = c;
    if (isStart) {
      if (index_a < 0 || index_a >= (int)sizeof(buffer)) {
        resetParser();
        prevc = c;
        return;
      }
      if (index_a == 2) {
        dataLen = c;
        frameLen = c;
      } else if (index_a > 2) dataLen--;
      writeBuffer(index_a, c);
    }
  }
  index_a++;
  if (isStart && dataLen == 0 && index_a > 3) {
    int action = readBuffer(9);
    int device = frameLen >= 8 ? readBuffer(10) : 0;
    int val = frameLen >= 10 ? readBuffer(12) : 0;
    noteCommand(action, device);
    if (action == 1 && device == 0x06 && frameLen >= 10) digitalWrite(gpLED, val ? HIGH : LOW);
    resetParser();
  }
}

void updateLight() {
  if (!lightEnabled) return;
  uint32_t now = millis();
  LightResult result;
  if (xQueueReceive(lightResults, &result, 0) == pdTRUE && result.generation == generation &&
      uint32_t(now - result.requestedAt) <= 600) {
    lastResult = now;
    sendBody(9, 12, result.valid ? result.direction : 0);
  }
  if (uint32_t(now - lastResult) > 700) {
    sendBody(9, 12, 0);
    lastResult = now;
  }
  if (uint32_t(now - lastRequest) >= 200) {
    lastRequest = now;
    const LightRequest request = { generation, now };
    xQueueOverwrite(lightRequests, &request);
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  cameraReady = CameraWebServer_init();
  if (cameraReady) lightReady = startLightWorker();
  server.begin();
  delay(100);

  pinMode(gpLED, OUTPUT);
  digitalWrite(gpLED, LOW);
  for (int i = 0; i < 5; i++) {
    digitalWrite(gpLED, HIGH);
    delay(20);
    digitalWrite(gpLED, LOW);
    delay(50);
  }

  webServer.on("/", []() { webServer.send(200, "text/html", html); });
  webServer.on("/control", handleWebControl);
  webServer.begin();
  sendStandby();
}

void loop() {
  webServer.handleClient();

  if (tcpClient && !tcpClient.connected()) {
    tcpClient.stop();
    resetParser();
    if (linkActive) sendStandby();
  }
  if (!tcpClient || !tcpClient.connected()) {
    WiFiClient incoming = server.available();
    if (incoming) {
      tcpClient = incoming;
      tcpClient.setNoDelay(true);
      resetParser();
      linkActive = true;
    }
  }
  for (int n = 0; n < 128 && tcpClient.connected() && tcpClient.available(); ++n) {
    uint8_t clientBuff = tcpClient.read() & 0xff;
    Serial2.write(clientBuff);
    Serial.write(clientBuff);
    feedTcp(clientBuff);
  }

  // No mezclar latidos ni direcciones de luz en medio de un paquete TCP a medias.
  if (linkActive && WiFi.softAPgetStationNum() == 0) {
    resetParser();
    sendStandby();
  } else if (!isStart) {
    if (linkActive && uint32_t(millis() - lastHeartbeat) >= 250) {
      lastHeartbeat = millis();
      sendAction(10);
    }
    updateLight();
  }
}
