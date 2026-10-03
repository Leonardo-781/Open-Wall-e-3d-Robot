// ============================================================
// Wall-E 3D Robot - ESP32 COMPLETE VERSION
// Integração completa: Servidor Web + Controle de Motores + Servos
// Recursos: 7 Servos + Auto-falante + Display OLED 0.96"
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// CONFIGURAÇÃO WIFI
// ============================================================
const char* ssid = "ROBO-CTRL";
const char* password = "12345678";

WebServer server(80);

// ============================================================
// DEFINIÇÃO DE PINOS - ESP32
// ============================================================

// Servos (7 servos usando PWM do ESP32)
const int SERVO_PINS[7] = {
  13,  // Servo 0: Braço Esquerdo
  12,  // Servo 1: Braço Direito
  14,  // Servo 2: Pescoço Base (Rotação)
  27,  // Servo 3: Pescoço Inclinação
  26,  // Servo 4: Rotação da Cabeça
  25,  // Servo 5: Olho Esquerdo
  33   // Servo 6: Olho Direito
};

// Controle de Motores - Ponte H L298N
// Motor Esquerdo
const int LEFT_MOTOR_PIN1 = 19;   // IN1
const int LEFT_MOTOR_PIN2 = 18;   // IN2
const int LEFT_MOTOR_PWM = 5;     // ENA (PWM)

// Motor Direito
const int RIGHT_MOTOR_PIN1 = 17;  // IN3
const int RIGHT_MOTOR_PIN2 = 16;  // IN4
const int RIGHT_MOTOR_PWM = 4;    // ENB (PWM)

// Auto-falante e LED de Status
const int SPEAKER_PIN = 23;       // Auto-falante (saída PWM para sons)
const int STATUS_LED = 2;         // LED onboard do ESP32

// Display OLED I2C (SSD1306 0.96")
const int OLED_SDA = 21;          // GPIO 21 (I2C SDA)
const int OLED_SCL = 22;          // GPIO 22 (I2C SCL)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1                // Reset pin (não usado)
#define SCREEN_ADDRESS 0x3C          // Endereço I2C padrão do OLED

// ============================================================
// VARIÁVEIS GLOBAIS
// ============================================================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Servo servos[7];
int servoAngles[7] = {90, 90, 90, 90, 90, 90, 90};
int motorLeft = 0;
int motorRight = 0;
int speedMultiplier = 100;
bool turboMode = false;
bool emergencyStop = false;
int batteryLevel = 100;
unsigned long lastCommandTime = 0;
const unsigned long COMMAND_TIMEOUT = 2000;

// Configuração PWM para motores
const int PWM_FREQ = 1000;       // Frequência 1kHz
const int PWM_RESOLUTION = 8;    // Resolução de 8 bits (0-255)
const int LEFT_PWM_CHANNEL = 0;
const int RIGHT_PWM_CHANNEL = 1;

// ============================================================
// HTML DA INTERFACE WEB
// ============================================================

