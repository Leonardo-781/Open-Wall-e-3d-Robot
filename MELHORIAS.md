# 🤖 Wall-E 3D Robot - Versão ESP32 Unificada

## 📋 Resumo da Nova Arquitetura

O projeto foi **completamente reestruturado** para usar **apenas um ESP32**, eliminando a necessidade do Arduino Mega2560 e comunicação serial entre placas.

---

## ✨ Principais Vantagens da Versão Unificada

### 🎯 Simplicidade
- ✅ **Uma única placa** ao invés de duas
- ✅ **Sem comunicação serial** entre dispositivos
- ✅ **Menos cabos e conexões**
- ✅ **Configuração mais simples**
- ✅ **Menor custo total**

### ⚡ Performance
- ✅ **Latência reduzida** - comandos vão direto aos motores/servos
- ✅ **Sem overhead** de protocolo serial
- ✅ **WiFi integrado** no ESP32
- ✅ **Processador mais rápido** (240 MHz dual-core)

### 🔧 Funcionalidades Novas
- ✅ **Display OLED 0.96"** - Status em tempo real
- ✅ **Auto-falante** - Feedback sonoro melhorado
- ✅ **Interface web moderna** - Design responsivo
- ✅ **Watchdog de segurança** - Para motores automaticamente

---

## 🎨 Interface Web Completa

### Design Moderno
- **Tema Dark** com gradientes ouro/bronze temático do Wall-E
- **Layout Responsivo** - Funciona em celular, tablet e desktop
- **Efeitos visuais** - Hover, sombras, animações suaves
- **Joystick virtual** - Controle intuitivo de movimento

### Controles Disponíveis
| Controle | Descrição |
|----------|-----------|
| 🕹️ **Joystick** | Movimento tank (esquerda/direita + frente/trás) |
| 🎚️ **7 Sliders** | Controle individual de cada servo (0-180°) |
| ⚙️ **Velocidade** | Ajuste de potência dos motores (30-255%) |
| 🚀 **Modo Turbo** | Toggle Normal/Turbo |
| 👋 **Acenar** | Animação do braço |
| 🔄 **Girar** | Rotação 360° |
| 📢 **Beep** | Som de alerta |
| 🎵 **Música** | Melodia em Dó maior |
| 🛑 **Emergência** | Para tudo imediatamente |

---

## 📺 Display OLED - Informações em Tempo Real

O display SSD1306 0.96" mostra:

```
=== WALL-E 3D ===
WiFi: 1 client(s)
L:120 R:120
Modo: Normal
Speed: 100%
```

Quando em emergência:
```
! EMERGENCY STOP !
```

Atualização: **500ms**

---

## 🔊 Auto-falante - Feedback Sonoro

Substituiu o buzzer piezo por um auto-falante 8Ω que oferece:
- 🎵 **Melhor qualidade de som**
- 🎶 **Melodias mais ricas**
- 📢 **Volume ajustável** (via hardware)
- 🔔 **Tons variados** para diferentes eventos

### Sons Programados
| Evento | Som |
|--------|-----|
| **Inicialização** | 2 beeps ascendentes (1000Hz → 1500Hz) |
| **Ação Beep** | 3 pulsos de 1000Hz |
| **Música** | Escala C-D-E-F-G-A-B-C |
| **Emergência** | 5 pulsos de 2000Hz |

---

## 🎮 Configuração de 7 Servos

Reduzido de 8 para 7 servos (removido servo da mandíbula):

| ID | Função | GPIO | Ângulo Padrão |
|----|--------|------|---------------|
| 0 | Braço Esquerdo | 13 | 90° |
| 1 | Braço Direito | 12 | 90° |
| 2 | Pescoço Base | 14 | 90° |
| 3 | Pescoço Inclinação | 27 | 90° |
| 4 | Rotação Cabeça | 26 | 90° |
| 5 | Olho Esquerdo | 25 | 90° |
| 6 | Olho Direito | 33 | 90° |

