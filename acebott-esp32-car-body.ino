#include <vehicle.h>
#include <ACB_Ultrasonic.h>
#include <ESP32Servo.h>
#include <Arduino.h>
#include <esp_system.h>

#define Shoot_PIN 32  //shoot---200ms
#define FIXED_SERVO_PIN 25
#define TURN_SERVO_PIN 26

#define LED_Module1 2
#define LED_Module2 12
#define Left_sensor 35
#define Middle_sensor 36
#define Right_sensor 39
#define Buzzer 33

#define CMD_RUN 1
#define CMD_GET 2
#define CMD_STANDBY 3
#define CMD_TRACK_1 4
#define CMD_TRACK_2 5
#define CMD_AVOID 6
#define CMD_FOLLOW 7
#define CMD_LIGHT_FOLLOW 8
#define CMD_LIGHT_DIRECTION 9
#define CMD_HEARTBEAT 10

#define C3 131
#define D3 147
#define E3 165
#define F3 175
#define G3 196
#define A3 221
#define B3 248

#define C4 262
#define D4 294
#define E4 330
#define F4 350
#define G4 393
#define A4 441
#define B4 495

#define DS4 311
#define FS4 370
#define GS4 415
#define AS4 466

#define C5 525
#define CS5 554
#define D5 589
#define DS5 622
#define E5 659
#define F5 698
#define FS5 740
#define G5 784
#define GS5 831
#define A5 880
#define AS5 932
#define B5 988
#define C6 1047
#define N 0

extern vehicle Acebott;
ACB_Ultrasonic Ultrasonic(13, 14);
Servo fixedServo;
Servo turnServo;

int Left_Tra_Value;
int Middle_Tra_Value;
int Right_Tra_Value;
int Black_Line = 2000;
int Off_Road = 4000;
int speeds = 250;
int leftDistance = 0;
int middleDistance = 0;
int rightDistance = 0;

String Version = "Firmware Version is 0.12.21";
byte dataLen, index_a = 0;
char buffer[52];
unsigned char prevc = 0;
bool isStart = false;
bool ED_client = true;
bool WA_en = false;
byte RX_package[17] = { 0 };
uint16_t angle = 90;
byte action = Stop, device;
byte val = 0;
char model_var = 0;
int UT_distance = -1;

int length0;
int length1;
int length2;
int length3;

// Melodia, en milisegundos. Dominio publico, una sola voz.
int tune0[] = {
  G4, G4, G4, DS4, N, F4, F4, F4, D4, N,
  G4, G4, G4, DS4, N, F4, F4, F4, D4, N,
  G4, G4, G4, DS4, F4, F4, F4, D4,
  G4, G4, G4, DS4, D4, D4, D4, C5
};
uint16_t durt0[] = {
  170, 170, 170, 900, 220, 170, 170, 170, 900, 280,
  150, 150, 150, 700, 180, 150, 150, 150, 700, 200,
  140, 140, 140, 420, 140, 140, 140, 420,
  130, 130, 130, 360, 130, 130, 130, 800
};

int tune1[] = {
  G4, D4, G4, D4, G4, D4, G4, B4, D5, N,
  C5, A4, FS4, A4, D4, FS4,
  G4, D5, B4, G4, B4, G4, A4, G4
};
uint16_t durt1[] = {
  300, 150, 300, 150, 150, 150, 150, 150, 450, 80,
  300, 150, 150, 150, 150, 450,
  150, 150, 150, 150, 150, 150, 300, 520
};

int tune2[] = {
  D5, G4, A4, B4, C5, D5, G4, G4,
  E5, C5, D5, E5, FS5, G5, G4, G4,
  C5, D5, C5, B4, A4, B4, C5, A4,
  D5, G4, A4, B4, C5, D5, G4
};
uint16_t durt2[] = {
  340, 340, 170, 170, 170, 340, 340, 340,
  340, 340, 170, 170, 170, 340, 340, 340,
  340, 170, 170, 340, 170, 170, 340, 340,
  340, 340, 170, 170, 170, 340, 680
};

int tune3[] = {
  B4, CS5, D5, E5, B4, CS5, D5, E5, B4, CS5, D5, E5, D5, CS5, B4, N,
  B4, CS5, D5, E5, B4, CS5, D5, E5, B4, CS5, D5, E5, D5, CS5, B4, N,
  B4, CS5, D5, E5, B4, CS5, D5, E5, B4, CS5, D5, E5, D5, CS5, B4,
  E5, E5, B5, E5
};
uint16_t durt3[] = {
  280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 240,
  170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 170, 140,
  95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95, 95,
  110, 110, 220, 700
};