const char HTML[] PROGMEM = R"====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Wall-E 3D - Controle Avancado</title>
<style>
* {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background: linear-gradient(135deg, #2a2a1f 0%, #1a1a0f 100%);
  color: #FFE680;
  text-align: center;
  padding: 10px;
  min-height: 100vh;
}

h1 {
  margin: 15px 0;
  text-shadow: 0 0 15px #FFD700, 0 0 30px #D4860C;
  font-size: 28px;
}

h3 {
  margin: 10px 0 15px 0;
  font-size: 18px;
}

.container {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 15px;
  max-width: 1200px;
  margin: 0 auto;
}

.card {
  background: linear-gradient(145deg, #3d3d2d, #4a4a3a);
  border: 3px solid #FFD700;
  border-radius: 12px;
  padding: 15px;
  min-width: 280px;
  box-shadow: 0 4px 15px rgba(212, 134, 12, 0.4), inset 0 0 10px rgba(255, 215, 0, 0.1);
  transition: all 0.3s ease;
}

.card:hover {
  box-shadow: 0 6px 20px rgba(212, 134, 12, 0.6), inset 0 0 15px rgba(255, 215, 0, 0.15);
  transform: translateY(-2px);
  border-color: #FFE680;
}

.joystick-container {
  display: flex;
  flex-direction: column;
  align-items: center;
}

#joystick {
  width: 200px;
  height: 200px;
  background: radial-gradient(circle, #3d3d2d, #2a2a1f);
  border: 3px solid #FFD700;
  border-radius: 50%;
  position: relative;
  touch-action: none;
  cursor: grab;
  margin-bottom: 10px;
  box-shadow: inset 0 0 20px rgba(0, 0, 0, 0.5), 0 0 15px rgba(255, 215, 0, 0.3);
}

#joystick.active {
  cursor: grabbing;
}

#stick {
  width: 50px;
  height: 50px;
  background: radial-gradient(circle at 30% 30%, #FFD700, #CCAA00);
  border-radius: 50%;
  position: absolute;
  left: 75px;
  top: 75px;
  box-shadow: 0 0 10px #FFD700;
  transition: all 0.05s linear;
}

.slider-group {
  margin: 12px 0;
  text-align: left;
}

.servo-label {
  display: flex;
  justify-content: space-between;
  font-size: 12px;
  margin-bottom: 5px;
  color: #FFE680;
}

.servo-value {
  font-weight: bold;
  color: #FFD700;
}

input[type="range"] {
  width: 100%;
  height: 6px;
  border-radius: 3px;
  background: linear-gradient(to right, #2a2a1f, #FFD700);
  outline: none;
  -webkit-appearance: none;
  appearance: none;
  box-shadow: inset 0 2px 4px rgba(0, 0, 0, 0.5);
}

input[type="range"]::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 18px;
  height: 18px;
  border-radius: 50%;
  background: linear-gradient(135deg, #FFD700, #CCAA00);
  cursor: pointer;
  box-shadow: 0 0 8px #FFD700, inset 0 0 3px rgba(255, 255, 255, 0.5);
  border: 2px solid #D4860C;
}

input[type="range"]::-moz-range-thumb {
  width: 18px;
  height: 18px;
  border-radius: 50%;
  background: linear-gradient(135deg, #FFD700, #CCAA00);
  cursor: pointer;
  border: 2px solid #D4860C;
  box-shadow: 0 0 8px #FFD700, inset 0 0 3px rgba(255, 255, 255, 0.5);
}

.button-group {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
  margin: 12px 0;
}

button {
  padding: 10px 15px;
  border: 2px solid #FFD700;
  background: linear-gradient(145deg, #3d3d2d, #2a2a1f);
  color: #FFE680;
  border-radius: 6px;
  font-weight: bold;
  cursor: pointer;
  font-size: 12px;
  transition: all 0.3s ease;
  text-transform: uppercase;
  box-shadow: 0 4px 10px rgba(212, 134, 12, 0.3), inset 0 1px 0 rgba(255, 215, 0, 0.2);
}

button:hover {
  background: linear-gradient(145deg, #FFD700, #CCAA00);
  color: #2a2a1f;
  transform: scale(1.05);
  box-shadow: 0 6px 15px rgba(255, 215, 0, 0.5), inset 0 1px 0 rgba(255, 255, 255, 0.3);
}

button:active {
  transform: scale(0.95);
}

.emergency-button {
  grid-column: 1 / -1;
  background: #8b0000;
  border-color: #ff0000;
  color: #ff0000;
}

.emergency-button:hover {
  background: #ff0000;
  color: #8b0000;
}

.mode-toggle {
  display: flex;
  gap: 8px;
  justify-content: center;
  margin: 12px 0;
}

.mode-btn {
  flex: 1;
  padding: 8px 12px;
  border: 2px solid #FFD700;
  background: linear-gradient(145deg, #3d3d2d, #2a2a1f);
  color: #FFE680;
  border-radius: 6px;
  cursor: pointer;
  font-weight: bold;
  font-size: 11px;
  transition: all 0.3s ease;
  box-shadow: 0 2px 8px rgba(212, 134, 12, 0.2);
}

.mode-btn.active {
  background: linear-gradient(145deg, #FFD700, #CCAA00);
  color: #2a2a1f;
  box-shadow: 0 4px 12px rgba(255, 215, 0, 0.6), inset 0 1px 0 rgba(255, 255, 255, 0.3);
}

.status-bar {
  background: linear-gradient(145deg, #2a2a1f, #3d3d2d);
  border: 2px solid #FFD700;
  border-radius: 8px;
  padding: 12px;
  margin-top: 15px;
  font-size: 12px;
  font-family: 'Courier New', monospace;
  box-shadow: inset 0 2px 8px rgba(0, 0, 0, 0.5), 0 0 10px rgba(255, 215, 0, 0.2);
}

.status-item {
  display: flex;
  justify-content: space-between;
  margin: 5px 0;
  color: #FFE680;
}

.status-value {
  color: #FFD700;
  font-weight: bold;
}
</style>
</head>
<body>

<h1>== Painel de Controle Wall-E ==</h1>

<div class="container">

  <div class="card">
    <h3>[ MOVIMENTO ]</h3>
    <div class="joystick-container">
      <div id="joystick">
        <div id="stick"></div>
      </div>
      <small>Esquerda/Direita: Rotacao | Para Cima/Baixo: Velocidade</small>
    </div>
  </div>

  <div class="card">
    <h3>[ SERVOS ]</h3>
    
    <div class="slider-group">
      <div class="servo-label">
        <span>Braco Esquerdo</span>
        <span class="servo-value" id="val0">90°</span>
      </div>
      <input type="range" min="0" max="180" value="90" oninput="updateServo(0, this.value)">
    </div>

    <div class="slider-group">
      <div class="servo-label">
        <span>Braco Direito</span>
        <span class="servo-value" id="val1">90°</span>
      </div>
      <input type="range" min="0" max="180" value="90" oninput="updateServo(1, this.value)">
    </div>

    <div class="slider-group">
      <div class="servo-label">
        <span>Pescoco Giratorio</span>
        <span class="servo-value" id="val2">90°</span>
      </div>
      <input type="range" min="0" max="180" value="90" oninput="updateServo(2, this.value)">
    </div>

    <div class="slider-group">
      <div class="servo-label">
        <span>Inclinacao do Pescoco</span>
        <span class="servo-value" id="val3">90°</span>
      </div>
      <input type="range" min="0" max="180" value="90" oninput="updateServo(3, this.value)">
    </div>

    <div class="slider-group">
      <div class="servo-label">
        <span>Rotacao da Cabeca</span>
        <span class="servo-value" id="val4">90°</span>
      </div>
      <input type="range" min="0" max="180" value="90" oninput="updateServo(4, this.value)">
    </div>

    <div class="button-group" style="grid-column: 1 / -1;">
      <button onclick="resetAllServos()" style="grid-column: 1 / -1;">RESETAR SERVOS</button>
    </div>

    <div class="slider-group">
      <div class="servo-label">
        <span>Olho Esquerdo</span>
        <span class="servo-value" id="val5">90°</span>
      </div>
      <input type="range" min="0" max="180" value="90" oninput="updateServo(5, this.value)">
    </div>

    <div class="slider-group">
      <div class="servo-label">
        <span>Olho Direito</span>
        <span class="servo-value" id="val6">90°</span>
      </div>
      <input type="range" min="0" max="180" value="90" oninput="updateServo(6, this.value)">
    </div>

  </div>

  <div class="card">
    <h3>[ VELOCIDADE ]</h3>
    
    <div class="slider-group">
      <div class="servo-label">
        <span>Velocidade dos Motores</span>
        <span class="servo-value" id="speedVal">100%</span>
      </div>
      <input type="range" min="30" max="255" value="100" oninput="updateSpeed(this.value)">
    </div>

    <div class="mode-toggle">
      <button class="mode-btn active" onclick="setMode('normal')">Normal</button>
      <button class="mode-btn" onclick="setMode('turbo')">Turbo</button>
    </div>
  </div>

  <div class="card">
    <h3>[ ACOES RAPIDAS ]</h3>
    
    <div class="button-group">
      <button onclick="executeAction(1)">Acenar</button>
      <button onclick="executeAction(2)">Girar</button>
      <button onclick="executeAction(3)">Bip</button>
      <button onclick="executeAction(4)">Musica</button>
    </div>

    <button class="emergency-button" id="eStopBtn" onclick="toggleEmergencyStop()">PARADA EMERGENCIA</button>
  </div>

  <div class="card" style="grid-column: 1 / -1; min-width: 280px;">
    <h3>[ STATUS ]</h3>
    <div class="status-bar">
      <div class="status-item">
        <span>Motor E.:</span>
        <span class="status-value" id="statusMotorL">0 PWM</span>
      </div>
      <div class="status-item">
        <span>Motor D.:</span>
        <span class="status-value" id="statusMotorR">0 PWM</span>
      </div>
      <div class="status-item">
        <span>Modo Velocidade:</span>
        <span class="status-value" id="statusMode">Normal</span>
      </div>
      <div class="status-item">
        <span>Bateria:</span>
        <span class="status-value" id="statusBatt">100%</span>
      </div>
      <div class="status-item">
        <span>Status:</span>
        <span class="status-value" id="statusMsg">Pronto</span>
      </div>
      <div class="status-item">
        <span>Conexao:</span>
        <span class="status-value" id="statusConnection" style="color: #00FF00;">● CONECTADO</span>
      </div>
    </div>
  </div>

</div>

<script>
let joy = document.getElementById('joystick');
let stick = document.getElementById('stick');
let rect = joy.getBoundingClientRect();
let center = 100;
let maxRadius = 70;
let isActive = false;
let currentSpeed = 100;
let isTurbo = false;
let isEmergency = false;
let connectionStatus = true;
let lastResponseTime = Date.now();

joy.addEventListener('pointerdown', (e) => {
  isActive = true;
  joy.classList.add('active');
  rect = joy.getBoundingClientRect();
});

document.addEventListener('pointerup', (e) => {
  isActive = false;
  joy.classList.remove('active');
  stick.style.left = '75px';
  stick.style.top = '75px';
  sendMotor(0, 0);
});

document.addEventListener('pointermove', (e) => {
  if (!isActive) return;

  let x = e.clientX - rect.left - center;
  let y = e.clientY - rect.top - center;

  let distance = Math.hypot(x, y);
  if (distance > maxRadius) {
    x = (x / distance) * maxRadius;
    y = (y / distance) * maxRadius;
  }

  stick.style.left = (x + center - 25) + 'px';
  stick.style.top = (y + center - 25) + 'px';

  let normX = x / maxRadius * 255;
  let normY = -y / maxRadius * 255;

  sendMotor(normX, normY);
});

function updateServo(id, value) {
  document.getElementById('val' + id).textContent = value + '°';
  fetch(`/servo?id=${id}&ang=${value}`);
}

function resetAllServos() {
  let sliders = document.querySelectorAll('.card:nth-child(2) input[type="range"]');
  for (let i = 0; i < sliders.length; i++) {
    sliders[i].value = 90;
    updateServo(i, 90);
  }
  document.getElementById('statusMsg').textContent = 'Servos resetados';
}

function sendMotor(x, y) {
  let speedFactor = isTurbo ? 255 : currentSpeed;
  let left = y + x;
  let right = y - x;

  left = Math.max(-255, Math.min(255, left));
  right = Math.max(-255, Math.min(255, right));

  left = Math.round(left * speedFactor / 255);
  right = Math.round(right * speedFactor / 255);

  document.getElementById('statusMotorL').textContent = left + ' PWM';
  document.getElementById('statusMotorR').textContent = right + ' PWM';

  if (!isEmergency && connectionStatus) {
    fetch(`/motor?l=${left}&r=${right}`);
  }
}

function updateSpeed(value) {
  currentSpeed = parseInt(value);
  document.getElementById('speedVal').textContent = currentSpeed + '%';
  fetch(`/speed?val=${value}`);
}

function setMode(mode) {
  if (mode === 'turbo') {
    isTurbo = true;
    document.querySelectorAll('.mode-btn')[0].classList.remove('active');
    document.querySelectorAll('.mode-btn')[1].classList.add('active');
    document.getElementById('statusMode').textContent = 'Turbo';
  } else {
    isTurbo = false;
    document.querySelectorAll('.mode-btn')[0].classList.add('active');
    document.querySelectorAll('.mode-btn')[1].classList.remove('active');
    document.getElementById('statusMode').textContent = 'Normal';
  }
  fetch(`/modo?m=${mode}`);
}

function executeAction(action) {
  fetch(`/acao?a=${action}`);
}

function toggleEmergencyStop() {
  isEmergency = !isEmergency;
  let btn = document.getElementById('eStopBtn');
  
  if (isEmergency) {
    btn.classList.remove('disabled');
    document.getElementById('statusMsg').textContent = '🛑 PARADO';
    fetch(`/stop`);
  } else {
    btn.classList.add('disabled');
    document.getElementById('statusMsg').textContent = 'Pronto';
  }
}

function sendHeartbeat() {
  fetch(`/ping`)
    .then(r => r.json())
    .then(data => {
      lastResponseTime = Date.now();
      if (!connectionStatus) {
        connectionStatus = true;
        document.getElementById('statusConnection').textContent = '● CONECTADO';
        document.getElementById('statusConnection').style.color = '#00FF00';
      }
    })
    .catch(e => {
      checkConnectionStatus();
    });
}

function checkConnectionStatus() {
  let timeSinceResponse = Date.now() - lastResponseTime;
  
  if (timeSinceResponse > 2000) {
    if (connectionStatus) {
      connectionStatus = false;
      document.getElementById('statusConnection').textContent = '● DESCONECTADO';
      document.getElementById('statusConnection').style.color = '#FF0000';
      document.getElementById('statusMsg').textContent = '🚫 SEM SINAL';
      fetch(`/stop`);
    }
  }
}

setInterval(() => {
  sendHeartbeat();
}, 500);

setInterval(() => {
  checkConnectionStatus();
}, 100);

setInterval(() => {
  fetch(`/status`)
    .then(r => r.json())
    .then(data => {
      if (data.battery) document.getElementById('statusBatt').textContent = data.battery + '%';
    })
    .catch(e => {});
}, 2000);
</script>

</body>
</html>
)====";

// ============================================================
// CONFIGURAÇÃO INICIAL
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Wall-E 3D Robot - ESP32 Complete Starting ===");
  
  // Configurar pinos de motores
  pinMode(LEFT_MOTOR_PIN1, OUTPUT);
  pinMode(LEFT_MOTOR_PIN2, OUTPUT);
  pinMode(RIGHT_MOTOR_PIN1, OUTPUT);
  pinMode(RIGHT_MOTOR_PIN2, OUTPUT);
  
  // Configurar canais PWM para motores
  ledcSetup(LEFT_PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(RIGHT_PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(LEFT_MOTOR_PWM, LEFT_PWM_CHANNEL);
  ledcAttachPin(RIGHT_MOTOR_PWM, RIGHT_PWM_CHANNEL);
  
  // Configurar auto-falante e LED
  pinMode(SPEAKER_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);
  
  // Inicializar display OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Falha ao inicializar OLED"));
  } else {
    Serial.println(F("Display OLED inicializado com sucesso"));
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Wall-E 3D Robot");
    display.println("ESP32 Complete");
    display.println("");
    display.println("Inicializando...");
    display.display();
  }
  
  // Inicializar servos
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  for (int i = 0; i < 7; i++) {
    servos[i].setPeriodHertz(50);  // Frequência padrão de servos: 50Hz
    servos[i].attach(SERVO_PINS[i], 500, 2400);  // Min/Max pulse width
    servos[i].write(90);  // Posição central
    Serial.print("Servo ");
    Serial.print(i);
    Serial.print(" attached to GPIO ");
    Serial.println(SERVO_PINS[i]);
  }
  
  // Inicializar motores parados
  stopAllMotors();
  
  // Configurar WiFi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  
  Serial.println("WiFi AP started!");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(IP);
  
  // Configurar rotas do servidor web
  setupWebServer();
  
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("=== System Ready ===\n");
  
  lastCommandTime = millis();
  
  // Som de inicialização no auto-falante
  tone(SPEAKER_PIN, 1000, 100);
  delay(150);
  tone(SPEAKER_PIN, 1500, 100);
  delay(150);
  
  // Atualizar display OLED com informações
  delay(1000);
  updateOLEDDisplay();
}

// ============================================================
// CONFIGURAÇÃO DO SERVIDOR WEB
// ============================================================

void setupWebServer() {
  // Página principal
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", HTML);
  });
  
  // Controle de servos
  server.on("/servo", HTTP_GET, handleServo);
  
  // Controle de motores
  server.on("/motor", HTTP_GET, handleMotor);
  
  // Controle de velocidade
  server.on("/speed", HTTP_GET, handleSpeed);
  
  // Modo turbo/normal
  server.on("/modo", HTTP_GET, handleMode);
  
  // Ações rápidas
  server.on("/acao", HTTP_GET, handleAction);
  
  // Parada de emergência
  server.on("/stop", HTTP_GET, handleEmergencyStop);
  
  // Ping/heartbeat
  server.on("/ping", HTTP_GET, handlePing);
  
  // Status
  server.on("/status", HTTP_GET, handleStatus);
}

// ============================================================
// HANDLERS DO SERVIDOR WEB
// ============================================================

void handleServo() {
  if (server.hasArg("id") && server.hasArg("ang")) {
    int id = server.arg("id").toInt();
    int angle = server.arg("ang").toInt();
    
    if (id >= 0 && id < 7 && angle >= 0 && angle <= 180) {
      servoAngles[id] = angle;
      if (!emergencyStop) {
        servos[id].write(angle);
        Serial.print("Servo ");
        Serial.print(id);
        Serial.print(" -> ");
        Serial.print(angle);
        Serial.println("°");
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleMotor() {
  if (server.hasArg("l") && server.hasArg("r")) {
    int left = server.arg("l").toInt();
    int right = server.arg("r").toInt();
    
    motorLeft = constrain(left, -255, 255);
    motorRight = constrain(right, -255, 255);
    
    if (!emergencyStop) {
      setMotorSpeed(motorLeft, motorRight);
      lastCommandTime = millis();
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleSpeed() {
  if (server.hasArg("val")) {
    speedMultiplier = constrain(server.arg("val").toInt(), 30, 255);
    Serial.print("Speed multiplier: ");
    Serial.print(speedMultiplier);
    Serial.println("%");
  }
  server.send(200, "text/plain", "OK");
}

void handleMode() {
  if (server.hasArg("m")) {
    String mode = server.arg("m");
    turboMode = (mode == "turbo");
    Serial.print("Mode: ");
    Serial.println(turboMode ? "TURBO" : "NORMAL");
  }
  server.send(200, "text/plain", "OK");
}

void handleAction() {
  if (server.hasArg("a")) {
    int action = server.arg("a").toInt();
    if (action >= 1 && action <= 4 && !emergencyStop) {
      executeAction(action);
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleEmergencyStop() {
  emergencyStop = !emergencyStop;
  
  if (emergencyStop) {
    Serial.println("🛑 EMERGENCY STOP ACTIVATED!");
    stopAllMotors();
    
    // Alert sound no auto-falante
    for (int i = 0; i < 5; i++) {
      digitalWrite(STATUS_LED, HIGH);
      tone(SPEAKER_PIN, 2000, 100);
      delay(150);
      digitalWrite(STATUS_LED, LOW);
      delay(150);
    }
    updateOLEDDisplay();
  } else {
    Serial.println("Emergency stop released");
    digitalWrite(STATUS_LED, LOW);
    updateOLEDDisplay();
  }
  
  server.send(200, "text/plain", emergencyStop ? "STOPPED" : "RUNNING");
}

void handlePing() {
  lastCommandTime = millis();
  String json = "{\"ok\":true}";
  server.send(200, "application/json", json);
}

void handleStatus() {
  String json = "{";
  json += "\"battery\":\"" + String(batteryLevel) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ============================================================
// CONTROLE DE MOTORES
// ============================================================

void setMotorSpeed(int leftSpeed, int rightSpeed) {
  // Aplicar multiplicador de velocidade
  leftSpeed = (leftSpeed * speedMultiplier) / 100;
  rightSpeed = (rightSpeed * speedMultiplier) / 100;
  
  // Limitar a faixa PWM válida
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);
  
  // MOTOR ESQUERDO
  if (leftSpeed > 0) {
    // Frente
    digitalWrite(LEFT_MOTOR_PIN1, HIGH);
    digitalWrite(LEFT_MOTOR_PIN2, LOW);
    ledcWrite(LEFT_PWM_CHANNEL, leftSpeed);
  } else if (leftSpeed < 0) {
    // Trás
    digitalWrite(LEFT_MOTOR_PIN1, LOW);
    digitalWrite(LEFT_MOTOR_PIN2, HIGH);
    ledcWrite(LEFT_PWM_CHANNEL, -leftSpeed);
  } else {
    // Parado
    digitalWrite(LEFT_MOTOR_PIN1, LOW);
    digitalWrite(LEFT_MOTOR_PIN2, LOW);
    ledcWrite(LEFT_PWM_CHANNEL, 0);
  }
  
  // MOTOR DIREITO
  if (rightSpeed > 0) {
    // Frente
    digitalWrite(RIGHT_MOTOR_PIN1, HIGH);
    digitalWrite(RIGHT_MOTOR_PIN2, LOW);
    ledcWrite(RIGHT_PWM_CHANNEL, rightSpeed);
  } else if (rightSpeed < 0) {
    // Trás
    digitalWrite(RIGHT_MOTOR_PIN1, LOW);
    digitalWrite(RIGHT_MOTOR_PIN2, HIGH);
    ledcWrite(RIGHT_PWM_CHANNEL, -rightSpeed);
  } else {
    // Parado
    digitalWrite(RIGHT_MOTOR_PIN1, LOW);
    digitalWrite(RIGHT_MOTOR_PIN2, LOW);
    ledcWrite(RIGHT_PWM_CHANNEL, 0);
  }
}

void stopAllMotors() {
  setMotorSpeed(0, 0);
  Serial.println("All motors stopped");
}

// ============================================================
// AÇÕES AUTOMATIZADAS
// ============================================================

void executeAction(int action) {
  switch (action) {
    case 1:
      waveAction();
      break;
    case 2:
      spinAction();
      break;
    case 3:
      beepAction();
      break;
    case 4:
      musicAction();
      break;
  }
}

void waveAction() {
  Serial.println("Action: WAVE");
  
  // Acenar com braço esquerdo 3 vezes
  for (int i = 0; i < 3; i++) {
    for (int angle = 90; angle <= 180; angle += 10) {
      servos[0].write(angle);
      delay(20);
    }
    for (int angle = 180; angle >= 90; angle -= 10) {
      servos[0].write(angle);
      delay(20);
    }
  }
}

void spinAction() {
  Serial.println("Action: SPIN");
  
  // Girar 360 graus
  setMotorSpeed(150, -150);
  delay(1500);
  setMotorSpeed(0, 0);
}

void beepAction() {
  Serial.println("Action: BEEP");
  
  // 3 beeps no auto-falante
  for (int i = 0; i < 3; i++) {
    tone(SPEAKER_PIN, 1000, 200);
    delay(300);
  }
}

void musicAction() {
  Serial.println("Action: MUSIC");
  
  // Melodia simples (C D E F G A B C) no auto-falante
  int melody[] = {262, 294, 330, 349, 392, 440, 494, 523};
  int noteDuration = 200;
  
  for (int i = 0; i < 8; i++) {
    tone(SPEAKER_PIN, melody[i], noteDuration);
    delay(noteDuration + 50);
  }
}

// ============================================================
// ATUALIZAÇÃO DO DISPLAY OLED
// ============================================================

void updateOLEDDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Título
  display.setCursor(0, 0);
  display.println("=== WALL-E 3D ===");
  
  // Status de conexão WiFi
  display.setCursor(0, 12);
  int clients = WiFi.softAPgetStationNum();
  display.print("WiFi: ");
  display.print(clients);
  display.println(" client(s)");
  
  // Motores
  display.setCursor(0, 24);
  display.print("L:");
  display.print(motorLeft);
  display.print(" R:");
  display.println(motorRight);
  
  // Modo e velocidade
  display.setCursor(0, 36);
  display.print("Modo: ");
  display.println(turboMode ? "TURBO" : "Normal");
  
  display.setCursor(0, 48);
  display.print("Speed: ");
  display.print(speedMultiplier);
  display.println("%");
  
  // Status de emergência
  if (emergencyStop) {
    display.setCursor(0, 56);
    display.setTextSize(1);
    display.println("! EMERGENCY STOP !");
  }
  
  display.display();
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================

void loop() {
  server.handleClient();
  
  // Watchdog: parar motores se sem comando por muito tempo
  if (!emergencyStop && (millis() - lastCommandTime) > COMMAND_TIMEOUT) {
    static bool timeoutWarning = false;
    if (!timeoutWarning) {
      Serial.println("⚠️  No commands received - safety timeout");
      stopAllMotors();
      updateOLEDDisplay();
      timeoutWarning = true;
    }
  }
  
  // Atualizar display OLED a cada 500ms
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 500) {
    updateOLEDDisplay();
    lastDisplayUpdate = millis();
  }
  
  delay(10);
}