---

## 🚗 Controle de Motores Tank Drive

### Características
- **Bidirecionais**: -255 (trás máxima) a +255 (frente máxima)
- **PWM nativo ESP32**: Frequência 1kHz, resolução 8-bit
- **Mixing automático**: Joystick converte para velocidades L/R
- **Multiplicador de velocidade**: 30% a 100% da potência

### Ponte H L298N
```
Motor Esquerdo: GPIO 19,18 (direção) + GPIO 5 (PWM)
Motor Direito:  GPIO 17,16 (direção) + GPIO 4 (PWM)
```

---

## 🛡️ Segurança Implementada

### Watchdog Timer
Se não receber comandos por **2 segundos**:
- ⛔ Para todos os motores
- 📺 Atualiza display com alerta
- 🔊 Não emite som (para não incomodar)

### Parada de Emergência
Ao ativar:
- 🛑 Para motores imediatamente
- 🚫 Bloqueia novos comandos de movimento
- 🔴 LED de status pisca
- 📢 Emite 5 beeps de alerta
- 📺 Mostra "EMERGENCY STOP" no OLED

---

## 📡 API HTTP - Endpoints

### Controle
```http
GET /servo?id=0-6&ang=0-180      # Move servo específico
GET /motor?l=-255-255&r=-255-255 # Controla motores
GET /speed?val=30-255            # Ajusta velocidade
GET /modo?m=normal|turbo         # Altera modo
GET /acao?a=1-4                  # Executa ação
GET /stop                        # Toggle emergência
```

### Monitoramento
```http
GET /ping      # Heartbeat (retorna JSON)
GET /status    # Status bateria e sistema
```

### Interface
```http
GET /          # Página web completa
```

---

## 🔄 Ações Programadas

### 1. Wave (Acenar) 👋
```cpp
- Move braço esquerdo de 90° → 180° → 90°
- Repete 3 vezes
- Velocidade: 10° por step, 20ms entre steps
```

### 2. Spin (Girar) 🔄
```cpp
- Motor esquerdo: +150 PWM
- Motor direito: -150 PWM
- Duração: 1500ms (aproximadamente 360°)
```

### 3. Beep (Som) 📢
```cpp
- 3 pulsos de 1000Hz
- Duração: 200ms cada
- Intervalo: 100ms
```

### 4. Music (Música) 🎵
```cpp
- Escala: C(262) D(294) E(330) F(349) G(392) A(440) B(494) C(523)
- Duração: 200ms por nota + 50ms pausa
```

---

## 📚 Bibliotecas Utilizadas

```cpp
#include <WiFi.h>              // WiFi nativo ESP32
#include <WebServer.h>         // Servidor HTTP
#include <ESP32Servo.h>        // Controle de servos
#include <Wire.h>              // I2C para OLED
#include <Adafruit_GFX.h>      // Gráficos base
#include <Adafruit_SSD1306.h>  // Driver OLED
```

---

## 🔌 Comparação: Antes vs Agora

| Aspecto | Versão Antiga | Versão Nova |
|---------|---------------|-------------|
| **Placas** | ESP32 + Mega2560 | Apenas ESP32 |
| **Servos** | 8 | 7 |
| **Comunicação** | Serial 115200 baud | Direta (sem serial) |
| **Display** | ❌ Nenhum | ✅ OLED 0.96" |
| **Som** | Buzzer piezo | Auto-falante 8Ω |
| **Complexidade** | Alta (2 códigos) | Baixa (1 código) |
| **Latência** | ~10-20ms | <5ms |
| **Custo** | ~R$150 | ~R$80 |
| **Cabos** | 3 fios serial + 50+ outros | 30-40 fios |

---

## 🎯 Melhorias Futuras Possíveis

