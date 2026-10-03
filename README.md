# 🤖 WALL·E 3D AI Robot — Dual-Brain System
### Raspberry Pi 3 Model B (Master & Visão IA) + Arduino Nano Shield (Controle de Movimento)

[![Python 3](https://img.shields.io/badge/Python-3.9+-blue.svg)](https://www.python.org/)
[![Flask](https://img.shields.io/badge/Web_HUD-Flask-green.svg)](https://palletsprojects.com/p/flask/)
[![OpenCV](https://img.shields.io/badge/Computer_Vision-OpenCV_4-red.svg)](https://opencv.org/)
[![Arduino](https://img.shields.io/badge/Co--Processor-Arduino_Nano-teal.svg)](https://www.arduino.cc/)

Robô WALL·E impresso em 3D equipado com arquitetura **Dual-Brain** de alto desempenho:
1. **Master (Raspberry Pi 3B):** Executa Linux, Visão Computacional OpenCV com rastreamento facial em tempo real (Face Tracking Pan & Tilt), servidor web Flask com streaming MJPEG e soundboard de áudio pela saída P2 (3.5mm).
2. **Slave Co-Processor (Arduino Nano + Sensor Shield):** Controlador em tempo real conectado via USB Serial a 38400 baud. Aciona a tração com torque total (2 motores DC via Ponte H), 7 servomotores SG90 com controle direto no Shield, Display OLED 0.96" vertical com olhos binoculares animados e telemetria de bateria via A0.

---

## 📸 Fotos do Projeto

| Vista Frontal | Vista Lateral | Em Construção |
| :---: | :---: | :---: |
| ![WALL·E 1](Fotos%20Wall-e/IMG_20230905_143028.jpg) | ![WALL·E 2](Fotos%20Wall-e/IMG_20231005_170400.jpg) | ![WALL·E 3](Fotos%20Wall-e/IMG_20250610_082019.jpg) |
| ![WALL·E 4](Fotos%20Wall-e/IMG_20250610_082500.jpg) | ![WALL·E 5](Fotos%20Wall-e/IMG_20250627_081030.jpg) | ![WALL·E 6](Fotos%20Wall-e/IMG_20251111_131129.jpg) |

---

## 🏗️ Arquitetura do Sistema

```text
               ┌────────────────────────────────────────────────────────┐
               │              RASPBERRY PI 3 MODEL B                    │
               │  - Webcam USB (/dev/video0) com OpenCV Face Tracking   │
               │  - Servidor Web Flask (Porta 5000: Painel Cyber HUD)   │
               │  - Saída de Áudio P2 (3.5mm ALSA hw:0,0)               │
               └──────────────────────────┬─────────────────────────────┘
                                          │ Cabo USB (/dev/ttyUSB* - 38400 baud)
                                          ▼
               ┌────────────────────────────────────────────────────────┐
               │              ARDUINO NANO + SENSOR SHIELD              │
               │  - Watchdog de Tração (< 800ms)                        │
               │  - Controle Direto de 7 Servos SG90 via Servo.h        │
               │  - Display OLED SSD1306 Vertical com Expressões        │
               │  - Leitura Analógica de Bateria em A0                  │
               └──────┬────────────────────┬─────────────────────┬──────┘
                      │                    │                     │
                      ▼                    ▼                     ▼
          ┌───────────────────────┐ ┌───────────────┐ ┌───────────────────┐
          │  PONTE H (L298N/DC)   │ │ 7 SERVOS SG90 │ │ DISPLAY OLED 0.96"│
          │  - Motor Esq: D4 / D5 │ │ - D8..D11     │ │ - SDA: A4         │
          │  - Motor Dir: D6 / D7 │ │ - A1..A3      │ │ - SCL: A5         │
          └───────────────────────┘ └───────────────┘ └───────────────────┘
```

---

## 🗺️ Mapa de Pinagem Oficial (Arduino Nano Shield)

| Componente | Função | Pino no Nano Shield | Tipo |
| :--- | :--- | :---: | :---: |
| **Ponte H (Tração DC)** | Motor Esquerdo DIR (IN1) | **D4** | Digital |
| | Motor Esquerdo PWM (IN2) | **D5** | PWM |
| | Motor Direito PWM (IN3) | **D6** | PWM |
| | Motor Direito DIR (IN4) | **D7** | Digital |
| **Servomotores (SG90)** | Servo 0 (Olho Esquerdo) | **D8** | Sinal (S) |
| | Servo 1 (Olho Direito) | **D9** | Sinal (S) |
| | Servo 2 (Pescoço Pan - Giro / Face Tracking) | **A1** | Sinal (S) |
| | Servo 3 (Pescoço Tilt - Cima/Baixo / Face Tracking) | **A2** | Sinal (S) |
| | Servo 4 (Pescoço Roll - Inclinação) | **A3** | Sinal (S) |
| | Servo 5 (Braço Esquerdo) | **D10** | Sinal (S) |
| | Servo 6 (Braço Direito) | **D11** | Sinal (S) |
| **Sensor de Bateria** | Divisor de Voltagem 0-25V (5:1) | **A0** | Analógico |
| **Display OLED 0.96"** | Dados I2C (SDA) | **A4** | I2C Hardware |
| | Clock I2C (SCL) | **A5** | I2C Hardware |
| **Link com Raspberry Pi** | Telemetria e Comandos | **USB** | 38400 baud |

---

## ⚡ Alimentação e Estabilidade Elétrica

1. **Separação de Energia dos Servos:**
   * A linha central (`V`) do **Nano Sensor Shield** é alimentada através do seu conector de energia externo (borne ou plugue P4).
   * **Recomendação:** Regulador **Step-Down (Buck) DC-DC** de 5A (ex: XL4015) ajustado para **`5.8V`**, ligado na bateria principal (2S ou 3S).
   * **Capacitor Buffer:** Instale um capacitor eletrolítico de **1000 µF ou 2200 µF (16V)** em paralelo na entrada de força do Shield para absorver os picos de partida dos servos.
2. **Motores de Tração (Ponte H):**
   * Alimentados diretamente pelo barramento da bateria (7.4V a 12V).
   * O código aplica 100% de tensão contínua no modo turbo para máxima entrega de torque nas esteiras.
3. **Raspberry Pi 3B:**
   * Alimentado via porta micro-USB com fonte/conversor regulado de 5V 2.5A a 3A.

---

## 📂 Estrutura do Repositório

```text
Open-Wall-e-3d-Robot/
├── Raspberry_Pi_Core/              # Código do Cérebro Principal (Python / Flask)
│   ├── app.py                     # Servidor, OpenCV, Face Tracking e Ponte Serial
│   ├── requirements.txt           # Dependências Python
│   ├── walle.service              # Serviço systemd de inicialização automática
│   └── templates/
│       └── index.html             # Painel Web Dark-Mode responsivo
├── Arduino_Nano_Controller/        # Firmware do Co-Processador de Movimento
│   └── WALLE_Slave_Nano.ino       # Firmware C++ completo para Arduino Nano
├── Legacy_ESP32_Versions/         # Versões legadas baseadas em ESP32 / Mega
│   ├── ESP32-Wall-e-Complete.ino
│   ├── ESP32-Wall-e-Unified.ino
│   ├── Esp32-Web-Server-TX.ino
│   └── Mega2560-Wall-e-RX.ino
├── Fotos Wall-e/                  # Galeria de fotos do robô físico
├── CONVERSOR_NIVEL_LOGICO.md      # Guia de níveis lógicos
├── PINOS_COMUNICACAO.md          # Notas adicionais de pinagem
└── README.md
```

---

## 🚀 Instalação e Execução

### 1. Configurando o Raspberry Pi 3B
No terminal do Raspberry Pi (via SSH ou monitor):

```bash
# 1. Instalar dependências de sistema
sudo apt update && sudo apt install -y python3-pip python3-serial python3-opencv python3-flask mpg123 alsa-utils

# 2. Clonar ou copiar o projeto para /home/leo/walle_core
mkdir -p /home/leo/walle_core
cp -r Raspberry_Pi_Core/* /home/leo/walle_core/

# 3. Instalar o serviço systemd de inicialização automática
sudo cp /home/leo/walle_core/walle.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now walle.service

# 4. Acessar a interface Web no navegador
# Abra no PC ou Celular: http://<IP_DO_RASPBERRY>:5000
```

### 2. Gravando o Arduino Nano
1. Abra o arquivo `Arduino_Nano_Controller/WALLE_Slave_Nano.ino` na **Arduino IDE**.
2. Selecione a placa: **Arduino Nano** (Processador: *ATmega328P* ou *ATmega328P Old Bootloader*).
3. Instale as bibliotecas necessárias:
   * `U8g2` (por Oliver Kraus)
   * `Servo` (padrão do Arduino)
4. Conecte o cabo USB e clique em **Carregar (Upload)**.
5. Conecte o cabo USB do Nano em qualquer porta USB do Raspberry Pi.

---

## 📎 Créditos do Modelo 3D

* **Modelo 3D base:** Disponível no Thingiverse — [Thingiverse #3703555](https://www.thingiverse.com/thing:3703555).
* Todos os créditos da modelagem 3D pertencem ao criador original. Este projeto é uma releitura completa de engenharia eletrônica, robótica autônoma e inteligência computacional.

---

## 📄 Licença
Distribuído sob licença aberta para fins educacionais e de pesquisa em robótica.
