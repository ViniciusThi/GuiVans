#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WebSocketsClient_Generic.h>

// Configurações de Hardware
#define SS_PIN 21
#define RST_PIN 22
#define LED_VERDE 2
#define LED_VERMELHO 4
#define BUZZER 5

// Memória RTC para sobreviver a resets
RTC_DATA_ATTR int rtcResetCount = 0;
RTC_DATA_ATTR int rtcWiFiFailCount = 0;
RTC_DATA_ATTR char rtcLastAdminCommand[50] = ""; // Armazenar último comando administrativo

// Configurações de Rede
const char* ssid = "Thi";
const char* password = "thi08012";
const char* serverURL = "http://54.233.146.9:3000";
const char* websocketHost = "54.233.146.9";
const int websocketPort = 3000;
const char* websocketPath = "/socket.io/?EIO=4&transport=websocket&device=esp32&id=ESP32";

// Timeouts e constantes
#define WIFI_CONNECT_TIMEOUT 20000    // 20 segundos para timeout de conexão WiFi
#define WIFI_RETRY_DELAY 5000         // 5 segundos entre tentativas de conexão WiFi
#define MAX_WIFI_ATTEMPTS 7           // Número máximo de tentativas antes de reiniciar (aumentado para 7)
#define MAX_RESET_COUNT 5             // Número máximo de resets consecutivos
#define SAFE_MODE_DURATION 300000     // 5 minutos em modo seguro

// Parâmetros de conexão - variáveis para ajuste dinâmico
unsigned long PING_INTERVAL = 2000;   // 2 segundos entre pings (reduzido de 3s para 2s)
const unsigned long RECONNECT_DELAY = 1000;    // 1 segundo entre tentativas de reconexão (reduzido para 1s)
const int MAX_RECONNECT_ATTEMPTS = 30;         // Máximo de tentativas antes de reiniciar WiFi (aumentado para 30)
const unsigned long WIFI_RECONNECT_TIMEOUT = 10000; // 10 segundos entre tentativas de reconexão WiFi (reduzido para 10s)
const unsigned long IDENTIFICACAO_INTERVAL = 15000; // 15 segundos entre envios de identificação (reduzido para 15s)

// ID único do ESP32
const String ESP32_ID = "ESP32";

// Objetos
MFRC522 rfid(SS_PIN, RST_PIN);
HTTPClient http;
WebSocketsClient webSocket;

// Variáveis de controle
unsigned long lastCardRead = 0;
const unsigned long DEBOUNCE_TIME = 2000;
bool wifiConnected = false;
bool websocketConnected = false;
bool adminReadingMode = false;
unsigned long adminModeStartTime = 0;
const unsigned long ADMIN_MODE_TIMEOUT = 120000; // Aumentado para 2 minutos
bool safeMode = false;
unsigned long safeModeStartTime = 0;
String adminModeType = ""; // Tipo de modo administrativo (generic, cadastroAluno, etc)

// Variáveis para monitoramento de conexão
unsigned long lastPingTime = 0;
unsigned long lastConnectAttempt = 0;
int reconnectAttempts = 0;  // Contador de tentativas de reconexão
unsigned long lastWifiReconnect = 0;
unsigned long lastIdentificacao = 0;
unsigned long lastWifiCheck = 0;               // Última verificação do WiFi
const unsigned long WIFI_CHECK_INTERVAL = 3000; // Verificar WiFi a cada 3 segundos (reduzido de 5s)

// Adicionar contador de resets para evitar loop infinito
int totalResets = 0;
const int MAX_TOTAL_RESETS = 5;  // Número máximo de resets antes de entrar em modo de economia