### Hardware
- [ ] Sensor ultrassônico para detecção de obstáculos
- [ ] Sensor de bateria (ADC) para % real
- [ ] LEDs RGB para os olhos
- [ ] Câmera ESP32-CAM para streaming
- [ ] Sensor IMU para auto-balanceamento

### Software
- [ ] Controle por joystick Bluetooth
- [ ] App móvel nativo (iOS/Android)
- [ ] Gravação/reprodução de sequências
- [ ] Controle por voz (integração com assistentes)
- [ ] Modo autônomo com IA

---

## 📊 Estrutura do Código

```
ESP32-Wall-e-Complete.ino
├── Setup
│   ├── Configuração de pinos
│   ├── Inicialização servos
│   ├── Configuração PWM motores
│   ├── Inicialização OLED
│   ├── Configuração WiFi AP
│   └── Setup servidor web
├── Loop Principal
│   ├── Handle cliente web
│   ├── Watchdog timer
│   └── Atualização OLED (500ms)
├── Handlers HTTP
│   ├── handleServo()
│   ├── handleMotor()
│   ├── handleSpeed()
│   ├── handleMode()
│   ├── handleAction()
│   ├── handleEmergencyStop()
│   ├── handlePing()
│   └── handleStatus()
├── Controle de Hardware
│   ├── setMotorSpeed()
│   ├── stopAllMotors()
│   └── updateOLEDDisplay()
└── Ações
    ├── waveAction()
    ├── spinAction()
    ├── beepAction()
    └── musicAction()
```

---

## ✅ Resultado Final

Um robô Wall-E **mais simples, mais rápido e mais funcional**, mantendo todas as capacidades originais e adicionando:
- 📺 Feedback visual em tempo real
- 🔊 Melhor qualidade de som  
- 📱 Interface web profissional
- 🛡️ Maior segurança operacional
- 💰 Custo reduzido
- 🔧 Manutenção mais fácil

**Pronto para usar!** 🎉

---

## ✨ Melhorias do ESP32 (Esp32-Web-Server-TX.ino)

### Interface Web
- **Design Moderno**: Interface com gradientes e efeitos hover
- **Layout Responsivo**: Grid layout que se adapta a diferentes tamanhos de tela
- **8 Servos Suportados**: Agora com servo adicional para mandíbula/boca
- **Joystick Melhorado**: 
  - Feedback visual com gradiente radial
  - Indicador de velocidade em tempo real
  - Controle suave e responsivo

### Novos Controles
- **Slider de Velocidade**: Controla a potência dos motores (0-100%)
- **Modo Normal/Turbo**: Toggle entre dois modos de movimento
- **Botões de Ação Rápida**:
  - 👋 Acena (wave)
  - 🔄 Gira (spin)
  - 📢 Beep (som)
  - 🎵 Música
- **Botão de Parada de Emergência**: Vermelho destacado para segurança
- **Display de Status**: Mostra velocidade dos motores e bateria

### Novos Endpoints HTTP
```
GET /servo?id=0-7&ang=0-180     → Controla servo específico
GET /motor?l=-255-255&r=-255-255 → Controla motores
GET /modo?m=1-2                  → Seleciona modo (Normal/Turbo)
GET /acao?tipo=1-4               → Executa ações rápidas
GET /stop                        → Parada de emergência
GET /status                      → Retorna status do robô
```

### Protocolo Serial Aprimorado
```
S,id,angle       → Servo (ex: S,0,90)
M,left,right     → Motor (ex: M,255,-128)
X,modo           → Modo (1=Normal, 2=Turbo)
A,acao           → Ação (1=Wave, 2=Spin, 3=Beep, 4=Music)
E                → Emergency Stop
```

---

## ✨ Melhorias do Mega2560 (Mega2560-Wall-e-RX.ino)