uint32_t lastLink = 0, lastLight = 0, lastSensor = 0;
bool linkActive = false;
uint8_t lightDirection = 0;
uint8_t avoidStep = 0;
int avoidChoice = 0;
uint32_t avoidDeadline = 0;
bool shooting = false;
uint32_t shotDeadline = 0;
int song = -1, noteIndex = 0;
uint32_t noteDeadline = 0;

unsigned char readBuffer(int index_r) {
  if (index_r < 0 || index_r >= (int)sizeof(buffer)) return 0;
  return buffer[index_r];
}
void writeBuffer(int index_w, unsigned char c) {
  if (index_w < 0 || index_w >= (int)sizeof(buffer)) return;
  buffer[index_w] = c;
}

enum FUNCTION_MODE {
  STANDBY,
  FOLLOW,
  TRACK_1,
  TRACK_2,
  AVOID,
  LIGHT_FOLLOW
} function_mode;

int newRunMode = 0;

bool due(uint32_t now, uint32_t deadline) { return int32_t(now - deadline) >= 0; }

void stopCar(bool center) {
  newRunMode = 0;
  avoidStep = 0;
  function_mode = STANDBY;
  lightDirection = 0;
  song = -1;
  silenceBuzzer();
  shooting = false;
  digitalWrite(Shoot_PIN, LOW);
  Acebott.Move(Stop, 0);
  if (center) {
    turnServo.write(90);
    fixedServo.write(90);
  }
}

void refreshDistance() {
  uint32_t now = millis();
  if (uint32_t(now - lastSensor) < 65) return;
  lastSensor = now;
  UT_distance = Ultrasonic.getData();
  middleDistance = UT_distance;
}

void silenceBuzzer() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(Buzzer, 0);
#else
  ledcWriteTone(15, 0);
#endif
}

void playTone(int frequency) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(Buzzer, frequency > 0 ? frequency : 0);
#else
  ledcWriteTone(15, frequency > 0 ? frequency : 0);
#endif
}

void setupBuzzer() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(Buzzer, 2000, 10);
#else
  ledcSetup(15, 2000, 10);
  ledcAttachPin(Buzzer, 15);
#endif
  silenceBuzzer();
}

void updateMusic() {
  if (song < 0 || !due(millis(), noteDeadline)) return;
  const int *notes[] = { tune0, tune1, tune2, tune3 };
  const uint16_t *durations[] = { durt0, durt1, durt2, durt3 };
  const int lengths[] = { length0, length1, length2, length3 };
  if (noteIndex >= lengths[song]) {
    song = -1;
    silenceBuzzer();
    return;
  }
  // tone() no cambia de nota en el ESP32 mientras el canal sigue ocupado.
  playTone(notes[song][noteIndex]);
  noteDeadline = millis() + durations[song][noteIndex];
  noteIndex++;
}

void setup() {
  Serial.setTimeout(10);
  Serial.begin(115200);
  randomSeed(esp_random());

  Acebott.Init();

  pinMode(LED_Module1, OUTPUT);
  pinMode(LED_Module2, OUTPUT);
  pinMode(Shoot_PIN, OUTPUT);
  pinMode(Left_sensor, INPUT);
  pinMode(Middle_sensor, INPUT);
  pinMode(Right_sensor, INPUT);
  digitalWrite(Shoot_PIN, LOW);
  Ultrasonic.setpin(13, 14);

  ESP32PWM::allocateTimer(1);
  fixedServo.attach(FIXED_SERVO_PIN);
  fixedServo.write(angle);
  turnServo.attach(TURN_SERVO_PIN);
  turnServo.write(angle);
  setupBuzzer();
  Acebott.Move(Stop, 0);
  delay(3000);

  length0 = sizeof(tune0) / sizeof(tune0[0]);
  length1 = sizeof(tune1) / sizeof(tune1[0]);
  length2 = sizeof(tune2) / sizeof(tune2[0]);
  length3 = sizeof(tune3) / sizeof(tune3[0]);

  Serial.println("ACEBOT listo. Escuchando Monitor Serie a 115200 baudios...");
}

void loop() {
  RXpack_func();
  uint32_t now = millis();
  if (linkActive && uint32_t(now - lastLink) > 1500) {
    linkActive = false;
    stopCar(true);
    Serial.println("Parada: enlace perdido");
  }
  if (shooting && due(now, shotDeadline)) {
    digitalWrite(Shoot_PIN, LOW);
    shooting = false;
  }
  updateMusic();
  switch (newRunMode) {
    case 4:
      model1_func();
      break;
    case 5:
      model4_func();
      break;
    case 6:
      function_mode = AVOID;
      model2_func();
      break;
    case 7:
      model3_func();
      break;
    case 8:
      function_mode = LIGHT_FOLLOW;
      model_light_func();
      break;
    case 3:
      function_mode = STANDBY;
      newRunMode = 0;
      break;
  }
}