// Declarações de função
void configurarWebSocket();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void enviarIdentificacaoESP32();
void processarMensagemWebSocket(String message);
void processarComandoESP32(JsonObject data);
void entrarModoAdministrativo(String tipo = "generic");
void sairModoAdministrativo();
void enviarRFIDViaWebSocket(String rfidTag);
void conectarWiFi();
String lerUIDCartao();
void processarRFID(String rfidTag);
void processarRespostaServidor(String response);
void tocarBuzzer(int vezes, int duracao);
void piscarLed(int led, int vezes, int duracao);
void verificarConexaoWebSocket();
void enviarPingWebSocket();
void reiniciarWebSocket();
void reiniciarWiFi();
bool tentarConectarWiFi();
void diagnosticarProblemaWiFi();
void enterSafeMode();
void salvarUltimoComandoAdmin(const char* comando);
bool verificarComandoAdminPersistente();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== VansControl ESP32 Versão 2.3 ===");
  Serial.printf("Reset count: %d, WiFi fail count: %d\n", rtcResetCount, rtcWiFiFailCount);
  
  // Incrementar contador de resets
  rtcResetCount++;
  
  // Configurar pinos
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  digitalWrite(LED_VERMELHO, HIGH);
  digitalWrite(LED_VERDE, LOW);
  
  // Inicializar SPI e RFID
  SPI.begin();
  rfid.PCD_Init();
  
  Serial.println("RFID RC522 inicializado");
  
  // Verificar se devemos entrar em modo seguro
  if (rtcResetCount > MAX_RESET_COUNT || rtcWiFiFailCount > MAX_RESET_COUNT) {
    enterSafeMode();
    return;
  }
  
  // Verificar se temos um comando administrativo pendente
  if (verificarComandoAdminPersistente()) {
    Serial.println("Comando administrativo persistente detectado. Priorizando conexão...");
  }
  
  // Inicialização normal do WiFi
  Serial.println("Iniciando WiFi normalmente...");
  // Evitar chamadas que podem causar o erro netstack
  WiFi.persistent(false);
  WiFi.disconnect(true, true);  // Desconectar e limpar configurações
  delay(1000);
  
  // Inicializar WiFi com configurações básicas
  WiFi.mode(WIFI_STA);
  // Não configurar callbacks avançados aqui
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  
  // Conectar WiFi sem métodos avançados
  WiFi.begin(ssid, password);
  
  // Esperar conexão com timeout
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado com sucesso!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    
    // Resetar contadores de erro quando temos sucesso
    rtcResetCount = 0;
    rtcWiFiFailCount = 0;
    
    // Piscar LED para indicar sucesso
    piscarLed(LED_VERDE, 3, 200);
    
    // Configurar WebSocket
    configurarWebSocket();
  } else {
    Serial.println("\nFalha na conexão WiFi inicial");
    rtcWiFiFailCount++; // Incrementar contador de falhas WiFi
  }
  
  Serial.println("Sistema pronto!");
  tocarBuzzer(2, 200);
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Verificar se estamos em modo seguro
  if (safeMode) {
    // Indicação visual de modo seguro (LED vermelho e verde alternando)
    if ((currentMillis / 500) % 2 == 0) {
      digitalWrite(LED_VERMELHO, HIGH);
      digitalWrite(LED_VERDE, LOW);
    } else {
      digitalWrite(LED_VERMELHO, LOW);
      digitalWrite(LED_VERDE, HIGH);
    }
    
    // Verificar se o tempo do modo seguro terminou
    if (currentMillis - safeModeStartTime > SAFE_MODE_DURATION) {
      Serial.println("Saindo do modo seguro, reiniciando ESP32...");
      // Resetar contadores antes de reiniciar
      rtcResetCount = 0;
      rtcWiFiFailCount = 0;
      delay(1000);
      ESP.restart();
    }
    
    // No modo seguro, ainda tentamos manter WiFi
    if (WiFi.status() != WL_CONNECTED && (currentMillis - lastWifiReconnect > 60000)) {
      lastWifiReconnect = currentMillis;
      Serial.println("Tentando reconectar WiFi em modo seguro...");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
    }
    
    // Leitura de RFID ainda funciona em modo seguro
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      String rfidTag = lerUIDCartao();
      Serial.print("RFID detectado em modo seguro: ");
      Serial.println(rfidTag);
      
      // Piscar LED e tocar buzzer para indicar leitura
      piscarLed(LED_VERDE, 2, 200);
      tocarBuzzer(1, 200);
      
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();
    }
    
    delay(100);
    return;
  }
  
  // Verificar WiFi a cada intervalo definido
  if (currentMillis - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = currentMillis;
    
    // Verificar estado atual do WiFi
    int wifiStatus = WiFi.status();
    
    // Se não estiver conectado, atualizar status e tentar reconectar se necessário
    if (wifiStatus != WL_CONNECTED && wifiConnected) {
      Serial.println("WiFi desconectado! Status: " + String(wifiStatus));
      wifiConnected = false;
      websocketConnected = false;
      
      // Mostrar status de erro no LED
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
    }
    
    // Se estava desconectado e agora está conectado, atualizar status
    if (wifiStatus == WL_CONNECTED && !wifiConnected) {
      Serial.println("WiFi reconectado automaticamente!");
      wifiConnected = true;
      
      // Piscar LED para indicar reconexão
      piscarLed(LED_VERDE, 2, 200);
      
      // Tentar reconectar WebSocket
      if (!websocketConnected) {
        configurarWebSocket();
      }
    }
  }
  
  // Verificar e gerenciar conexões
  verificarConexaoWebSocket();
  
  // Processar WebSocket apenas se WiFi estiver conectado
  if (wifiConnected) {
    webSocket.loop();
    
    // Reenviar identificação periodicamente se estiver conectado
    if (websocketConnected && (currentMillis - lastIdentificacao > IDENTIFICACAO_INTERVAL)) {
      enviarIdentificacaoESP32();
      lastIdentificacao = currentMillis;
    }
  }
  
  // Verificar timeout do modo admin
  if (adminReadingMode && (currentMillis - adminModeStartTime > ADMIN_MODE_TIMEOUT)) {
    Serial.println("Timeout do modo administrativo");
    sairModoAdministrativo();
  }
  
  // Verificar cartão RFID
  String uid = lerUIDCartao();
  if (uid.length() > 0) {
    unsigned long now = millis();
    if (now - lastCardRead > DEBOUNCE_TIME) {
      lastCardRead = now;
      processarRFID(uid);
    } else {
      Serial.println("Debounce: ignorando leitura repetida");
    }
  }
  
  // Indicação visual baseada no status
  if (adminReadingMode) {
    // LED verde piscando no modo admin
    static unsigned long lastBlink = 0;
    if (currentMillis - lastBlink > 500) {
      digitalWrite(LED_VERDE, !digitalRead(LED_VERDE));
      lastBlink = currentMillis;
    }
    digitalWrite(LED_VERMELHO, LOW);
  } else {
    // LED de acordo com o status das conexões
    if (websocketConnected) {
      // Verde fixo indica WebSocket conectado
      digitalWrite(LED_VERDE, HIGH);
      digitalWrite(LED_VERMELHO, LOW);
    } else if (wifiConnected) {
      // Verde piscando lento indica WiFi conectado mas WebSocket desconectado
      static unsigned long lastWsBlinkTime = 0;
      if (currentMillis - lastWsBlinkTime > 1000) {
        digitalWrite(LED_VERDE, !digitalRead(LED_VERDE));
        lastWsBlinkTime = currentMillis;
      }
      digitalWrite(LED_VERMELHO, LOW);
    } else {
      // Vermelho fixo indica problema de conexão
      digitalWrite(LED_VERMELHO, HIGH);
      digitalWrite(LED_VERDE, LOW);
    }
  }
  delay(50); // Reduzir delay para verificar a conexão mais frequentemente
}

