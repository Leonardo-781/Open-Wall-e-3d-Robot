// =============================================================================
// ROBÔ WALL-E — CO-PROCESSADOR DE MOVIMENTO (ARDUINO NANO SHIELD)
// =============================================================================
// Hardware:
// 1. Ponte H (2 Motores DC de Tração):
//    - Motor Esquerdo: Pino 4 (Digital DIR/IN1) e Pino 5 (PWM Timer 0/IN2)
//    - Motor Direito:  Pino 6 (PWM Timer 0/IN3) e Pino 7 (Digital DIR/IN4)
// 2. 7 Servomotores SG90 (Controle Direto pelo Nano Shield via Servo.h):
//    - D8:  Servo 0 (Olho Esquerdo)
//    - D9:  Servo 1 (Olho Direito)
//    - A1:  Servo 2 (Pescoço Pan - Rastreamento Horizontal)
//    - A2:  Servo 3 (Pescoço Tilt - Rastreamento Vertical)
//    - A3:  Servo 4 (Pescoço Roll - Inclinação Lateral)
//    - D10: Servo 5 (Braço Esquerdo)
//    - D11: Servo 6 (Braço Direito)
// 3. Display OLED 0.96" SSD1306 (0x3C via I2C: A4 = SDA, A5 = SCL):
//    - Orientação 90° (Vertical 64x128) com Olhos Animados e Expressões
// 4. Sensor de Voltagem da Bateria: Pino A0 com filtro de média móvel
// 5. Comunicação Serial: 38400 baud via USB D0/D1 com Raspberry Pi 3B
//
// Blindagens Implementadas:
// - Deadman Watchdog (trava motores de tração se perder sinal por >800ms)
// - Slew-Rate Limiter (interpolação suave de 2.0 graus/ciclo para evitar picos de corrente)
// - Buffer de leitura serial não-bloqueante com proteção contra overflow
// - Page Buffer U8g2 de baixo consumo de RAM (~128 bytes no ATmega328P)
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <Servo.h>
#include <U8g2lib.h>

// =============================================================================
// CONFIGURAÇÕES DE PINAGEM E CALIBRAÇÃO
// =============================================================================

constexpr uint8_t PIN_LED_STATUS = 13; // LED onboard para confirmação visual de sinal

// Ponte H (Pinos 4, 5, 6, 7)
constexpr uint8_t PIN_M_L1 = 4; // Motor Esquerdo Pino 1 (Digital DIR/IN1)
constexpr uint8_t PIN_M_L2 = 5; // Motor Esquerdo Pino 2 (PWM Timer 0/IN2)
constexpr uint8_t PIN_M_R1 = 6; // Motor Direito Pino 1 (PWM Timer 0/IN3)
constexpr uint8_t PIN_M_R2 = 7; // Motor Direito Pino 2 (Digital DIR/IN4)

constexpr bool INVERT_LEFT_MOTOR  = false;
constexpr bool INVERT_RIGHT_MOTOR = true; // Invertido para calibrar polaridade física da esteira direita

// Sensor de Voltagem da Bateria
constexpr uint8_t PIN_BATTERY = A0;
constexpr float BATTERY_DIVIDER_RATIO = 5.0f; // Módulo padrão 0-25V divisor 5:1

// Servos SG90 (Controle direto pelo Shield: D8 a D11 e A1 a A3)
constexpr uint8_t SERVO_COUNT = 7;
constexpr uint8_t SERVO_PINS[SERVO_COUNT] = {
  8,   // Servo 0: Pino D8  (Olho Esquerdo)
  9,   // Servo 1: Pino D9  (Olho Direito)
  A1,  // Servo 2: Pino A1  (Pescoço Pan - Rastreamento Horizontal)
  A2,  // Servo 3: Pino A2  (Pescoço Tilt - Rastreamento Vertical)
  A3,  // Servo 4: Pino A3  (Pescoço Roll - Inclinação Lateral)
  10,  // Servo 5: Pino D10 (Braço Esquerdo)
  11   // Servo 6: Pino D11 (Braço Direito)
};

float servoStepPerCycle = 0.8f; // Graus por ciclo de 20ms (Padrão 0.8 deg/ciclo = 40 deg/s - movimento suave e sem trancos)
Servo servos[SERVO_COUNT];

// Display OLED 0.96" SSD1306 a 90° (Vertical: 64 largura x 128 altura)
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R1, /* reset=*/ U8X8_PIN_NONE);

