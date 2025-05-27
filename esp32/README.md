# VansControl ESP32 - Configuração v2.1

## Bibliotecas Necessárias

Para o código funcionar corretamente, você precisa instalar as seguintes bibliotecas no Arduino IDE:

### 1. Bibliotecas Padrão (já incluídas)
- WiFi
- SPI
- HTTPClient

### 2. Bibliotecas Externas (instalar via Library Manager)

#### MFRC522 (para RFID)
1. Abra o Arduino IDE
2. Vá em **Sketch > Include Library > Manage Libraries**
3. Procure por "MFRC522"
4. Instale a biblioteca **MFRC522 by GithubCommunity**

#### WebSocketsClient_Generic (para comunicação em tempo real)
1. No Library Manager, procure por "WebSocketsClient_Generic"
2. Instale a biblioteca **WebSocketsClient_Generic by Khoi Hoang**
3. **OU** baixe diretamente do GitHub: https://github.com/khoih-prog/WebSockets_Generic

#### ArduinoJson (para manipulação JSON)
1. No Library Manager, procure por "ArduinoJson"
2. Instale a biblioteca **ArduinoJson by Benoit Blanchon**
3. **Versão recomendada:** 6.x ou superior

## Configuração de Hardware

### Conexões RFID (MFRC522)
- **SDA/SS**: Pino 21
- **SCK**: Pino 18
- **MOSI**: Pino 23
- **MISO**: Pino 19
- **IRQ**: Não conectado
- **GND**: GND
- **RST**: Pino 22
- **3.3V**: 3.3V

### LEDs
- **LED Verde**: Pino 2 + Resistor 220Ω → GND
- **LED Vermelho**: Pino 4 + Resistor 220Ω → GND

### Buzzer
- **Buzzer**: Pino 5 → GND

## Configuração de Rede

Antes de fazer upload do código, altere as configurações de rede no arquivo `vansControl.ino`:

```cpp
// Configurações de Rede
const char* ssid = "SEU_WIFI_AQUI";
const char* password = "SUA_SENHA_AQUI";
const char* serverURL = "http://SEU_IP_SERVIDOR:3000";
const char* websocketHost = "SEU_IP_SERVIDOR";
const int websocketPort = 3000;

// ID único do ESP32 (único para cada van)
const String ESP32_ID = "ESP32_VAN_001";
```

## Funcionalidades v2.1

### Modo Normal
- LED vermelho fixo quando aguardando cartão
- Ao detectar cartão RFID, envia para servidor via HTTP
- LED verde = acesso autorizado
- LED vermelho piscando = acesso negado
- **NOVO:** Envia dados via WebSocket para tempo real

### Modo Administrativo
- Ativado remotamente pelo painel admin
- LED verde piscando quando aguardando cartão
- Envia RFID via WebSocket para cadastro
- Timeout automático de 60 segundos
- **NOVO:** Protocolo Socket.IO melhorado
- **NOVO:** Heartbeat automático para manter conexão

### Melhorias v2.1
- **WebSocketsClient_Generic**: Biblioteca mais robusta e estável
- **Socket.IO Protocol**: Suporte completo ao protocolo Socket.IO
- **Auto-reconnect**: Reconexão automática em caso de queda
- **Heartbeat**: Mantém conexão WebSocket ativa
- **Logs melhorados**: Prefixo [WSc] para mensagens WebSocket
- **Status detalhado**: Envio de informações completas do ESP32

## Monitoramento Serial

### Mensagens de Inicialização
```
=== VansControl ESP32 v2.1 ===
Inicializando sistema...
Usando WebSocketsClient_Generic
RFID RC522 inicializado
ESP32 ID: ESP32_VAN_001
```

### Mensagens WebSocket
```
[WSc] WebSocket Conectado: /socket.io/?EIO=4&transport=websocket
[WSc] Identificação enviada
[WSc] Evento: esp32Command
[WSc] Comando recebido: startRFIDReading
[WSc] Status enviado: admin_mode_active
```

### Mensagens RFID
```
RFID detectado: A1B2C3D4
Modo: Administrativo
[WSc] Enviando RFID via WebSocket para admin...
[WSc] RFID enviado para admin com sucesso!
```

## Troubleshooting

### Problema: ESP32 não conecta ao WiFi
- Verifique SSID e senha
- Certifique-se que o WiFi é 2.4GHz (ESP32 não suporta 5GHz)
- Verifique se há caracteres especiais no nome da rede

### Problema: RFID não é detectado
- Verifique as conexões do MFRC522
- Teste com diferentes cartões RFID
- Verifique se o cartão está próximo o suficiente (< 3cm)
- Confirme alimentação 3.3V do sensor

### Problema: WebSocket não conecta
- Verifique se o servidor está rodando na porta 3000
- Confirme o IP do servidor no código
- Teste conectividade: `ping IP_DO_SERVIDOR`
- Verifique firewall/antivírus
- **NOVO:** Verifique logs com prefixo [WSc]

### Problema: Biblioteca WebSocketsClient_Generic não encontrada
**Solução:**
1. Instale via Library Manager: "WebSocketsClient_Generic"
2. **OU** baixe manualmente do GitHub
3. **OU** use a biblioteca alternativa "WebSockets by Markus Sattler"

### Problema: Erro de compilação ArduinoJson
**Solução:**
- Atualize para ArduinoJson v6.x ou superior
- Verifique compatibilidade com ESP32
- Limpe cache do Arduino IDE

### Problema: Modo administrativo não ativa
**Solução:**
- Verifique conexão WebSocket no Serial Monitor
- Confirme que mensagens [WSc] aparecem
- Teste comando manual via console do navegador
- Verifique se ESP32 está registrado no servidor

## Comandos de Debug

### Via Serial Monitor
```cpp
// Adicione no loop() para debug
if (Serial.available()) {
  String cmd = Serial.readString();
  if (cmd == "status") {
    printSystemInfo();
  } else if (cmd == "admin") {
    entrarModoAdministrativo();
  } else if (cmd == "normal") {
    sairModoAdministrativo();
  }
}
```

### Via WebSocket (console do navegador)
```javascript
// Enviar comando para ESP32
socket.emit('esp32Command', {
  command: 'getStatus',
  esp32Id: 'ESP32_VAN_001'
});

// Ativar modo admin
socket.emit('esp32Command', {
  command: 'startRFIDReading'
});
```

## Especificações Técnicas

- **Versão:** 2.1
- **Biblioteca WebSocket:** WebSocketsClient_Generic
- **Protocolo:** Socket.IO v4
- **Timeout Admin:** 60 segundos
- **Heartbeat:** 15 segundos
- **Reconexão:** 5 segundos
- **Debounce RFID:** 2 segundos
- **Baud Rate:** 115200

## Compatibilidade

- **ESP32:** Todas as variantes
- **Arduino IDE:** 1.8.x ou superior
- **ESP32 Core:** 2.x ou superior
- **Socket.IO:** v4.x
- **Node.js Server:** v14.x ou superior 