bool tentarConectarWiFi() {
  Serial.println("Tentando estabelecer conexão WiFi com abordagem simplificada...");
  
  // Abordagem simplificada para evitar o erro netstack
  WiFi.disconnect(true, true);
  delay(1000);
  
  // Evitar funções que podem causar o erro netstack
  WiFi.mode(WIFI_STA);
  
  // Evitar callbacks e configurações complexas
  // WiFi.setSleep(false);  // Comentado para evitar o erro
  
  // Tentar conexão básica
  Serial.println("Iniciando conexão WiFi básica...");
  WiFi.begin(ssid, password);
  
  // Esperar até 15 segundos
  for (int i = 0; i < 30; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConectado com método básico!");
      return true;
    }
    delay(500);
    Serial.print(".");
  }
  
  // Se falhou, tentar abordagem alternativa (sem callbacks)
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nTentando abordagem alternativa...");
    WiFi.disconnect(true, true);
    delay(1000);
    WiFi.mode(WIFI_STA);
    
    // Não usar configurações avançadas
    WiFi.begin(ssid, password);
    
    // Esperar mais tempo
    for (int i = 0; i < 40; i++) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConectado com abordagem alternativa!");
        return true;
      }
      delay(500);
      Serial.print(".");
    }
  }
  
  // Se chegou aqui, todas as tentativas falharam
  Serial.println("\nFalha em todas as tentativas de conexão WiFi");
  rtcWiFiFailCount++; // Incrementar contador de falhas WiFi
  return false;
}

void conectarWiFi() {
  Serial.print("Conectando WiFi");
  
  // Desconectar qualquer conexão existente e resetar configurações
  WiFi.disconnect(true);
  delay(1000);
  
  // Configurações WiFi mais básicas e estáveis
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Desativar modo de economia de energia para conexão mais estável
  
  // Não usar setTxPower que pode causar problemas em alguns ESP32
  // WiFi.setTxPower(WIFI_POWER_19_5dBm); 
  
  // Tentar abordagem robusta primeiro
  if (tentarConectarWiFi()) {
    Serial.println("WiFi conectado com sucesso!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    
    // Feedback visual
    piscarLed(LED_VERDE, 3, 200);
    return;
  }
  
  // Se o método robusto falhar, tente o método tradicional
  Serial.println("Tentando método tradicional...");
  
  // Iniciar conexão
  WiFi.begin(ssid, password);
  
  // Aguardar conexão com timeout
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("WiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    
    // Feedback visual
    piscarLed(LED_VERDE, 3, 200);
  } else {
    Serial.println();
    Serial.println("Falha WiFi!");
    Serial.print("Status: ");
    Serial.println(WiFi.status());
    
    // Feedback visual
    piscarLed(LED_VERMELHO, 5, 100);
  }
}

void reiniciarWiFi() {
  Serial.println("Reiniciando conexão WiFi...");
  
  // Desconectar interfaces
  webSocket.disconnect();
  WiFi.disconnect(true, true);
  
  // Aguardar para garantir que a interface esteja desligada
  delay(1000);
  
  // Reconectar usando o método de tentativa robusta
  if (tentarConectarWiFi()) {
    configurarWebSocket();
  } else {
    // Se o número de resets exceder o limite, entrar em modo de economia
    if (totalResets >= MAX_TOTAL_RESETS) {
      Serial.println("Muitos resets detectados. Entrando em modo de recuperação por 1 minuto...");
      // Indicar com LED e esperar
      piscarLed(LED_VERMELHO, 10, 100);
      delay(60000); // Esperar 1 minuto antes de tentar novamente
      totalResets = 0; // Resetar contador
    }
    
    // Reiniciar ESP32 como último recurso
    Serial.println("Falha crítica na conexão WiFi. Reiniciando ESP32...");
    delay(1000);
    ESP.restart();
  }
}

void configurarWebSocket() {
  Serial.println("Configurando WebSocket...");
  
  // Limpar estado anterior do WebSocket
  webSocket.disconnect();
  delay(300);
  
  // Verificar se WiFi está conectado
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi não conectado. Impossível configurar WebSocket.");
    return;
  }
  
  // Construir caminho com o ID único e informações adicionais
  String path = String(websocketPath);
  path.replace("ESP32", ESP32_ID);
  
  // Adicionar parâmetros de qualidade de conexão
  path += "&rssi=";
  path += WiFi.RSSI();
  path += "&reset=";
  path += rtcResetCount;
  path += "&uptime=";
  path += millis() / 1000;
  path += "&random=";
  path += random(1000, 9999); // Adicionar número aleatório para evitar cache
  
  // Configurações mais robustas para o WebSocket
  webSocket.setExtraHeaders("User-Agent: TransControl ESP32/2.3\r\nOrigin: http://esp32.local\r\nConnection: keep-alive");
  webSocket.setReconnectInterval(200);  // 200ms (reduzido de 500ms) - Reconectar muito mais rápido
  webSocket.enableHeartbeat(10000, 1000, 5); // 10 segundos entre heartbeats (1s timeout, 5 tentativas) - Ainda mais agressivo
  
  // Usar Socket.IO v4 com configurações para garantir estabilidade
  Serial.printf("Conectando ao servidor WebSocket: %s:%d%s\n", websocketHost, websocketPort, path.c_str());
  
  // Iniciar conexão WebSocket com protocolo Socket.IO
  webSocket.beginSocketIO(websocketHost, websocketPort, path.c_str());
  Serial.println("Conexão WebSocket iniciada");
  
  // Configurar handler de eventos
  webSocket.onEvent(webSocketEvent);
  
  // Atualizar variáveis de controle
  lastConnectAttempt = millis();
  lastPingTime = 0; // Forçar envio de ping logo após conexão
  lastIdentificacao = 0; // Forçar envio de identificação
  reconnectAttempts = 0;
  
  Serial.println("WebSocket configurado");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Desconectado");
      websocketConnected = false;
      
      // Feedback visual
      digitalWrite(LED_VERMELHO, HIGH);
      digitalWrite(LED_VERDE, LOW);
      
      // Tentar reconectar imediatamente se a desconexão não foi iniciada pelo próprio ESP32
      if (millis() - lastConnectAttempt > 5000) {
        Serial.println("Tentando reconexão imediata após desconexão não planejada");
        lastConnectAttempt = millis();
        // Não reconectar imediatamente, permitir que o mecanismo de reconexão automática funcione
        reconnectAttempts++;
      }
      break;
      
    case WStype_CONNECTED:
      Serial.printf("[WSc] Conectado: %s\n", payload);
      websocketConnected = true;
      reconnectAttempts = 0;
      
      // Feedback visual
      piscarLed(LED_VERDE, 3, 200);
      
      // Enviar identificação imediatamente após conexão
      lastIdentificacao = millis();
      enviarIdentificacaoESP32();
      
      // Programar ping imediato para verificar conexão
      lastPingTime = millis() - PING_INTERVAL + 1000; // Enviar ping em 1 segundo
      break;
      
    case WStype_TEXT:
      Serial.printf("[WSc] Mensagem: %s\n", payload);
      processarMensagemWebSocket((char*)payload);
      break;
      
    case WStype_PING:
      Serial.println("[WSc] Ping recebido");
      // O cliente responde automaticamente com pong
      lastPingTime = millis();
      break;
      
    case WStype_PONG:
      Serial.println("[WSc] Pong recebido");
      // Resetar timer de ping quando receber pong
      lastPingTime = millis();
      break;
      
    case WStype_ERROR:
      Serial.printf("[WSc] Erro WebSocket: %s\n", payload);
      // Tentar reconectar em caso de erro
      websocketConnected = false;
      break;
      
    case WStype_BIN:
      Serial.printf("[WSc] Dados binários recebidos (tamanho: %u)\n", length);
      break;
      
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
      Serial.println("[WSc] Início de fragmento");
      break;
      
    case WStype_FRAGMENT:
      Serial.println("[WSc] Fragmento");
      break;
      
    case WStype_FRAGMENT_FIN:
      Serial.println("[WSc] Fim de fragmento");
      break;
      
    default:
      Serial.printf("[WSc] Tipo de evento não tratado: %d\n", type);
      break;
  }
}