// =============================================================================
// VARIÁVEIS DE ESTADO DO SISTEMA
// =============================================================================

// Motores DC
int targetSpeedL = 0;
int targetSpeedR = 0;
int currentSpeedL = 0;
int currentSpeedR = 0;
uint32_t lastDriveCommandTime = 0;
constexpr uint32_t DRIVE_WATCHDOG_TIMEOUT_MS = 800;

// Servomotores (Ângulos e Interpolação Suave)
float currentServoAngle[SERVO_COUNT] = {90, 90, 90, 90, 90, 90, 90};
int   targetServoAngle[SERVO_COUNT]  = {90, 90, 90, 90, 90, 90, 90};

// Expressões Faciais no OLED
enum Expression : uint8_t {
  EXP_NORMAL = 0,   // Olhos normais e atentos
  EXP_CURIOSO = 1,  // Um olho inclinado, focado
  EXP_PISCAR = 2,   // Piscada rápida
  EXP_TRISTE = 3,   // Olhos caídos expressivos
  EXP_BRAVO = 4,    // Olhos determinados
  EXP_FELIZ = 5,    // Olhos arqueados alegres (Pose p/ foto)
  EXP_AMOR = 6      // Modo EVA / Amor com coração
};
Expression currentExpression = EXP_NORMAL;
uint32_t expressionStartTime = 0;
uint32_t lastAutoBlinkTime = 0;
uint32_t nextBlinkInterval = 4000;

// Telemetria e Comunicação
float currentBatteryVoltage = 7.4f;
uint32_t lastPacketReceivedTime = 0;
bool commActive = false;
uint8_t animTick = 0;

// Temporizações de Loops Não-Bloqueantes
uint32_t lastServoUpdate = 0;
uint32_t lastBatteryTelemetry = 0;
uint32_t lastOledUpdate = 0;
uint32_t lastMotorUpdate = 0;

// Buffer Serial
char hwRxBuffer[32];
uint8_t hwRxIdx = 0;

// =============================================================================
// CONTROLE DOS MOTORES DC (PONTE H)
// =============================================================================

void applyLeftMotor(int speed) {
  if (INVERT_LEFT_MOTOR) speed = -speed;
  speed = constrain(speed, -255, 255);

  if (abs(speed) < 15) { // Parada / Deadband
    digitalWrite(PIN_M_L1, LOW);
    digitalWrite(PIN_M_L2, LOW);
  } else if (speed > 0) {
    digitalWrite(PIN_M_L1, LOW);
    if (speed >= 250) {
      digitalWrite(PIN_M_L2, HIGH); // 100% Força Total Contínua (Sem perdas PWM)
    } else {
      analogWrite(PIN_M_L2, speed);
    }
  } else {
    digitalWrite(PIN_M_L1, HIGH);
    if (speed <= -250) {
      digitalWrite(PIN_M_L2, LOW);  // 100% Força Total Contínua
    } else {
      analogWrite(PIN_M_L2, 255 - (-speed));
    }
  }
}

void applyRightMotor(int speed) {
  if (INVERT_RIGHT_MOTOR) speed = -speed;
  speed = constrain(speed, -255, 255);

  if (abs(speed) < 15) { // Parada / Deadband
    digitalWrite(PIN_M_R1, LOW);
    digitalWrite(PIN_M_R2, LOW);
  } else if (speed > 0) {
    digitalWrite(PIN_M_R2, LOW);
    if (speed >= 250) {
      digitalWrite(PIN_M_R1, HIGH); // 100% Força Total Contínua (Sem perdas PWM)
    } else {
      analogWrite(PIN_M_R1, speed);
    }
  } else {
    digitalWrite(PIN_M_R2, HIGH);
    if (speed <= -250) {
      digitalWrite(PIN_M_R1, LOW);  // 100% Força Total Contínua
    } else {
      analogWrite(PIN_M_R1, 255 - (-speed));
    }
  }
}

void emergencyStop() {
  targetSpeedL = 0;
  targetSpeedR = 0;
  currentSpeedL = 0;
  currentSpeedR = 0;
  digitalWrite(PIN_M_L1, LOW);
  digitalWrite(PIN_M_L2, LOW);
  digitalWrite(PIN_M_R1, LOW);
  digitalWrite(PIN_M_R2, LOW);
}

