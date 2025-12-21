# Conversor de Nível Lógico Bidirecional - 4 Canais

## Por Que Usar o Conversor?

O **ESP32 funciona com 3.3V** e o **Arduino Mega2560 com 5V**. Ao conectar os pinos diretamente:
- Os pinos RX do ESP32 (3.3V) receberão sinais de 5V do Mega, causando dano permanente
- O Mega2560 consegue suportar 3.3V, mas não é ideal

**Solução:** Usar um conversor de nível lógico bidirecional para adaptar os sinais com segurança.

---

## Especificações do Conversor

| Característica | Valor |
|---|---|
| **Tipo** | Conversor Bidirecional (I2C/Serial) |
| **Canais** | 4 canais |
| **Tensão Baixa (LV)** | 3.3V (ESP32) |
| **Tensão Alta (HV)** | 5V (Mega2560) |
| **Comunicação** | UART/Serial ou I2C |
| **Velocidade** | Suporta até 115200 baud |

---

## Conexões com o Conversor

```
┌─────────────┐      ┌──────────────────────┐      ┌──────────────┐
│   ESP32     │      │ Conversor Nível      │      │  Mega2560    │
│ (3.3V Logic)│      │  Lógico Bidirecional │      │ (5V Logic)   │
│             │      │                      │      │              │
│             │      │    LV        HV      │      │              │
│   GPIO 21   ├──────┤ LV1 ←→ HV1 ──────────┼──────┤ Pino 18 (RX1)│
│    (TX)     │      │                      │      │              │
│             │      │                      │      │              │
│   GPIO 20   ├──────┤ LV2 ←→ HV2 ──────────┼──────┤ Pino 19 (TX1)│
│    (RX)     │      │                      │      │              │
│             │      │                      │      │              │
│    3.3V     ├──────┤ GND ←→ GND ──────────┼──────┤ GND          │
│             │      │                      │      │              │
│    GND      ├──────┤ GND                  │      │              │
│             │      │                      │      │              │
└─────────────┘      │    +5V ──────────────┼──────┤ +5V          │
                     │                      │      │              │
                     └──────────────────────┘      └──────────────┘
```

---

## Pinagem Detalhada do Conversor (Tipo I2C/Serial)

### Lado ESP32 (LV - 3.3V):
```
Pino LV1    → ESP32 GPIO 21 (TX)
Pino LV2    → ESP32 GPIO 20 (RX)
Pino GND(LV)→ ESP32 GND
Pino LV+    → NÃO CONECTAR (ou ligar a GND se não tiver linha de 3.3V)
```

### Lado Mega2560 (HV - 5V):
```
Pino HV1    → Mega2560 Pino 18 (RX1)
Pino HV2    → Mega2560 Pino 19 (TX1)
Pino GND(HV)→ Mega2560 GND
Pino HV+    → Mega2560 +5V
```

### Alimentação do Conversor:
```
GND (comum) → ESP32 GND + Mega2560 GND
LV+         → (Deixar desconectado - usa GND como referência)
HV+         → Mega2560 +5V (obrigatório!)
```

---

## Diagrama de Conexão Completa

```
ESP32                  Conversor              Mega2560
┌─────────────────┐   ┌──────────────┐    ┌─────────────────┐
│                 │   │              │    │                 │
│ GPIO 21 (TX) ───┼───┼─ LV1  ←→ HV1 ┼────┼─→ Pino 18 (RX1)│
│                 │   │              │    │                 │
│ GPIO 20 (RX) ←──┼───┼─ LV2  ←→ HV2 ┼────┼─← Pino 19 (TX1)│
│                 │   │              │    │                 │
│ GND ────────────┼───┼─ GND  ←→ GND ┼────┼─ GND            │
│                 │   │              │    │                 │
└─────────────────┘   │              │    │ +5V (Fonte)     │
                      │ LV+  (não    │    │                 │
                      │  conectar)   │    └─────────────────┘
                      │              │
                      │ HV+  ←────────┼─────┤ +5V
                      │              │
                      └──────────────┘
```

---

## Passo a Passo de Instalação Física