void verificarConexaoWebSocket() {
  unsigned long currentMillis = millis();
  
  // Verificar WiFi mais frequentemente
  if (currentMillis - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = currentMillis;
    
    // Verificar estado atual do WiFi
    int wifiStatus = WiFi.status();
    
    // Se não estiver conectado, atualizar status e tentar reconectar se necessário
    if (wifiStatus != WL_CONNECTED && wifiConnected) {
      Serial.println("WiFi desconectado! Status: " + String(wifiStatus));
      wifiConnected = false;
      websocketConnected = false;
      
      // Mostrar status de erro no LED
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
      
      // Tentar reconectar imediatamente no modo admin
      if (adminReadingMode) {
        Serial.println("Reconexão imediata devido ao modo administrativo ativo");
        tentarConectarWiFi();
      }
    }
    
    // Se estava desconectado e agora está conectado, atualizar status
    if (wifiStatus == WL_CONNECTED && !wifiConnected) {
      Serial.println("WiFi reconectado automaticamente!");
      wifiConnected = true;
      
      // Piscar LED para indicar reconexão
      piscarLed(LED_VERDE, 2, 200);
      
      // Tentar reconectar WebSocket imediatamente
      if (!websocketConnected) {
        configurarWebSocket();
      }
    }
  }
  
  // Verificar WiFi
  if (WiFi.status() != WL_CONNECTED) {
    // WiFi desconectado
    if (wifiConnected) {
      // Acabamos de perder a conexão
      Serial.println("Conexão WiFi perdida!");
      wifiConnected = false;
      websocketConnected = false;
      
      // Feedback visual
      digitalWrite(LED_VERMELHO, HIGH);
      digitalWrite(LED_VERDE, LOW);
    }
    
    // Tentar reconectar mais frequentemente se estiver em modo administrativo
    unsigned long reconnectTimeout = adminReadingMode ? 5000 : WIFI_RECONNECT_TIMEOUT;
    
    // Tentar reconectar a cada WIFI_RECONNECT_TIMEOUT
    if (currentMillis - lastWifiReconnect > reconnectTimeout) {
      lastWifiReconnect = currentMillis;
      
      // Abordagem robusta de reconexão
      Serial.println("Tentando reconectar WiFi...");
      if (tentarConectarWiFi()) {
        Serial.println("WiFi reconectado, tentando reconectar WebSocket");
        wifiConnected = true;
        configurarWebSocket();
      } else {
        Serial.println("Falha na reconexão WiFi");
        rtcWiFiFailCount++; // Incrementar contador de falhas persistente
      }
    }
    return;
  }
  
  // WiFi está conectado
  if (!wifiConnected) {
    Serial.println("WiFi reconectado automaticamente!");
    wifiConnected = true;
    
    // Tentar reconectar WebSocket imediatamente
    lastConnectAttempt = currentMillis - RECONNECT_DELAY - 1000; // Forçar tentativa imediata
  }
  
  // Enviar ping periódico para manter a conexão ativa
  if (websocketConnected && (currentMillis - lastPingTime > PING_INTERVAL)) {
    enviarPingWebSocket();
    lastPingTime = currentMillis;
  }
  
  // Tentar reconectar WebSocket se estiver desconectado
  if (!websocketConnected && (currentMillis - lastConnectAttempt > RECONNECT_DELAY)) {
    lastConnectAttempt = currentMillis;
    
    if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
      Serial.println("Tentando reconectar WebSocket...");
      
      // Fechar qualquer conexão pendente
      webSocket.disconnect();
      delay(200); // Reduzido para 200ms
      
      // Construir caminho com o ID único e parâmetros adicionais
      String path = String(websocketPath);
      path.replace("ESP32", ESP32_ID);
      path += "&rssi=";
      path += WiFi.RSSI();
      path += "&reconnect=";
      path += reconnectAttempts;
      path += "&random=";
      path += millis(); // Adicionar timestamp para evitar cache
      path += "&adminMode=";
      path += adminReadingMode ? "1" : "0"; // Indicar se está em modo admin
      
      // Iniciar nova conexão
      webSocket.beginSocketIO(websocketHost, websocketPort, path.c_str());
      Serial.println("Tentativa de reconexão WebSocket iniciada");
      reconnectAttempts++;
      
      // Feedback visual
      piscarLed(LED_VERMELHO, 2, 100);
    } else {
      Serial.println("Muitas tentativas de reconexão. Reiniciando WiFi...");
      reiniciarWiFi();
      reconnectAttempts = 0;
    }
  }
}