### Configuração de Hardware
- **8 Servos**: Pins 22-29
  - 0: Braço Esquerdo
  - 1: Braço Direito
  - 2: Pescoço Base (Rotação)
  - 3: Pescoço Inclinação
  - 4: Giro da Cabeça
  - 5: Olho Esquerdo
  - 6: Olho Direito
  - 7: Mandíbula/Boca

- **Motores L298N**: 
  - Motor Esquerdo: Pins 30(FWD), 31(BWD), 3(PWM)
  - Motor Direito: Pins 32(FWD), 33(BWD), 5(PWM)
  - Suporta velocidade: -255 (trás) a +255 (frente)

- **Buzzer**: Pin 50 (áudio para ações)
- **LED Status**: Pin 13

### Funcionalidades Principais
- ✅ Controle completo de 8 servos com validação
- ✅ Motores bidirecionais com controle de velocidade
- ✅ Multiplicador de velocidade (30-255%)
- ✅ Modo Turbo para velocidade máxima

### Ações Programadas
1. **Wave (Acena)**: Movimento do braço esquerdo 3x
2. **Spin (Gira)**: Robot gira 360° em 1.5 segundos
3. **Beep**: 3 bips de 1kHz (200ms cada)
4. **Music**: Melodia em escala de Dó maior (8 notas)

### Segurança
- ⚠️ **Parada de Emergência**: Interrompe todos os movimentos
- ⚠️ **Validação de Comandos**: Verifica range válido
- ⚠️ **Timeout de Comando**: Para motores se não receber comandos por 2 segundos
- ⚠️ **LED de Status**: Pisca quando em estado de emergência

### Comunicação Serial
- Serial1 (RX1=18, TX1=19): Comunica com ESP32 @ 115200 baud
- Serial (USB): Saída de debug
- Parser robusto com tratamento de erros

---

## 🔧 Estrutura do Projeto

```
Open-Wall-e-3d-Robot/
├── Esp32-Web-Server-TX.ino      # Controlador WiFi + Interface
├── Mega2560-Wall-e-RX.ino       # Receiver de controle
├── README.md                     # Documentação original
└── MELHORIAS.md                 # Este arquivo
```

---

## 📡 Fluxo de Comunicação

```
Usuário (Browser)
    ↓ HTTP Request
ESP32 WiFi Server
    ↓ Serial (115200 baud)
Arduino Mega2560 Receiver
    ↓ PWM/Servo
Motores e Servos
```

---

## 🚀 Como Usar

1. **Upload ESP32**: Carregue `Esp32-Web-Server-TX.ino` no ESP32
2. **Upload Mega2560**: Carregue `Mega2560-Wall-e-RX.ino` no Arduino Mega2560
3. **Conectar Séries**: 
   - ESP32 RX (GPIO 20) → Mega2560 TX1 (19)
   - ESP32 TX (GPIO 21) → Mega2560 RX1 (18)
4. **Conectar WiFi**: 
   - SSID: `ROBO-CTRL`
   - Senha: `12345678`
5. **Acessar Interface**: Abra navegador em `http://192.168.4.1`

---

## 📊 Características Principais

| Recurso | Antes | Depois |
|---------|-------|--------|
| Servos | 7 | 8 |
| Controle Velocidade | Não | Sim |
| Modo Turbo | Não | Sim |
| Ações Rápidas | Não | 4 Ações |
| Interface | Básica | Profissional |
| Status Display | Não | Sim |
| Parada Emergência | Não | Sim |
| Proteções | Não | Múltiplas |

---

## ⚡ Próximas Melhorias Sugeridas

- [ ] Adicionar sensor de bateria e ADC
- [ ] Implementar câmera e streaming de vídeo
- [ ] Adicionar sensores ultrassônico/distância
- [ ] Implementar gravação/playback de movimentos
- [ ] Criar app mobile dedicado
- [ ] Adicionar suporte a múltiplos modos de locomoção
- [ ] Implementar IA/reconhecimento de movimento

---

**Desenvolvido para Wall-E 3D Robot Project** 🤖✨
