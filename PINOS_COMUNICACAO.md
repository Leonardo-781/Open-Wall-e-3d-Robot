# 📡 Pinos de Comunicação Serial - ESP32 ↔ Mega2560

## Configuração de Pinos

### **ESP32 (Transmissor)**
```cpp
RoboSerial.begin(115200, SERIAL_8N1, 20, 21);
```
| Função | Pino GPIO | Descrição |
|--------|-----------|-----------|
| **RX** | GPIO 20 | Recebe dados do Mega2560 |
| **TX** | GPIO 21 | Envia dados para o Mega2560 |
| **Baud** | 115200 | Velocidade de comunicação |

### **Arduino Mega2560 (Receptor)**
```cpp
Serial1.begin(115200); // RX1=18, TX1=19
```
| Função | Pino | Descrição |
|--------|------|-----------|
| **RX1** | Pino 18 | Recebe dados do ESP32 |
| **TX1** | Pino 19 | Envia dados para o ESP32 |
| **Baud** | 115200 | Velocidade de comunicação |

---

## 🔌 Diagrama de Conexão

```
ESP32                          Arduino Mega2560
┌──────────────────┐           ┌──────────────────┐
│                  │           │                  │
│  GPIO 21 (TX) ───┼───────────┼──→ Pino 18 (RX1) │
│                  │           │                  │
│  GPIO 20 (RX) ←──┼───────────┼─── Pino 19 (TX1)│
│                  │           │                  │
│  GND ────────────┼───────────┼─── GND           │
│                  │           │                  │
└──────────────────┘           └──────────────────┘
```

### **Conexão Física (3 fios):**
```
ESP32 GPIO 21 (TX)  → Mega2560 Pino 18 (RX1)
ESP32 GPIO 20 (RX)  → Mega2560 Pino 19 (TX1)
ESP32 GND           → Mega2560 GND
```

---

## 📊 Especificações da Comunicação

| Parâmetro | Valor |
|-----------|-------|
| **Protocolo** | UART Serial |
| **Baudrate** | 115200 bps |
| **Data Bits** | 8 |
| **Stop Bits** | 1 |
| **Parity** | None |
| **Flow Control** | None |

---

## 📨 Protocolo de Dados

### **Comandos Enviados (ESP32 → Mega2560):**
```
S,id,angle       → Servo control (ex: S,0,90)
M,left,right     → Motor control (ex: M,255,-128)
V,value          → Velocity (ex: V,200)
X,mode           → Mode (0=Normal, 1=Turbo)
A,action         → Action (1=Wave, 2=Spin, 3=Beep, 4=Music)
E,status         → Emergency (1=Stop, 0=Resume)
P                → Ping (heartbeat)
```

### **Respostas Enviadas (Mega2560 → ESP32):**
```
B,level          → Battery level (ex: B,85)
```

---

## 🔧 Verificação dos Pinos

### **Teste no ESP32:**
```cpp
// Enviar teste
RoboSerial.println("S,0,90");  // Move servo 0 para 90 graus

// Receber resposta
if (RoboSerial.available()) {
  String response = RoboSerial.readStringUntil('\n');
  Serial.println(response);
}
```

### **Teste no Mega2560:**
```cpp
// Receber comando
while (Serial1.available()) {
  String command = Serial1.readStringUntil('\n');
  Serial.println("Recebido: " + command);
}

// Enviar resposta
Serial1.println("B,100");  // Bateria 100%
```

---

## ⚠️ Dicas Importantes

1. **Usar cabo de boa qualidade** - Serial em 115200 bps precisa de conexão estável
2. **Manter fios curtos** - Reduz interferência eletromagnética
3. **Usar mesma GND** - Ambas as placas devem compartilhar a mesma terra
4. **Não usar 5V** - ESP32 usa 3.3V (compatível com 3.3V do Mega no RX/TX)
5. **Verificar orientação** - TX sempre vai para RX, RX sempre vem de TX

---

## 🧪 Teste de Comunicação

Para testar se os pinos estão funcionando:

**No Monitor Serial do ESP32 (após upload):**
```
Connected to WiFi
Ready to send commands to Mega2560
Sending heartbeat pings...
```

**No Monitor Serial do Mega2560 (após upload):**
```
=== Wall-E 3D Robot Receiver Starting ===
Servo 0 attached to pin 22
...
Initialization complete. Waiting for commands...
Received: P
Received: S,0,90
Motor -> L:0 R:0
```

Se vir essas mensagens, os pinos estão **100% corretos e funcionando!** ✅