void enviarPingWebSocket() {
  if (!websocketConnected) return;
  
  // Enviar pacote de ping usando protocolo Socket.IO
  webSocket.sendTXT("2"); // Código "2" é o ping no Socket.IO v4
  Serial.println("[WSc] Ping enviado");
}

void reiniciarWebSocket() {
  Serial.println("Reiniciando conexão WebSocket...");
  
  webSocket.disconnect();
  delay(500);
  configurarWebSocket();
}

void enviarIdentificacaoESP32() {
  // Verificar se WebSocket está conectado
  if (!websocketConnected) {
    Serial.println("WebSocket não conectado, impossível enviar identificação");
    return;
  }
  
  Serial.println("Enviando identificação ESP32...");
  
  // Criar objeto JSON de identificação no formato Socket.IO
  DynamicJsonDocument doc(512);
  doc[0] = "esp32Identification";
  
  JsonObject data = doc.createNestedObject(1);
  data["esp32Id"] = ESP32_ID;
  data["version"] = "2.3"; // Versão do firmware
  data["ip"] = WiFi.localIP().toString();
  data["rssi"] = WiFi.RSSI();
  data["freeHeap"] = ESP.getFreeHeap();
  data["uptime"] = millis() / 1000;
  data["resetCount"] = rtcResetCount;
  data["wifiFailCount"] = rtcWiFiFailCount;
  data["adminMode"] = adminReadingMode;
  data["adminModeType"] = adminReadingMode ? adminModeType : "";
  data["clientTime"] = String(millis());
  
  // Serializar para envio
  String jsonMessage;
  serializeJson(doc, jsonMessage);
  
  // Prefixo 42 indica evento Socket.IO
  webSocket.sendTXT("42" + jsonMessage);
  
  Serial.println("Identificação ESP32 enviada");
  
  // Indicação visual breve de que o ESP32 está enviando dados
  digitalWrite(LED_VERDE, HIGH);
  delay(50);
  if (!websocketConnected) {
    digitalWrite(LED_VERDE, LOW);
  }
}

void processarMensagemWebSocket(String message) {
  // Tratar ping/pong do Socket.IO - muito importante para manter a conexão
  if (message == "2") {
    // Recebeu ping do Socket.IO (código "2")
    // Responder com pong (código "3")
    webSocket.sendTXT("3");
    Serial.println("Ping respondido automaticamente");
    lastPingTime = millis();
    return;
  } else if (message == "3") {
    // Recebeu pong do Socket.IO (código "3")
    Serial.println("Pong recebido do servidor");
    lastPingTime = millis();
    return;
  } else if (message.startsWith("0")) {
    // Mensagem de abertura de conexão
    Serial.println("Conexão Socket.IO estabelecida");
    // Não precisa responder
    return;
  } else if (message.startsWith("40")) {
    // Mensagem de conexão Socket.IO
    Serial.println("Socket.IO conectado e pronto");
    // Identificar o ESP32 após conexão
    delay(500);
    enviarIdentificacaoESP32();
    return;
  } else if (message.startsWith("41")) {
    // Mensagem de desconexão
    Serial.println("Socket.IO solicitou desconexão");
    return;
  }
  
  // Processar mensagens JSON
  if (message.startsWith("42")) {
    String jsonMessage = message.substring(2);
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, jsonMessage);
    
    if (error) {
      Serial.print("Erro JSON: ");
      Serial.println(error.c_str());
      return;
    }
    
    if (doc.is<JsonArray>() && doc.size() >= 2) {
      String eventName = doc[0];
      JsonObject eventData = doc[1];
      
      Serial.printf("Evento: %s\n", eventName.c_str());
      
      if (eventName == "esp32Command") {
        processarComandoESP32(eventData);
      } else if (eventName == "keepalive") {
        // Responder ao keepalive do servidor imediatamente
        DynamicJsonDocument response(512);
        response[0] = "keepaliveResponse";
        
        JsonObject data = response.createNestedObject(1);
        data["esp32Id"] = ESP32_ID;
        data["timestamp"] = millis();
        data["status"] = "alive";
        data["memory"] = ESP.getFreeHeap();
        data["uptime"] = millis() / 1000;
        data["rssi"] = WiFi.RSSI();
        data["resetCount"] = rtcResetCount;
        data["sequence"] = eventData.containsKey("pingSequence") ? eventData["pingSequence"].as<int>() : 0;
        
        String responseStr;
        serializeJson(response, responseStr);
        webSocket.sendTXT("42" + responseStr);
        
        Serial.println("Keepalive respondido");
        
        // Atualizar última atividade
        lastPingTime = millis();
        
        // Feedback visual breve
        digitalWrite(LED_VERDE, HIGH);
        delay(50);
        if (!websocketConnected) {
          digitalWrite(LED_VERDE, LOW);
        }
      } else if (eventName == "esp32_connected_ack") {
        // Servidor confirmou a conexão - aplicar configurações
        if (eventData.containsKey("config")) {
          JsonObject config = eventData["config"];
          if (config.containsKey("reconnectInterval")) {
            int reconnectMs = config["reconnectInterval"].as<int>();
            webSocket.setReconnectInterval(reconnectMs);
            Serial.printf("Intervalo de reconexão atualizado: %dms\n", reconnectMs);
          }
          if (config.containsKey("pingInterval")) {
            int pingMs = config["pingInterval"].as<int>();
            // Configurar intervalo de ping local
            PING_INTERVAL = pingMs;
            Serial.printf("Intervalo de ping atualizado: %dms\n", pingMs);
          }
        }
        
        // Indicar conexão confirmada pelo servidor
        piscarLed(LED_VERDE, 2, 200);
      }
    }
  } else {
    Serial.print("Mensagem desconhecida: ");
    Serial.println(message);
  }
}