void functionMode() {
  switch (function_mode) {
    case FOLLOW:
      model3_func();
      break;
    case TRACK_1:
      model1_func();
      break;
    case TRACK_2:
      model4_func();
      break;
    case AVOID:
      model2_func();
      break;
    case LIGHT_FOLLOW:
      model_light_func();
      break;
    default:
      break;
  }
}

void Receive_data() {
}

// La camara esta en la otra placa. Este modo solo obedece la direccion que ella envia.
void model_light_func() {
  uint32_t now = millis();
  refreshDistance();
  if (UT_distance < 0 || UT_distance <= 25 || uint32_t(now - lastLight) > 700) {
    Acebott.Move(Stop, 0);
    return;
  }
  if (lightDirection == 1) Acebott.Move(Forward, 160);
  else if (lightDirection == 9) Acebott.Move(Contrarotate, 140);
  else if (lightDirection == 10) Acebott.Move(Clockwise, 140);
  else Acebott.Move(Stop, 0);
}

void model2_func() {
  uint32_t now = millis();
  if (avoidStep) {
    if (!due(now, avoidDeadline)) return;
    switch (avoidStep) {
      case 1:
        turnServo.write(45);
        avoidDeadline = now + 200;
        avoidStep = 2;
        break;
      case 2:
        turnServo.write(135);
        avoidDeadline = now + 200;
        avoidStep = 3;
        break;
      case 3:
        turnServo.write(90);
        avoidChoice = random(0, 4);
        if (avoidChoice < 2) Acebott.Move(Backward, 180);
        else if (avoidChoice == 2) Acebott.Move(Clockwise, 180);
        else Acebott.Move(Contrarotate, 180);
        avoidDeadline = now + (avoidChoice < 2 ? 500 : 1000);
        avoidStep = 4;
        break;
      case 4:
        if (avoidChoice < 2) {
          Acebott.Move(avoidChoice == 0 ? Move_Left : Move_Right, 180);
          avoidDeadline = now + 500;
          avoidStep = 5;
        } else {
          Acebott.Move(Stop, 0);
          avoidStep = 0;
        }
        break;
      default:
        Acebott.Move(Stop, 0);
        avoidStep = 0;
        break;
    }
    return;
  }

  refreshDistance();
  if (UT_distance < 0) {
    Acebott.Move(Stop, 0);
    return;
  }
  if (UT_distance <= 25) {
    Acebott.Move(Stop, 0);
    avoidStep = 1;
    avoidDeadline = now + 100;
  } else {
    Acebott.Move(Forward, 180);
  }
}

void model3_func() {
  turnServo.write(90);
  refreshDistance();
  if (UT_distance < 0 || UT_distance > 50) {
    Acebott.Move(Stop, 0);
  } else if (UT_distance < 15) {
    Acebott.Move(Backward, 200);
  } else if (UT_distance <= 20) {
    Acebott.Move(Stop, 0);
  } else if (UT_distance <= 25) {
    Acebott.Move(Forward, max(0, speeds - 70));
  } else {
    Acebott.Move(Forward, 220);
  }
}

void model4_func() {
  turnServo.write(90);
  Left_Tra_Value = analogRead(Left_sensor);
  Middle_Tra_Value = analogRead(Middle_sensor);
  Right_Tra_Value = analogRead(Right_sensor);
  if (Left_Tra_Value < Black_Line && Middle_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line) {
    Acebott.Move(Forward, 180);
  }
  if (Left_Tra_Value < Black_Line && Middle_Tra_Value >= Black_Line && Right_Tra_Value >= Black_Line) {
    Acebott.Move(Forward, 180);
  }
  if (Left_Tra_Value >= Black_Line && Middle_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line) {
    Acebott.Move(Forward, 180);
  } else if (Left_Tra_Value >= Black_Line && Middle_Tra_Value < Black_Line && Right_Tra_Value < Black_Line) {
    Acebott.Move(Contrarotate, 220);
  } else if (Left_Tra_Value < Black_Line && Middle_Tra_Value < Black_Line && Right_Tra_Value >= Black_Line) {
    Acebott.Move(Clockwise, 220);
  } else if (Left_Tra_Value >= Off_Road && Middle_Tra_Value >= Off_Road && Right_Tra_Value >= Off_Road) {
    Acebott.Move(Stop, 0);
  }
}

