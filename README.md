
---

### `README.md` (atualizado)

```markdown
# WALL·E 3D — Robô com ESP32 (Controle WiFi + Tank drive + 7 servos)

![WALL·E](docs/img/walle.jpg)

Robô WALL·E impresso em 3D com **ESP32** (WiFi integrado),
**ponte H L298N** para duas esteiras (motores DC), **7× servos SG90** (cabeça, olhos, braços),
**display OLED 0.96"** e **auto-falante** para feedback sonoro.
Controle via **interface web** responsiva e moderna, com watchdog de segurança.

---

## 📸 Fotos do projeto

| Vista 1 | Vista 2 | Vista 3 |
|---|---|---|
| ![WALL·E - Vista 1](Fotos%20Wall-e/IMG_20230905_143028.jpg) | ![WALL·E - Vista 2](Fotos%20Wall-e/IMG_20231005_170400.jpg) | ![WALL·E - Vista 3](Fotos%20Wall-e/IMG_20250610_082019.jpg) |
| ![WALL·E - Vista 4](Fotos%20Wall-e/IMG_20250610_082500.jpg) | ![WALL·E - Vista 5](Fotos%20Wall-e/IMG_20250627_081030.jpg) | ![WALL·E - Vista 6](Fotos%20Wall-e/IMG_20251111_131129.jpg) |

> ⚠️ **Segurança**: uso educacional. Teste primeiro sem carga e **SEM hélices/partes soltas**.
> Desconecte a bateria antes de mexer na fiação. LiPo exige atenção (veja `docs/safety.md`).

---

## 📎 Origem do modelo 3D & créditos

- **Modelo 3D base**: retirado do **Thingiverse** —  
  **Link**: https://www.thingiverse.com/thing:3703555#google_vignette  
- **Créditos integrais do projeto 3D** ao **criador original** no Thingiverse.  
- Este repositório é **uma releitura**: realizamos **adaptações e modificações** em algumas peças,
  usamos **outros componentes eletrônicos e software/controles**, e descrevemos **um método de montagem**
  e **pintura** próprios.
- Para respeitar a licença do autor, **não redistribuímos os STL originais aqui**; consulte o link acima
  para baixar os arquivos e **verificar a licença** aplicável.

---

## ✨ Destaques
- **ESP32 Unificado**: Uma única placa controla tudo (WiFi + servos + motores)
- **Interface Web Moderna**: Controle completo via navegador com joystick virtual
- **Display OLED 0.96"**: Status em tempo real (velocidade, modo, conexões)
- **Auto-falante**: Feedback sonoro para ações e alertas
- Direção **tank** (diferencial) com controle de velocidade variável
- **7 servos** com SG90 (cabeça, olhos, braços)
- **Watchdog de segurança**: Sem comando = motores param automaticamente
- **Modo Turbo**: Alterna entre velocidade normal e máxima

---

## 🧵 Impressão 3D (material, impressoras e notas)

- **Material**: **PLA** (ácido polilático)
  - **Prós**: fácil de imprimir, baixa contração/warp, bom acabamento, biodegradável (baseado em fontes renováveis),
    ideal para **peças visuais e funcionais leves** como carenagens, suportes e partes externas.
  - **Contras**: **sensível ao calor** (Tg ~60 °C); pode amolecer dentro de carro quente/exposição solar direta; menor resistência à fadiga
    em comparação a PETG/ABS.
  - **Parâmetros típicos** (ponto de partida): bico **200–210 °C**, mesa **55–65 °C**, camada **0,20 mm**, infill **20–35%** (peças estruturais até 40–60%).
- **Impressoras**: impresso em **duas impressoras da linha Creality Ender** (ex.: Ender-3/Ender-3 Pro/derivadas).
- **Pós-processo**: lixamento fino, primer automotivo, pintura acrílica (camadas finas) e *weathering* leve para efeito “sujinho”.
- Detalhes adicionais de montagem/pintura estão em `docs/assembly.md`.

---

## 📂 Estrutura do repositório
```



````

---

## 🧰 BOM (Bill of Materials)

| Qtde | Item                                      | Notas |
|----:|-------------------------------------------|------|
| 1   | **ESP32 DevKit**                          | MCU principal com WiFi integrado |
| 1   | **Display OLED SSD1306 0.96" I2C**       | Feedback visual em tempo real |
| 1   | **Auto-falante 8Ω 0.5W**                  | Feedback sonoro (ou buzzer piezo) |
| 1   | **L298N**                                  | Ponte H para 2 motores DC |
| 2   | Motor DC alto torque/baixa rotação         | Esteiras |
| 7   | **Servo SG90 9g**                          | Cabeça/olhos/braços |
| 1   | Bateria (LiPo 2S/3S ou pack 7–12 V)        | Alimentação geral |
| 1   | Conversor Buck 5V (opcional)               | Para alimentar ESP32 e servos |
| n   | Fios, terminais, parafusos                 | Montagem |
| n   | **Capacitores**: 470–1000 µF (barramento 5 V) | Estabilidade dos servos |

> Detalhes de corrente/queda de tensão/autonomia: `docs/power.md`.

---

## 🔌 Pinout (ESP32)