void processarComandoESP32(JsonObject data) {
  String command = data["command"];
  
  Serial.printf("Comando: %s\n", command.c_str());
  
  if (command == "startRFIDReading") {
    // Verificar se há tipo específico de leitura
    String tipo = "generic";
    if (data.containsKey("adminType")) {
      tipo = data["adminType"].as<String>();
    }
    
    // Salvar comando para persistência em caso de reinício
    if (tipo == "cadastroAluno") {
      salvarUltimoComandoAdmin("startRFIDReading_cadastroAluno");
      
      // Priorizar a conexão WiFi para cadastro
      if (!wifiConnected || !websocketConnected) {
        Serial.println("Priorizando reconexão para cadastro de aluno");
        if (!wifiConnected) {
          tentarConectarWiFi();
        }
        if (wifiConnected && !websocketConnected) {
          configurarWebSocket();
        }
      }
    }
    
    Serial.printf("Iniciando modo administrativo: %s\n", tipo.c_str());
    entrarModoAdministrativo(tipo);
    
    // Confirmar que o comando foi recebido
    String jsonMessage = "[\"commandAck\", {\"command\":\"" + command + "\", \"status\":\"received\", \"type\":\"" + tipo + "\", \"timestamp\":" + String(millis()) + "}]";
    webSocket.sendTXT("42" + jsonMessage);
    Serial.println("Confirmação de comando enviada");
  } else if (command == "stopRFIDReading") {
    // Limpar comando persistente
    salvarUltimoComandoAdmin("");
    
    Serial.println("Parando modo administrativo");
    sairModoAdministrativo();
    
    // Confirmar que o comando foi recebido
    String jsonMessage = "[\"commandAck\", {\"command\":\"" + command + "\", \"status\":\"received\", \"timestamp\":" + String(millis()) + "}]";
    webSocket.sendTXT("42" + jsonMessage);
  } else if (command == "reconnect") {
    Serial.println("Comando de reconexão recebido do servidor");
    reiniciarWebSocket();
  } else if (command == "reboot") {
    Serial.println("Comando de reinicialização recebido do servidor");
    // Aguardar para o comando ser enviado completamente
    delay(1000);
    ESP.restart();
  }
}

void entrarModoAdministrativo(String tipo) {
  adminReadingMode = true;
  adminModeStartTime = millis();
  adminModeType = tipo;
  
  Serial.printf("=== MODO ADMINISTRATIVO ATIVADO: %s ===\n", tipo.c_str());
  Serial.println("Aguardando leitura de cartão RFID...");
  
  // Feedback visual específico para cada tipo
  if (tipo == "cadastroAluno") {
    // Mais feedback para cadastro de aluno
    piscarLed(LED_VERDE, 7, 80);
    tocarBuzzer(4, 80);
    
    // Notificar que estamos em modo de cadastro
    String jsonMessage = "[\"esp32Status\", {\"status\":\"ready\", \"mode\":\"cadastroAluno\", \"timestamp\":" + String(millis()) + "}]";
    webSocket.sendTXT("42" + jsonMessage);
  } else {
    // Feedback padrão
  piscarLed(LED_VERDE, 5, 100);
  tocarBuzzer(3, 100);
  }
}

void sairModoAdministrativo() {
  adminReadingMode = false;
  adminModeType = "";
  
  Serial.println("=== MODO ADMINISTRATIVO DESATIVADO ===");
  
  digitalWrite(LED_VERDE, LOW);
  if (websocketConnected) {
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(LED_VERMELHO, LOW);
  } else {
  digitalWrite(LED_VERMELHO, HIGH);
  }
  tocarBuzzer(1, 200);
}

