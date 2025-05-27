# VansControl ESP32 - Configuração

## Bibliotecas Necessárias

Para o código funcionar corretamente, você precisa instalar as seguintes bibliotecas no Arduino IDE:

### 1. Bibliotecas Padrão (já incluídas)
- WiFi
- SPI
- HTTPClient
- ArduinoJson

### 2. Bibliotecas Externas (instalar via Library Manager)

#### MFRC522 (para RFID)
1. Abra o Arduino IDE
2. Vá em **Sketch > Include Library > Manage Libraries**
3. Procure por "MFRC522"
4. Instale a biblioteca **MFRC522 by GithubCommunity**

#### WebSocketsClient (para comunicação em tempo real)
1. No Library Manager, procure por "WebSockets"
2. Instale a biblioteca **WebSockets by Markus Sattler**

#### ArduinoJson (para manipulação JSON)
1. No Library Manager, procure por "ArduinoJson"
2. Instale a biblioteca **ArduinoJson by Benoit Blanchon**

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
```

## Funcionalidades

### Modo Normal
- LED vermelho fixo quando aguardando cartão
- Ao detectar cartão RFID, envia para servidor via HTTP
- LED verde = acesso autorizado
- LED vermelho piscando = acesso negado

### Modo Administrativo
- Ativado remotamente pelo painel admin
- LED verde piscando quando aguardando cartão
- Envia RFID via WebSocket para cadastro
- Timeout automático de 60 segundos

## Troubleshooting

### Problema: ESP32 não conecta ao WiFi
- Verifique SSID e senha
- Certifique-se que o WiFi é 2.4GHz (ESP32 não suporta 5GHz)

### Problema: RFID não é detectado
- Verifique as conexões do MFRC522
- Teste com diferentes cartões RFID
- Verifique se o cartão está próximo o suficiente

### Problema: WebSocket não conecta
- Verifique se o servidor está rodando
- Confirme o IP e porta do servidor
- Verifique firewall/antivírus

## Monitoramento

Use o Serial Monitor (115200 baud) para acompanhar:
- Status de conexão WiFi
- Status de conexão WebSocket
- Leituras RFID
- Comandos recebidos
- Modo administrativo ativo/inativo 