#include <WiFi.h>
#include <WebServer.h>

// WiFi Configuration
const char* ssid = "ROBO-CTRL";
const char* password = "12345678";

WebServer server(80);
HardwareSerial RoboSerial(1);

// Global control variables
int servoAngles[8] = {90, 90, 90, 90, 90, 90, 90, 90};
int motorLeft = 0, motorRight = 0;
bool turboMode = false;
int speedMultiplier = 100;
int batteryLevel = 100;
bool emergencyStop = false;

// Connection monitoring variables
bool connectionActive = false;
unsigned long lastResponseTime = 0;
const unsigned long WATCHDOG_TIMEOUT = 2000; // 2 seconds
unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL = 500; // 500ms heartbeat

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

    <div class="slider-group">
      <div class="servo-label">
        <span>Mandibula</span>
        <span class="servo-value" id="val7">90°</span>
      </div>
      <input type="range" min="0" max="180" value="90" oninput="updateServo(7, this.value)">
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
  // Get all range sliders in the SERVOS card
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
      // Send emergency stop
      fetch(`/stop`);
    }
  }
}

// Heartbeat sender - sends ping every 500ms
setInterval(() => {
  sendHeartbeat();
}, 500);

// Watchdog timer - checks connection status every 100ms
setInterval(() => {
  checkConnectionStatus();
}, 100);

// Status update every 2 seconds
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

void setup() {
  RoboSerial.begin(115200, SERIAL_8N1, 20, 21);
  delay(1000);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.begin(115200);
  Serial.println("WiFi AP started at ");
  Serial.println(IP);

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", HTML);
  });

  server.on("/servo", HTTP_GET, []() {
    if (server.hasArg("id") && server.hasArg("ang")) {
      int id = server.arg("id").toInt();
      int angle = server.arg("ang").toInt();
      if (id >= 0 && id < 8 && angle >= 0 && angle <= 180) {
        servoAngles[id] = angle;
        if (!emergencyStop) {
          RoboSerial.printf("S,%d,%d\n", id, angle);
        }
      }
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/motor", HTTP_GET, []() {
    if (server.hasArg("l") && server.hasArg("r")) {
      int left = server.arg("l").toInt();
      int right = server.arg("r").toInt();
      motorLeft = constrain(left, -255, 255);
      motorRight = constrain(right, -255, 255);
      if (!emergencyStop) {
        RoboSerial.printf("M,%d,%d\n", motorLeft, motorRight);
      }
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/speed", HTTP_GET, []() {
    if (server.hasArg("val")) {
      speedMultiplier = constrain(server.arg("val").toInt(), 30, 255);
      RoboSerial.printf("V,%d\n", speedMultiplier);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/modo", HTTP_GET, []() {
    if (server.hasArg("m")) {
      String mode = server.arg("m");
      turboMode = (mode == "turbo");
      RoboSerial.printf("X,%d\n", turboMode ? 1 : 0);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/acao", HTTP_GET, []() {
    if (server.hasArg("a")) {
      int action = server.arg("a").toInt();
      if (action >= 1 && action <= 4 && !emergencyStop) {
        RoboSerial.printf("A,%d\n", action);
      }
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/stop", HTTP_GET, []() {
    emergencyStop = !emergencyStop;
    if (emergencyStop) {
      RoboSerial.printf("E,1\n");
    } else {
      RoboSerial.printf("E,0\n");
    }
    server.send(200, "text/plain", emergencyStop ? "STOPPED" : "RUNNING");
  });

  server.on("/ping", HTTP_GET, []() {
    // Heartbeat/ping endpoint - always responds to keep connection alive
    lastResponseTime = millis();
    RoboSerial.println("P");
    String json = "{\"ok\":true}";
    server.send(200, "application/json", json);
  });

  server.on("/status", HTTP_GET, []() {
    String json = "{";
    json += "\"battery\":\"" + String(batteryLevel) + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("HTTP server started");
  lastResponseTime = millis();
}

void loop() {
  server.handleClient();

  if (RoboSerial.available()) {
    String response = RoboSerial.readStringUntil('\n');
    if (response.startsWith("B")) {
      int level = response.substring(2).toInt();
      batteryLevel = constrain(level, 0, 100);
    }
  }

  delay(10);
}