void enviarRFIDViaWebSocket(String rfidTag) {
  // Se estamos em modo de cadastro de aluno, tentar mais agressivamente garantir conexão
  if (adminModeType == "cadastroAluno") {
    // Garantir conexão WiFi
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconectando WiFi para enviar RFID de cadastro");
      tentarConectarWiFi();
      delay(500);
    }
    
    // Tentar reconectar WebSocket se necessário
  if (!websocketConnected) {
      Serial.println("Reconectando WebSocket para enviar RFID de cadastro");
      configurarWebSocket();
      delay(1000);
    }
  }
  
  if (!websocketConnected) {
    Serial.println("WebSocket não conectado, tentando reconectar");
    configurarWebSocket();
    delay(500);
    
    // Tentar enviar mesmo sem confirmação de conexão (se WiFi estiver conectado)
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi não conectado, impossível enviar RFID");
    tocarBuzzer(3, 100);
    return;
    }
  }
  
  Serial.println("Enviando RFID via WebSocket...");
  
  DynamicJsonDocument doc(512);
  doc[0] = "rfidRead";
  
  JsonObject data = doc.createNestedObject(1);
  data["rfidTag"] = rfidTag;
  data["esp32Id"] = ESP32_ID;
  data["source"] = "admin_reading";
  data["timestamp"] = millis();
  data["adminType"] = adminModeType;  // Adicionar tipo de modo administrativo
  
  // Para cadastro de aluno, não desativar o modo após leitura (permitir leituras múltiplas)
  if (adminModeType != "cadastroAluno") {
    data["stopAfterRead"] = true;
  }
  
  String jsonMessage;
  serializeJson(doc, jsonMessage);
  
  // Enviar a mensagem
  if (webSocket.sendTXT("42" + jsonMessage)) {
  Serial.println("RFID enviado para admin!");
  } else {
    Serial.println("Falha ao enviar RFID via WebSocket!");
  }
  
  digitalWrite(LED_VERDE, HIGH);
  digitalWrite(LED_VERMELHO, LOW);
  tocarBuzzer(2, 150);
  
  // Se estiver em modo de cadastro, enviar novamente após um breve intervalo para garantir recebimento
  if (adminModeType == "cadastroAluno") {
    delay(500);
    webSocket.sendTXT("42" + jsonMessage);
    Serial.println("RFID reenviado para garantir recebimento no cadastro");
    delay(500);
    webSocket.sendTXT("42" + jsonMessage);  // Terceira tentativa
    
    // Não sair do modo administrativo após enviar, permitir múltiplas leituras
    // O timeout ainda está ativo para encerrar eventualmente
    adminModeStartTime = millis();  // Reiniciar o timeout
    
    // Manter LEDs indicando que ainda está em modo de leitura
    piscarLed(LED_VERDE, 2, 200);
  } else {
    // Para outros modos, seguir comportamento normal
    delay(1000);
  }
}

String lerUIDCartao() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return "";
  }

  Serial.println("Cartão detectado!");
  
  String uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uidString += "0";
    }
    uidString += String(rfid.uid.uidByte[i], HEX);
  }
  uidString.toUpperCase();
  
  Serial.print("UID do cartão: ");
  Serial.println(uidString);

  // Halt PICC
  rfid.PICC_HaltA();
  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();

  return uidString;
}

void processarRFID(String rfidTag) {
  if (adminReadingMode) {
    enviarRFIDViaWebSocket(rfidTag);
    return;
  }

  Serial.println("Enviando RFID para o servidor: " + rfidTag);

  // Preparar URL
  String url = String(serverURL) + "/api/registros/rfid";
  
  // Preparar JSON
  StaticJsonDocument<200> doc;
  doc["rfidTag"] = rfidTag;
  doc["esp32Id"] = ESP32_ID;
  
  // Adicionar localização se disponível (mock por enquanto)
  JsonObject localizacao = doc.createNestedObject("localizacao");
  localizacao["latitude"] = -22.7468;  // Exemplo
  localizacao["longitude"] = -47.6156;  // Exemplo

  String jsonString;
  serializeJson(doc, jsonString);

  Serial.println("Dados a serem enviados: " + jsonString);

  // Configurar requisição HTTP
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Enviar requisição
  int httpResponseCode = http.POST(jsonString);
  
  Serial.print("Código de resposta HTTP: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Resposta do servidor: " + response);
    
    // Processar resposta
    DynamicJsonDocument respDoc(1024);
    DeserializationError error = deserializeJson(respDoc, response);
    
    if (!error) {
      const char* status = respDoc["status"];
      const char* message = respDoc["message"];
      const char* ledColor = respDoc["ledColor"];
      bool buzzer = respDoc["buzzer"];
      
      Serial.print("Status: ");
      Serial.println(status);
      Serial.print("Mensagem: ");
      Serial.println(message);
      
      // Acionar LED apropriado
      if (strcmp(ledColor, "green") == 0) {
        digitalWrite(LED_VERDE, HIGH);
        digitalWrite(LED_VERMELHO, LOW);
        delay(1000);
        digitalWrite(LED_VERDE, LOW);
      } else {
        digitalWrite(LED_VERMELHO, HIGH);
        digitalWrite(LED_VERDE, LOW);
        delay(1000);
        digitalWrite(LED_VERMELHO, LOW);
      }
      
      // Acionar buzzer se necessário
      if (buzzer) {
        tocarBuzzer(1, 200);
      }
    } else {
      Serial.println("Erro ao processar resposta JSON");
      piscarLed(LED_VERMELHO, 3, 200);
      tocarBuzzer(3, 200);
    }
  } else {
    Serial.print("Erro na requisição HTTP: ");
    Serial.println(httpResponseCode);
    piscarLed(LED_VERMELHO, 3, 200);
    tocarBuzzer(3, 200);
  }
  
  http.end();
}

void processarRespostaServidor(String response) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, response);
  
  String status = doc["status"];
  String ledColor = doc["ledColor"];
  bool buzzer = doc["buzzer"];
  
  Serial.print("Status: ");
  Serial.println(status);
  
  if (ledColor == "green") {
    digitalWrite(LED_VERMELHO, LOW);
    digitalWrite(LED_VERDE, HIGH);
    
    if (buzzer) {
      tocarBuzzer(2, 150);
    }
    
    delay(3000);
  } else if (ledColor == "red") {
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, HIGH);
    
    tocarBuzzer(1, 500);
    
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_VERMELHO, LOW);
      delay(200);
      digitalWrite(LED_VERMELHO, HIGH);
      delay(200);
    }
  }
  
  delay(1000);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, HIGH);
}