### **Materiais Necessários:**
- 1x Conversor de Nível Lógico Bidirecional 4 Canais
- Cabos jumper macho-macho de qualidade
- Fonte de alimentação 5V para o Mega2560

### **Procedimento:**

1. **Desconecte todos os componentes da alimentação** (desplugue ESP32 e Mega2560)

2. **Conecte o GND comum primeiro:**
   - ESP32 GND → Conversor GND (lado LV)
   - Mega2560 GND → Conversor GND (lado HV)
   - ESP32 GND → Mega2560 GND (em paralelo com o conversor)

3. **Conecte a alimentação HV:**
   - Mega2560 +5V → Conversor HV+ (obrigatório para o conversor funcionar!)

4. **Conecte os sinais (UART):**
   - ESP32 GPIO 21 → Conversor LV1 → Conversor HV1 → Mega2560 Pino 18
   - ESP32 GPIO 20 → Conversor LV2 → Conversor HV2 → Mega2560 Pino 19

5. **Deixe LV+ do conversor desconectado** (ele usa GND como referência)

6. **Verifique todas as conexões** antes de alimentar

---

## Teste de Funcionamento

### Sem o conversor (CUIDADO - pode danificar ESP32!):
```
ESP32 GPIO 21 (TX)  → direto em Mega2560 Pino 18 (RX1)  ❌ RISCO 5V em 3.3V
ESP32 GPIO 20 (RX)  → direto em Mega2560 Pino 19 (TX1)  ✓ Seguro (3.3V aguenta 5V entrada)
```

### Com o conversor (SEGURO):
```
ESP32 GPIO 21 (TX)  → Conversor LV1 → HV1 → Mega2560 Pino 18  ✓ Seguro
ESP32 GPIO 20 (RX)  → Conversor LV2 → HV2 → Mega2560 Pino 19  ✓ Seguro
```

---

## Verificação no Código

Os códigos atuais **não precisam de modificação**, pois o conversor é transparente (fica entre os pinos). Apenas certifique-se que:

### ESP32:
```cpp
RoboSerial.begin(115200, SERIAL_8N1, 20, 21);  // ← Correto
```

### Mega2560:
```cpp
Serial1.begin(115200);  // ← Correto (usa pinos 18 e 19 por padrão)
```

---

## Troubleshooting

| Problema | Causa | Solução |
|----------|-------|---------|
| Nenhum sinal recebido | HV+ não conectado | Conectar +5V do Mega ao HV+ do conversor |
| Comunicação intermitente | Cabo ruim | Testar com cabos jumper novos |
| Apenas um sentido funciona | Conversor invertido | Verificar qual lado é LV e qual é HV |
| ESP32 não liga | Queimado pela tensão | Verificar com multímetro se há >3.3V nos pinos |
| Mega não recebe dados | Conexões invertidas | Inverter TX e RX no conversor |

---

## Checklist de Conexão

- [ ] Conversor recebeu +5V no pino HV+
- [ ] GND está conectado em todos os 3 pontos (ESP32, Conversor, Mega)
- [ ] GPIO 21 → LV1 → HV1 → Pino 18
- [ ] GPIO 20 → LV2 → HV2 → Pino 19
- [ ] Todos os cabos estão firmes
- [ ] Não há cabos soltos tocando outros componentes
- [ ] ESP32 e Mega foram desconectados antes da montagem
- [ ] Testou a comunicação com ambos os boards

---

## Referência Rápida

| Conexão | De | Para |
|---------|-----|------|
| UART TX | ESP32 GPIO 21 | Conversor LV1 |
| UART RX | ESP32 GPIO 20 | Conversor LV2 |
| UART RX | Mega2560 Pino 18 | Conversor HV1 |
| UART TX | Mega2560 Pino 19 | Conversor HV2 |
| Alimentação HV | Mega2560 +5V | Conversor HV+ |
| GND Comum | ESP32 + Mega + Conversor | Todos GND |

---

**Nota Importante:** O conversor bidirecional permite comunicação em ambos os sentidos simultaneamente, garantindo que o ESP32 não seja danificado por sinais de 5V vindos do Mega2560. ✅