void model1_func() {
  Left_Tra_Value = analogRead(Left_sensor);
  Right_Tra_Value = analogRead(Right_sensor);
  if (Left_Tra_Value < Black_Line && Right_Tra_Value < Black_Line) {
    Acebott.Move(Forward, 130);
  } else if (Left_Tra_Value >= Black_Line && Right_Tra_Value < Black_Line) {
    Acebott.Move(Contrarotate, 150);
  } else if (Left_Tra_Value < Black_Line && Right_Tra_Value >= Black_Line) {
    Acebott.Move(Clockwise, 150);
  } else if (Left_Tra_Value >= Black_Line && Left_Tra_Value < Off_Road && Right_Tra_Value >= Black_Line && Right_Tra_Value < Off_Road) {
    Acebott.Move(Stop, 0);
  } else if (Left_Tra_Value >= Off_Road && Right_Tra_Value >= Off_Road) {
    Acebott.Move(Stop, 0);
  }
}

void Servo_Move(int val_app) {
  int servo_angle = constrain(val_app, 0, 180);
  turnServo.write(servo_angle);
  fixedServo.write(servo_angle);
}

void Buzzer_run(int M) {
  if (M < 1 || M > 4) {
    song = -1;
    silenceBuzzer();
    return;
  }
  song = M - 1;
  noteIndex = 0;
  noteDeadline = millis();
}

void runModule(int device) {
  val = readBuffer(12);
  switch (device) {
    case 0x0C:
      newRunMode = 0;
      avoidStep = 0;
      switch (val) {
        case 0x01: Acebott.Move(Forward, speeds); break;
        case 0x02: Acebott.Move(Backward, speeds); break;
        case 0x03: Acebott.Move(Move_Left, speeds); break;
        case 0x04: Acebott.Move(Move_Right, speeds); break;
        case 0x05: Acebott.Move(Top_Left, speeds); break;
        case 0x06: Acebott.Move(Bottom_Left, speeds); break;
        case 0x07: Acebott.Move(Top_Right, speeds); break;
        case 0x08: Acebott.Move(Bottom_Right, speeds); break;
        case 0x0A: Acebott.Move(Clockwise, speeds); break;
        case 0x09: Acebott.Move(Contrarotate, speeds); break;
        case 0x00: Acebott.Move(Stop, 0); break;
        default: break;
      }
      break;
    case 0x02:
    case 35:
      newRunMode = 0;
      avoidStep = 0;
      Servo_Move(val);
      break;
    case 0x03:
      Buzzer_run(val);
      break;
    case 0x05:
      digitalWrite(LED_Module1, val);
      digitalWrite(LED_Module2, val);
      break;
    case 0x08:
      shooting = true;
      shotDeadline = millis() + 200;
      digitalWrite(Shoot_PIN, HIGH);
      break;
    case 0x0D:
      speeds = val;
      break;
  }
}

void parseData() {
  isStart = false;
  int action = readBuffer(9);
  int device = readBuffer(10);

  if (action == CMD_HEARTBEAT) {
    if (linkActive) lastLink = millis();
    return;
  }
  if (action == CMD_LIGHT_DIRECTION) {
    int dir = readBuffer(12);
    if (newRunMode == CMD_LIGHT_FOLLOW && device == 0x0C && dir >= 0 && dir <= 10) {
      lightDirection = dir;
      lastLight = lastLink = millis();
    }
    return;
  }

  Serial.print("Accion: ");
  Serial.print(action);
  Serial.print(" | Dispositivo: ");
  Serial.print(device);
  Serial.print(" | Valor: ");
  Serial.println(readBuffer(12));

  linkActive = true;
  lastLink = millis();

  if (action >= CMD_TRACK_1 && action <= CMD_LIGHT_FOLLOW) {
    avoidStep = 0;
    newRunMode = action;
    if (action == CMD_LIGHT_FOLLOW) {
      lightDirection = 0;
      lastLight = millis();
      turnServo.write(90);
    }
  }

  switch (action) {
    case CMD_RUN:
      if (device != 2 && device != 35) function_mode = STANDBY;
      runModule(device);
      break;
    case CMD_STANDBY:
      stopCar(true);
      linkActive = false;
      break;
    default:
      break;
  }
}

void RXpack_func() {
  for (int count = 0; count < 64 && Serial.available() > 0; ++count) {
    unsigned char c = Serial.read() & 0xff;

    if (c == 0x55 && isStart == false) {
      if (prevc == 0xff) {
        index_a = 1;
        isStart = true;
      }
    } else {
      prevc = c;
      if (isStart) {
        if (index_a < 0 || index_a >= (int)sizeof(buffer)) {
          index_a = 0;
          isStart = false;
          dataLen = 0;
          prevc = c;
          continue;
        }
        if (index_a == 2) dataLen = c;
        else if (index_a > 2) dataLen--;
        writeBuffer(index_a, c);
      }
    }
    index_a++;
    if (isStart && dataLen == 0 && index_a > 3) {
      isStart = false;
      parseData();
      index_a = 0;
    }
  }
}