void tocarBuzzer(int vezes, int duracao) {
  for (int i = 0; i < vezes; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(duracao);
    digitalWrite(BUZZER, LOW);
    if (i < vezes - 1) {
      delay(duracao / 2);
    }
  }
}

void piscarLed(int led, int vezes, int duracao) {
  for (int i = 0; i < vezes; i++) {
    digitalWrite(led, HIGH);
    delay(duracao);
    digitalWrite(led, LOW);
    delay(duracao);
  }
}

void diagnosticarProblemaWiFi() {
  Serial.println("\n=== DIAGNÓSTICO DE CONEXÃO ===");
  
  // Status do WiFi
  int status = WiFi.status();
  Serial.print("Status WiFi: ");
  switch (status) {
    case WL_CONNECTED:      Serial.println("CONECTADO"); break;
    case WL_IDLE_STATUS:    Serial.println("IDLE"); break;
    case WL_NO_SSID_AVAIL:  Serial.println("SSID NÃO DISPONÍVEL"); break;
    case WL_SCAN_COMPLETED: Serial.println("SCAN COMPLETO"); break;
    case WL_CONNECT_FAILED: Serial.println("FALHA NA CONEXÃO"); break;
    case WL_CONNECTION_LOST:Serial.println("CONEXÃO PERDIDA"); break;
    case WL_DISCONNECTED:   Serial.println("DESCONECTADO"); break;
    default:                Serial.println(status); break;
  }
  
  // Configurações de rede
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("BSSID: ");
  Serial.println(WiFi.BSSIDstr());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS: ");
  Serial.println(WiFi.dnsIP());
  
  // Verificar modo WiFi
  Serial.print("Modo WiFi: ");
  switch (WiFi.getMode()) {
    case WIFI_OFF:   Serial.println("DESLIGADO"); break;
    case WIFI_STA:   Serial.println("ESTAÇÃO"); break;
    case WIFI_AP:    Serial.println("PONTO DE ACESSO"); break;
    case WIFI_AP_STA:Serial.println("PONTO DE ACESSO + ESTAÇÃO"); break;
    default:         Serial.println("DESCONHECIDO"); break;
  }
  
  // Status da configuração WiFi
  Serial.print("Auto-reconexão: ");
  Serial.println(WiFi.getAutoReconnect() ? "ATIVADA" : "DESATIVADA");
  Serial.print("Modo de economia de energia: ");
  Serial.println(WiFi.getSleep() ? "ATIVADO" : "DESATIVADO");
  
  // Memória livre
  Serial.print("Memória livre: ");
  Serial.println(ESP.getFreeHeap());
  
  // Status do WebSocket
  Serial.print("WebSocket conectado: ");
  Serial.println(websocketConnected ? "SIM" : "NÃO");
  Serial.print("Tentativas de reconexão: ");
  Serial.println(reconnectAttempts);
  
  // Informações de tempo
  Serial.print("Tempo desde boot: ");
  Serial.println(millis() / 1000);
  Serial.print("Última tentativa de conexão WiFi: ");
  Serial.println((millis() - lastWifiReconnect) / 1000);
  Serial.print("Última tentativa de conexão WebSocket: ");
  Serial.println((millis() - lastConnectAttempt) / 1000);
  
  Serial.println("=== FIM DO DIAGNÓSTICO ===\n");
  
  // Piscar LED para indicar diagnóstico concluído
  piscarLed(LED_VERMELHO, 3, 100);
  piscarLed(LED_VERDE, 3, 100);
}

// Função para entrar em modo seguro
void enterSafeMode() {
  Serial.println("ENTRANDO EM MODO SEGURO - Muitos resets ou falhas de WiFi");
  safeMode = true;
  safeModeStartTime = millis();
  
  // Indicação visual de modo seguro
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_VERMELHO, HIGH);
    digitalWrite(LED_VERDE, HIGH);
    delay(200);
    digitalWrite(LED_VERMELHO, LOW);
    digitalWrite(LED_VERDE, LOW);
    delay(200);
  }
  
  // Configuração mínima de WiFi
  WiFi.disconnect(true, true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Esperar conexão com timeout longo
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 60) {
    delay(1000);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado em modo seguro!");
    wifiConnected = true;
    
    // Não tentar conectar WebSocket imediatamente
    Serial.println("WebSocket será tentado após o período de recuperação");
  }
  
  Serial.printf("Modo seguro ativo por %d segundos\n", SAFE_MODE_DURATION / 1000);
}

// Novas funções para persistência de comandos administrativos
void salvarUltimoComandoAdmin(const char* comando) {
  strncpy(rtcLastAdminCommand, comando, sizeof(rtcLastAdminCommand) - 1);
  rtcLastAdminCommand[sizeof(rtcLastAdminCommand) - 1] = '\0';
  Serial.printf("Comando administrativo salvo na memória RTC: %s\n", rtcLastAdminCommand);
}

bool verificarComandoAdminPersistente() {
  if (strlen(rtcLastAdminCommand) > 0) {
    Serial.printf("Comando administrativo persistente encontrado: %s\n", rtcLastAdminCommand);
    
    // Se o comando for para iniciar leitura RFID para cadastro
    if (strncmp(rtcLastAdminCommand, "startRFIDReading_", 17) == 0) {
      String tipo = String(rtcLastAdminCommand).substring(17);
      Serial.printf("Restaurando modo administrativo: %s\n", tipo.c_str());
      entrarModoAdministrativo(tipo);
      return true;
    }
    
    return true;
  }
  return false;
} 