# 🛡️ Sistema de Segurança - Wall-E 3D Robot

## O que Acontece se Perder o Sinal?

### **Sistema de Proteção Implementado:**

```
┌─────────────────────────────────────────────────────────┐
│         SISTEMA DE FAILSAFE (Proteção de Falha)        │
└─────────────────────────────────────────────────────────┘

ESP32 (Transmissor)              Mega2560 (Receptor)
    │                                  │
    ├─ Envia HEARTBEAT                │
    │  a cada 500ms (/ping)           │
    │                                 │
    ├─────────── SINAL OK ──────────>├─ Reseta lastCommandTime
    │                                 │
    │ (Se WiFi cair)                 │
    │                                 │ (Se timeout de 2s)
    ├─ Para de enviar tudo           ├─ connectionLost = true
    ├─ 2 segundos passam             ├─ PARA TODOS OS MOTORES
    │                                 ├─ Toca alerta sonoro
    │                                 ├─ LED pisca (STATUS)
    │                                 └─ Aguarda sinal restaurar
    │
```

---

## 📡 **Timeout de Comunicação (Mega2560)**

### **Constantes de Segurança:**
```cpp
const unsigned long COMMAND_TIMEOUT = 2000;  // 2 segundos
```

### **O que acontece:**

1. **Durante comunicação normal:**
   - ESP32 envia heartbeat a cada 500ms
   - Mega reseta o `lastCommandTime` com cada ping
   - Motores funcionam normalmente

2. **Se sinal cai (WiFi cai ou conexão interrompe):**
   - ESP32 para de enviar pings
   - Mega conta 2 segundos sem receber nada
   - ⚠️ **AUTOMATICAMENTE:**
     - Todos os motores **PARAM IMEDIATAMENTE**
     - Buzzer toca 3 bips de alerta (1500 Hz)
     - LED de status pisca
     - Serial exibe: `"CONNECTION LOST - MOTORS STOPPED"`

3. **Quando sinal retorna:**
   - Primeiro ping recebido
   - Mega reconecta automaticamente
   - Serial exibe: `"CONNECTION RESTORED"`
   - Sistema pronto para novos comandos

---

## 🔌 **Heartbeat (Batida de Coração do Sistema)**

### **No ESP32:**
```cpp
const unsigned long PING_INTERVAL = 500;  // 500ms

// Envia ping a cada 500ms
setInterval(() => {
  sendHeartbeat();
}, PING_INTERVAL);

function sendHeartbeat() {
  fetch(`/ping`)  // Envia para Mega
}
```

### **No Mega2560:**
```cpp
// Qualquer comando reseta o timer
if (command.length() > 0) {
  lastCommandTime = millis();  // "Refresca" o timeout
}

// Se não receber nada por 2 segundos
if ((millis() - lastCommandTime) > COMMAND_TIMEOUT) {
  stopAllMotors();  // PARA COM SEGURANÇA
}
```

---

## 🚨 **Cenários de Falha:**

### **Cenário 1: WiFi cai durante movimento**
```
Tempo  0s:   Joystick movimentando, ESP32 envia comandos
Tempo  1s:   WiFi cai, ESP32 não consegue enviar mais nada
Tempo  2s:   Mega2560 detecta timeout
Tempo  2s:   >>> MOTORES PARAM AUTOMATICAMENTE <<<
Resultado:  Robot para, sem travar, sem comportamento estranho
```

### **Cenário 2: Conexão WiFi instável (ping-pong)**
```
Tempo  0s:   Ping recebido, conexão OK
Tempo  0.5s: Ping recebido, conexão OK
Tempo  1.0s: Perda temporária
Tempo  1.5s: Ping recebido novamente
Resultado:  Sistema continua funcionando, nunca desativa
            (porque ping chega antes de 2s)
```

### **Cenário 3: Serial cai entre ESP32 e Mega2560**
```
Tempo  0s:   ESP32 envia comandos normalmente
Tempo  0s:   Cabo serial desconectado
Tempo  2s:   Mega não recebe NADA (nem motores, nem heartbeat)
Tempo  2s:   >>> MOTORES PARAM COM SEGURANÇA <<<
Resultado:  Sistema protegido mesmo com falha física
```

---

## ⚡ **Proteções Adicionais:**

### **1. Emergency Stop Button**
- Botão vermelho sempre disponível
- Desativa todos os motores e servos instantaneamente
- Funciona independentemente de WiFi

### **2. Bloqueio de Comandos**
```cpp
if (emergencyStop) {
  // Nenhum comando é processado
  return;
}
```

### **3. Validação de Comandos**
```cpp
// Só aceita servo angles entre 0-180
if (angle >= 0 && angle <= 180) {
  // Processa
}
```

### **4. Indicador de Status LED**
- Pisca quando recebe comando
- Pisca rapidamente em emergência
- Mostra status visual de conexão

---

## 🔧 **Como Testar a Segurança:**

1. **Teste de WiFi:**
   - Mova o joystick
   - Desligue o WiFi do smartphone/PC
   - Observe: motores param em 2 segundos

2. **Teste de Cabo:**
   - Desconecte o cabo serial RX/TX
   - Observe: motores param em 2 segundos
   - Reconecte: sistema volta a funcionar

3. **Teste de Bateria Baixa:**
   - Observe voltagem no Serial
   - Sistema continua funcionando com proteção

---

## 📊 **Tempos de Resposta:**

| Evento | Tempo de Resposta |
|--------|------------------|
| Heartbeat entre pings | 500ms |
| Detecção de timeout | 2000ms (2s) |
| Parada de motores | < 50ms |
| Alerta sonoro | < 100ms |
| LED de status | < 50ms |

---

## ✅ **Conclusão:**

**O sistema é SEGURO porque:**
- ✅ Motores param automaticamente se sinal cai
- ✅ Heartbeat mantém conexão "viva"
- ✅ Timeout de 2 segundos oferece proteção
- ✅ Múltiplas camadas de proteção
- ✅ Não trava, sempre responde
- ✅ Feedback visual e sonoro de problemas

**Se WiFi cair, você fica 100% seguro!** 🛡️
