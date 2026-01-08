# 📡 Configuração de Pinos - ESP32 UNIFICADO

# 📡 Configuração de Pinos - ESP32 UNIFICADO

## Arquitetura Simplificada

Este projeto utiliza **apenas um ESP32** que controla:
- ✅ Servidor Web WiFi (Access Point)
- ✅ 7 Servos PWM
- ✅ 2 Motores DC via Ponte H L298N
- ✅ Display OLED 0.96" I2C
- ✅ Auto-falante para feedback sonoro

**Sem necessidade de comunicação serial entre placas!**

---

## 🎯 Mapeamento Completo de Pinos

### **Servos (7 unidades - PWM)**
```cpp
const int SERVO_PINS[7] = {13, 12, 14, 27, 26, 25, 33};
```

| Servo | Função | GPIO | Notas |
|-------|--------|------|-------|
| **0** | Braço Esquerdo | **GPIO 13** | SG90 9g |
| **1** | Braço Direito | **GPIO 12** | SG90 9g |
| **2** | Pescoço Base (Rotação) | **GPIO 14** | SG90 9g |
| **3** | Pescoço Inclinação | **GPIO 27** | SG90 9g |
| **4** | Rotação da Cabeça | **GPIO 26** | SG90 9g |
| **5** | Olho Esquerdo | **GPIO 25** | SG90 9g |
| **6** | Olho Direito | **GPIO 33** | SG90 9g |

---

### **Motores DC (Ponte H L298N)**

#### Motor Esquerdo
| Função | GPIO | L298N Pin |
|--------|------|-----------|
| IN1 (Frente) | **GPIO 19** | IN1 |
| IN2 (Trás) | **GPIO 18** | IN2 |
| ENA (PWM) | **GPIO 5** | ENA |

#### Motor Direito
| Função | GPIO | L298N Pin |
|--------|------|-----------|
| IN3 (Frente) | **GPIO 17** | IN3 |
| IN4 (Trás) | **GPIO 16** | IN4 |
| ENB (PWM) | **GPIO 4** | ENB |

**Configuração PWM:**
- Frequência: 1000 Hz
- Resolução: 8 bits (0-255)
- Canal PWM Esquerdo: 0
- Canal PWM Direito: 1

---

### **Display OLED SSD1306 0.96" (I2C)**
| Função | GPIO | OLED Pin |
|--------|------|----------|
| SDA (Dados) | **GPIO 21** | SDA |
| SCL (Clock) | **GPIO 22** | SCL |
| VCC | 3.3V | VCC |
| GND | GND | GND |

**Endereço I2C:** `0x3C` (padrão)

---

### **Auto-falante**
| Função | GPIO | Notas |
|--------|------|-------|
| Speaker Out | **GPIO 23** | Saída PWM para sons |

⚠️ **Nota:** Conectar via transistor/amplificador para proteger o ESP32

---

### **LED de Status**
| Função | GPIO | Notas |
|--------|------|-------|
| LED Onboard | **GPIO 2** | LED azul integrado |

---

## 🔌 Diagrama de Conexões

```
                           ╔═══════════════════════════╗
                           ║       ESP32 DevKit        ║
                           ╠═══════════════════════════╣
                           ║                           ║
    Servos (7x)            ║  GPIO 13, 12, 14, 27,     ║
    └─────────────────────→║       26, 25, 33          ║
                           ║                           ║
    Display OLED           ║  GPIO 21 (SDA)            ║
    └─────────────────────→║  GPIO 22 (SCL)            ║
                           ║                           ║
    Auto-falante           ║  GPIO 23 (PWM)            ║
    └─────────────────────→║                           ║
                           ║                           ║
    Ponte H L298N          ║  GPIO 19,18,5 (Motor L)   ║
    └─────────────────────→║  GPIO 17,16,4 (Motor R)   ║
                           ║                           ║
                           ╚═══════════════════════════╝
```

---

## ⚙️ Configuração WiFi

### Access Point (AP)
```cpp
SSID: "ROBO-CTRL"
Password: "12345678"
IP: 192.168.4.1 (padrão AP)
```

### Endpoints da API
```
GET /                    → Interface Web principal
GET /servo?id=0-6&ang=0-180  → Controla servo
GET /motor?l=-255-255&r=-255-255  → Controla motores
GET /speed?val=30-255    → Ajusta velocidade
GET /modo?m=normal|turbo → Altera modo
GET /acao?a=1-4          → Executa ação
GET /stop                → Parada de emergência
GET /ping                → Heartbeat
GET /status              → Status do robô
```

---

## 📊 Informações Exibidas no OLED

O display mostra em tempo real:
- 📡 **WiFi:** Número de clientes conectados
- ⚡ **Motores:** Velocidade L/R (-255 a +255)
- 🚀 **Modo:** Normal ou Turbo
- 🎚️ **Speed:** Multiplicador de velocidade (%)
- 🛑 **Emergency:** Alerta visual se ativado

Atualização: **500ms**

---

## 🔧 Bibliotecas Necessárias

Instalar via Arduino IDE Library Manager:

1. **ESP32Servo** - Controle de servos no ESP32
2. **Adafruit GFX Library** - Gráficos para OLED
3. **Adafruit SSD1306** - Driver do display OLED

```bash
# No Arduino IDE:
Sketch → Include Library → Manage Libraries
# Buscar e instalar as 3 bibliotecas acima
```

---

## ⚡ Alimentação

| Componente | Tensão | Corrente |
|------------|--------|----------|
| **ESP32** | 3.3V (regulado) | ~500mA (pico com WiFi) |
| **Servos (7x)** | 5V | ~300mA cada (pico) = 2.1A total |
| **Motores DC** | 7-12V | Varia conforme carga |
| **OLED** | 3.3V ou 5V | ~20mA |
| **Auto-falante** | 3.3V PWM | ~100mA |

**Recomendação:**
- ESP32 + OLED: Alimentar pelo USB ou regulador 5V→3.3V
- Servos: Fonte externa 5V 3A+ (com capacitor 1000µF)
- Motores: Bateria LiPo 2S/3S direto no L298N

⚠️ **Importante:** Conectar todos os GND juntos!

---

## ✅ Checklist de Montagem

- [ ] ESP32 alimentado via USB ou 5V regulado
- [ ] Servos conectados aos GPIOs corretos (13,12,14,27,26,25,33)
- [ ] Ponte H L298N conectada (motores + GPIOs)
- [ ] Display OLED no I2C (SDA=21, SCL=22)
- [ ] Auto-falante no GPIO 23 (com proteção)
- [ ] Todos os GND conectados juntos
- [ ] Fonte de 5V adequada para servos (3A+)
- [ ] Bateria para motores conectada ao L298N
- [ ] Bibliotecas instaladas no Arduino IDE
- [ ] Código ESP32-Wall-e-Complete.ino compilado e enviado

---

## 🧪 Teste Rápido

1. **Upload do código** no ESP32
2. **Conectar ao WiFi** ROBO-CTRL (senha: 12345678)
3. **Abrir navegador** em http://192.168.4.1
4. **Testar cada servo** individualmente
5. **Testar joystick** para motores
6. **Verificar OLED** mostrando status
7. **Testar ações** (acenar, girar, beep, música)

Se tudo funcionar, está pronto! 🎉

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