void updateMotors() {
  const uint32_t now = millis();

  // Watchdog de Tração: 800ms sem comando = STOP automático
  if (now - lastDriveCommandTime > DRIVE_WATCHDOG_TIMEOUT_MS) {
    targetSpeedL = 0;
    targetSpeedR = 0;
  }

  // Resposta direta com Torque Máximo Imediato (sem delay de rampa)
  currentSpeedL = targetSpeedL;
  currentSpeedR = targetSpeedR;

  applyLeftMotor(currentSpeedL);
  applyRightMotor(currentSpeedR);
}

// =============================================================================
// CONTROLE DOS 7 SERVOS (VELOCIDADE TOTAL E TORQUE MÁXIMO)
// =============================================================================

void updateServos() {
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    float diff = (float)targetServoAngle[i] - currentServoAngle[i];
    if (fabsf(diff) <= servoStepPerCycle) {
      currentServoAngle[i] = (float)targetServoAngle[i];
    } else if (diff > 0.0f) {
      currentServoAngle[i] += servoStepPerCycle;
    } else {
      currentServoAngle[i] -= servoStepPerCycle;
    }
    servos[i].write((int)(currentServoAngle[i] + 0.5f));
  }
}

// =============================================================================
// DISPLAY OLED 0.96" A 90° (VERTICAL: 64x128)
// =============================================================================

void drawEyeV(int cx, int cy, int pupilOffsetX, int pupilOffsetY, int tiltY) {
  u8g2.drawRFrame(cx - 13, cy - 15, 26, 30, 5);
  u8g2.drawRFrame(cx - 12, cy - 14, 24, 28, 4);

  int px = cx + pupilOffsetX;
  int py = cy + pupilOffsetY + tiltY;
  u8g2.drawDisc(px, py, 5);
  u8g2.drawCircle(px, py, 7);

  u8g2.setDrawColor(0);
  u8g2.drawDisc(px - 1, py - 1, 1);
  u8g2.setDrawColor(1);
}

void drawBlinkEyeV(int cx, int cy) {
  u8g2.drawRBox(cx - 13, cy - 2, 26, 4, 1);
}

void drawHappyEyeV(int cx, int cy) {
  drawEyeV(cx, cy - 2, 0, -1, 0);
  // Pálpebra inferior arqueada feliz
  u8g2.setDrawColor(0);
  u8g2.drawBox(cx - 10, cy + 5, 20, 9);
  u8g2.setDrawColor(1);
  u8g2.drawLine(cx - 10, cy + 7, cx - 5, cy + 4);
  u8g2.drawLine(cx - 5, cy + 4, cx + 5, cy + 4);
  u8g2.drawLine(cx + 5, cy + 4, cx + 10, cy + 7);
  u8g2.drawLine(cx - 10, cy + 8, cx - 5, cy + 5);
  u8g2.drawLine(cx - 5, cy + 5, cx + 5, cy + 5);
  u8g2.drawLine(cx + 5, cy + 5, cx + 10, cy + 8);
}

void drawHeartIcon(int cx, int cy) {
  u8g2.drawDisc(cx - 2, cy - 1, 2);
  u8g2.drawDisc(cx + 2, cy - 1, 2);
  u8g2.drawTriangle(cx - 4, cy, cx + 4, cy, cx, cy + 5);
}

