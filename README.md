
---

### `README.md` (atualizado)

```markdown
# WALL·E 3D — Robô com Arduino Mega + nRF24L01 (Tank drive + 7 servos)

![WALL·E](docs/img/walle.jpg)

Robô WALL·E impresso em 3D com **Arduino Mega 2560**, **nRF24L01+** (qualquer TX compatível),
**ponte H L298N** para duas esteiras (motores DC), e **7× servos SG90** (cabeça, olhos, braços).
Controle por rádio 2.4 GHz, *failsafe*, *mixing* de tanque, e arquitetura elétrica segura com **LM2596**.

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
- Compatível com **qualquer transmissor** baseado em nRF24 (endereço/payload combinam).
- Direção **tank** (diferencial) com *mixing* e limite de PWM.
- **7 DOF** com SG90 (PAN/TILT cabeça, olhos, braços).
- **Failsafe** por *watchdog* de pacote: sem RF = motores param e servos neutros.
- Estrutura elétrica robusta: **LM2596** isolando lógica/servos dos motores (L298N).

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
| 1   | **Arduino Mega 2560**                     | MCU principal |
| 1   | **nRF24L01+** (ideal PA/LNA + antena)     | Rádio 2.4 GHz; capacitor 10–47 µF no VCC |
| 1   | **LM2596** (buck 4–40 V→5.0–5.3 V ≥3 A)   | Lógica + servos |
| 1   | **L298N**                                  | 2 canais DC para esteiras |
| 2   | Motor DC alto torque/baixa rotação         | Esteiras |
| 7   | **Servo SG90 9g**                          | Cabeça/olhos/braços/punhos |
| 1   | Shield/protoboard para Mega                | Solda/organização |
| 1   | Bateria (LiPo 2S/3S ou pack 7–12 V)        | Motores + LM2596 |
| n   | Fios, terminais, parafusos, dissipadores   | Montagem |
| n   | **Capacitores**: 10–47 µF (nRF), 470–1000 µF (barramento 5 V) | Estabilidade |

> Detalhes de corrente/queda de tensão/autonomia: `docs/power.md`.

---

## 🔌 Pinout (Arduino Mega)

### nRF24L01+
- CE → **D48**
- CSN (CS) → **D49**
- **SCK=52**, **MOSI=51**, **MISO=50** (SPI HW do Mega)
- IRQ → D2 (opcional)
- VCC **3.3 V** (NÃO 5 V) + **10–47 µF** próximo ao módulo
- GND comum

### L298N (tanque)
- ENA (PWM) → **D5**
- IN1 → **D30**
- IN2 → **D31**
- IN3 → **D32**
- IN4 → **D33**
- ENB (PWM) → **D6**
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

---

### `CREDITS.md` (opcional)

```markdown
# Créditos

- **Modelo 3D base**: WALL·E no Thingiverse — https://www.thingiverse.com/thing:3703555#google_vignette  
  - Direitos e licença do modelo 3D pertencem ao respectivo **criador original** no Thingiverse.
  - Este repositório **não** redistribui os STL originais; apenas referencia o link oficial.
- **Releitura/derivações**: adaptações de peças, eletrônica (Arduino Mega + nRF24 + L298N + SG90),
  firmware, método de montagem, pintura e documentação por **[seu nome/equipe]**.
- Bibliotecas de software: **RF24 (TMRh20)**, **Servo (Arduino)**.
````