### Servos (7 unidades)
- Servo 0 (Braço Esquerdo) → **GPIO 13**
- Servo 1 (Braço Direito) → **GPIO 12**
- Servo 2 (Pescoço Base) → **GPIO 14**
- Servo 3 (Pescoço Inclinação) → **GPIO 27**
- Servo 4 (Rotação Cabeça) → **GPIO 26**
- Servo 5 (Olho Esquerdo) → **GPIO 25**
- Servo 6 (Olho Direito) → **GPIO 33**

### L298N (Ponte H - Motores)
- Motor Esquerdo:
  - IN1 → **GPIO 19**
  - IN2 → **GPIO 18**
  - ENA (PWM) → **GPIO 5**
- Motor Direito:
  - IN3 → **GPIO 17**
  - IN4 → **GPIO 16**
  - ENB (PWM) → **GPIO 4**

### Display OLED (I2C)
- SDA → **GPIO 21**
- SCL → **GPIO 22**
- Endereço: **0x3C**

### Auto-falante
- Speaker+ → **GPIO 23** (via transistor/amplificador)
- Speaker- → GND

### LED Status
- LED onboard → **GPIO 2**
- VMOT → **Bateria 7–12 V**
- 5V lógica → **LM2596 (5 V)** *(remover/ignorar 5V-jumper interno)*
- GND → **comum**

### Servos (7× SG90)
- PAN (cabeça) → **D22**
- TILT (cabeça) → **D23**
- OLHOS → **D24**
- BRAÇO E → **D25**
- BRAÇO D → **D26**
- PUNHO E → **D27**
- PUNHO D → **D28**
- V+ dos servos → **LM2596 (5.0–5.3 V)** | **GND comum**

> Esquemas/ASCII: `docs/wiring.md`.

---

## 📡 Protocolo RF (resumo)
- Endereço: `"00001"` (pipe 1)
- DataRate: **250 kbps** (robustez/alcance)
- Payload (11 bytes):
  ```c
  struct PacketRF {
    uint8_t ch[8];  // 0..255
    uint8_t s1;     // botões/flags
    uint8_t s2;     // botões/flags
    uint8_t crc;    // opcional
  };
````

* Mapeamento: `ch0`=throttle, `ch1`=steer, `ch2..ch7`=servos.
* *Failsafe*: >600 ms sem pacote → motores **param** (servos podem voltar a 90°).

> Detalhe/CRC/heartbeat: `docs/protocol.md`.

---

## 🧪 Firmware (RX no Mega)

Código de referência em `firmware/mega_rx/mega_rx.ino` — RF24 + tank mixing + 7 servos + failsafe.
Arduino IDE (lib **RF24/TMRh20**) ou PlatformIO (`platformio.ini` incluso).

---

## 🏗️ Montagem

Passo a passo mecânico/eletrônico com checklist e fotos: `docs/assembly.md`.
Inclui observações sobre **pintura** e **detalhes únicos** desta montagem.

---

## 🧭 Calibração & Uso

1. Ajuste **LM2596** para **5.0–5.3 V** sem carga.
2. Boota com servos em **90°**.
3. TX: `ch0` acelera/reverte; `ch1` gira; `ch2..ch7` = servos.
4. Teste *failsafe* desligando o TX.

Calibração fina: `docs/testing.md`.

---

## 🧯 Troubleshooting (rápido)

* **nRF mudo**: 3.3 V estável + capacitor; **SCK=52/MOSI=51/MISO=50**; CE/CSN corretos.
* **Servos tremendo**: LM2596 com folga; **1000 µF** no 5 V; *ground* comum; deadband.
* **Mega reinicia**: queda de tensão; separe VMOT dos motores da lógica/servos; reforce GND.
* **L298N quente**: dissipador/ventilação; limite PWM; considere ponte MOSFET futura.

Lista estendida: `docs/faq.md`.

---

## 🗺️ Roadmap

* Áudio/LEDs nos olhos; telemetria (bateria/RSSI); IMU/autonomia.
* Ponte H MOSFET (eficiência) e **PCB** dedicada.

---

## 🤝 Contribuição

Contribuições são bem-vindas! Veja `CONTRIBUTING.md`.

---

## 📜 Licença

Este projeto (código, documentação e peças **derivadas autorais**) está sob **MIT** (`LICENSE`).
**Modelos 3D originais**: consulte a **licença do criador no Thingiverse** (link acima). Respeite os termos.

---

## 🙌 Créditos

* Autor(es): *Leonardo Cardoso / MonteBot / Laboratório de Robótica*
* UFU / Núcleo de Robótica
* Comunidade **Liga Academica de IA e Robótica**
* **Criador original do modelo 3D** no Thingiverse (link em “Origem do modelo 3D & créditos”)

````


- **Modelo 3D base**: WALL·E no Thingiverse — https://www.thingiverse.com/thing:3703555#google_vignette  
  - Direitos e licença do modelo 3D pertencem ao respectivo **criador original** no Thingiverse.
  - Este repositório **não** redistribui os STL originais; apenas referencia o link oficial.
- **Releitura/derivações**: adaptações de peças, eletrônica (Arduino Mega + nRF24 + L298N + SG90),
  firmware, método de montagem, pintura e documentação por **[seu nome/equipe]**.
- Bibliotecas de software: **RF24 (TMRh20)**, **Servo (Arduino)**.
````