void renderVerticalOled() {
  const uint32_t now = millis();
  commActive = (now - lastPacketReceivedTime < 1500);

  u8g2.firstPage();
  do {
    // 1. Olhos do WALL-E
    switch (currentExpression) {
      case EXP_NORMAL:
        drawEyeV(17, 22, 0, 0, 0);
        drawEyeV(47, 22, 0, 0, 0);
        break;

      case EXP_CURIOSO:
        drawEyeV(17, 23, 2, -1, 0);
        drawEyeV(47, 19, 2, -2, -2);
        u8g2.drawLine(35, 6, 59, 10);
        break;

      case EXP_PISCAR:
        drawBlinkEyeV(17, 22);
        drawBlinkEyeV(47, 22);
        break;

      case EXP_TRISTE:
        drawEyeV(17, 24, 0, 2, 1);
        drawEyeV(47, 24, 0, 2, 1);
        u8g2.drawLine(5, 12, 27, 8);
        u8g2.drawLine(37, 8, 59, 12);
        break;

      case EXP_BRAVO:
        drawEyeV(17, 22, 0, 0, 0);
        drawEyeV(47, 22, 0, 0, 0);
        u8g2.drawLine(5, 7, 27, 13);
        u8g2.drawLine(5, 8, 27, 14);
        u8g2.drawLine(37, 13, 59, 7);
        u8g2.drawLine(37, 14, 59, 8);
        break;

      case EXP_FELIZ:
        drawHappyEyeV(17, 22);
        drawHappyEyeV(47, 22);
        break;

      case EXP_AMOR:
        drawEyeV(17, 22, 1, -1, 0);
        drawEyeV(47, 22, -1, -1, 0);
        drawHeartIcon(32, 8);
        break;
    }

    u8g2.drawHLine(2, 45, 60);

    // 2. Status de Comunicação
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.setCursor(4, 55);
    if (commActive) {
      u8g2.print(F("COM: OK"));
      if ((animTick / 4) % 2 == 0) {
        u8g2.drawDisc(55, 52, 2);
      }
    } else {
      u8g2.print(F("COM: OFFLINE"));
    }

    // Expressão
    u8g2.setCursor(4, 68);
    u8g2.print(F("MODO:"));
    u8g2.setCursor(32, 68);
    switch (currentExpression) {
      case EXP_NORMAL:  u8g2.print(F("NORM")); break;
      case EXP_CURIOSO: u8g2.print(F("CURI")); break;
      case EXP_PISCAR:  u8g2.print(F("PISC")); break;
      case EXP_TRISTE:  u8g2.print(F("TRIS")); break;
      case EXP_BRAVO:   u8g2.print(F("BRAV")); break;
      case EXP_FELIZ:   u8g2.print(F("FELZ")); break;
      case EXP_AMOR:    u8g2.print(F("AMOR")); break;
    }

    u8g2.drawHLine(2, 74, 60);

    // 3. Motores
    u8g2.setCursor(4, 85);
    u8g2.print(F("MOT:"));
    u8g2.setCursor(30, 85);
    if (currentSpeedL == 0 && currentSpeedR == 0) {
      u8g2.print(F("[STOP]"));
    } else if (currentSpeedL > 0 && currentSpeedR > 0) {
      u8g2.print(F("^ FRENTE"));
    } else if (currentSpeedL < 0 && currentSpeedR < 0) {
      u8g2.print(F("v RE"));
    } else if (currentSpeedL < currentSpeedR) {
      u8g2.print(F("< GIRO"));
    } else {
      u8g2.print(F("> GIRO"));
    }

    int barL = map(abs(currentSpeedL), 0, 255, 0, 26);
    int barR = map(abs(currentSpeedR), 0, 255, 0, 26);
    u8g2.drawFrame(4, 90, 28, 4);
    u8g2.drawBox(5, 91, barL, 2);
    u8g2.drawFrame(34, 90, 28, 4);
    u8g2.drawBox(35, 91, barR, 2);

    u8g2.drawHLine(2, 98, 60);

    // 4. Bateria
    u8g2.drawFrame(4, 104, 16, 9);
    u8g2.drawBox(20, 106, 2, 5);

    int fillW = map(constrain((int)(currentBatteryVoltage * 10), 62, 82), 62, 82, 0, 12);
    if (fillW > 0) {
      u8g2.drawBox(6, 106, fillW, 5);
    }

    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setCursor(26, 112);
    u8g2.print(currentBatteryVoltage, 1);
    u8g2.print(F("V"));

    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.setCursor(4, 124);
    u8g2.print(F("WALL-E NANO SHIELD"));

  } while (u8g2.nextPage());

  animTick++;
}

// =============================================================================
// TELEMETRIA DE VOLTAGEM (A0)
// =============================================================================

void checkBattery() {
  int raw = analogRead(PIN_BATTERY);
  float measured = (raw * 5.0f / 1023.0f) * BATTERY_DIVIDER_RATIO;
  if (measured < 0.5f) measured = 0.0f;

  if (currentBatteryVoltage <= 0.1f) {
    currentBatteryVoltage = measured;
  } else {
    currentBatteryVoltage = (currentBatteryVoltage * 0.85f) + (measured * 0.15f);
  }

  Serial.print(F("BAT:"));
  Serial.println(currentBatteryVoltage, 2);
}

// =============================================================================
// PARSER SERIAL (COMANDOS RECEBIDOS DO RASPBERRY PI)
// =============================================================================

void processCommand(char *cmd) {
  if (cmd == nullptr || cmd[0] == '\0') return;

  lastPacketReceivedTime = millis();
  digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));

  // 1. Motores: "M:<esq>,<dir>"
  if (cmd[0] == 'M' && cmd[1] == ':') {
    char *comma = strchr(cmd + 2, ',');
    if (comma != nullptr) {
      *comma = '\0';
      targetSpeedL = constrain(atoi(cmd + 2), -255, 255);
      targetSpeedR = constrain(atoi(comma + 1), -255, 255);
      lastDriveCommandTime = millis();
    }
  }
  // 2. Servos: "S:<id>,<ang>"
  else if (cmd[0] == 'S' && cmd[1] == ':') {
    char *comma = strchr(cmd + 2, ',');
    if (comma != nullptr) {
      *comma = '\0';
      int id = atoi(cmd + 2);
      int ang = atoi(comma + 1);
      if (id >= 0 && id < SERVO_COUNT) {
        targetServoAngle[id] = constrain(ang, 0, 180);
      }
    }
  }
  // 3. Expressão OLED: "E:<id>"
  else if (cmd[0] == 'E' && cmd[1] == ':') {
    int expId = atoi(cmd + 2);
    if (expId >= 0 && expId <= 6) {
      currentExpression = (Expression)expId;
      expressionStartTime = millis();
    }
  }
  // 4. Velocidade de interpolação dos servos: "V:<val>" (decimos de grau por ciclo)
  else if (cmd[0] == 'V' && cmd[1] == ':') {
    int spd = constrain(atoi(cmd + 2), 2, 80);
    servoStepPerCycle = spd / 10.0f;
  }
  // 5. Parada de Emergência: "STOP"
  else if (strcmp(cmd, "STOP") == 0) {
    emergencyStop();
  }
  // 6. Ping / Heartbeat
  else if (strcmp(cmd, "PING") == 0) {
    Serial.println(F("PONG"));
  }
}

void readSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (hwRxIdx > 0) {
        hwRxBuffer[hwRxIdx] = '\0';
        processCommand(hwRxBuffer);
        hwRxIdx = 0;
      }
    } else {
      if (c >= 32 && c <= 126) {
        if (hwRxIdx < sizeof(hwRxBuffer) - 1) {
          hwRxBuffer[hwRxIdx++] = c;
        } else {
          hwRxIdx = 0;
        }
      }
    }
  }
}

// =============================================================================
// SETUP E LOOP PRINCIPAL
// =============================================================================

void setup() {
  // Motores em LOW por segurança
  pinMode(PIN_M_L1, OUTPUT);
  pinMode(PIN_M_L2, OUTPUT);
  pinMode(PIN_M_R1, OUTPUT);
  pinMode(PIN_M_R2, OUTPUT);
  emergencyStop();

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, LOW);

  // Serial com Raspberry Pi a 38400 baud
  Serial.begin(38400);

  // Inicializa os 7 servomotores no Nano Shield
  for (uint8_t i = 0; i < SERVO_COUNT; i++) {
    servos[i].attach(SERVO_PINS[i]);
    servos[i].write(90);
    currentServoAngle[i] = 90.0f;
    targetServoAngle[i] = 90;
  }

  // Inicializa o barramento I2C a 100kHz estável
  Wire.begin();
  Wire.setClock(100000L);

  // Inicializa Display OLED 0.96" SSD1306
  u8g2.begin();
  renderVerticalOled();

  // Leitura inicial de bateria
  checkBattery();
}

void loop() {
  const uint32_t now = millis();

  // 1. Processa comandos seriais do Raspberry Pi
  readSerialCommands();

  // 2. Atualiza motores de tração a cada 15ms
  if (now - lastMotorUpdate >= 15) {
    lastMotorUpdate = now;
    updateMotors();
  }

  // 3. Atualiza os 7 servomotores com rampa suave a cada 20ms
  if (now - lastServoUpdate >= 20) {
    lastServoUpdate = now;
    updateServos();
  }

  // 4. Animação de piscada casual nos olhos
  if (currentExpression == EXP_NORMAL) {
    if (now - lastAutoBlinkTime >= nextBlinkInterval) {
      currentExpression = EXP_PISCAR;
      expressionStartTime = now;
      lastAutoBlinkTime = now;
      nextBlinkInterval = random(3500, 6500);
    }
  } else if (currentExpression == EXP_PISCAR) {
    if (now - expressionStartTime >= 160) {
      currentExpression = EXP_NORMAL;
    }
  }

  // 5. Atualiza tela OLED a cada 60ms (~16 FPS)
  if (now - lastOledUpdate >= 60) {
    lastOledUpdate = now;
    renderVerticalOled();
  }

  // 6. Telemetria da Bateria a cada 1000ms
  if (now - lastBatteryTelemetry >= 1000) {
    lastBatteryTelemetry = now;
    checkBattery();
  }
}
