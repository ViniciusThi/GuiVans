#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WebSocketsClient.h>

// Configurações de Hardware
#define SS_PIN 21
#define RST_PIN 22
#define LED_VERDE 2
#define LED_VERMELHO 4
#define BUZZER 5

// Configurações de Rede
const char* ssid = "dlink - 5Ghz";
const char* password = "661705550232";
const char* serverURL = "http://192.168.0.113:3000";
const char* websocketHost = "192.168.0.113";
const int websocketPort = 3000;

// ID único do ESP32 (deve ser único para cada van)
const String ESP32_ID = "ESP32_VAN_001";

// Objetos
MFRC522 rfid(SS_PIN, RST_PIN);
HTTPClient http;
WebSocketsClient webSocket;

// Variáveis de controle
unsigned long lastCardRead = 0;
const unsigned long DEBOUNCE_TIME = 2000; // 2 segundos entre leituras
bool wifiConnected = false;
bool websocketConnected = false;

// Modo administrativo
bool adminReadingMode = false;
unsigned long adminModeStartTime = 0;
const unsigned long ADMIN_MODE_TIMEOUT = 60000; // 60 segundos timeout

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== VansControl ESP32 v2.0 ===");
  Serial.println("Inicializando sistema...");
  
  // Configurar pinos
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  
  // Inicializar com LED vermelho
  digitalWrite(LED_VERMELHO, HIGH);
  digitalWrite(LED_VERDE, LOW);
  
  // Inicializar SPI e RFID
  SPI.begin();
  rfid.PCD_Init();
  
  Serial.println("RFID RC522 inicializado");
  Serial.print("ESP32 ID: ");
  Serial.println(ESP32_ID);
  
  // Conectar WiFi
  conectarWiFi();
  
  // Configurar WebSocket
  configurarWebSocket();
  
  // Testar conexão com servidor
  testarConexaoServidor();
  
  Serial.println("Sistema pronto!");
  tocarBuzzer(2, 200); // 2 bips de confirmação
}

void loop() {
  // Verificar conexão WiFi
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    websocketConnected = false;
    digitalWrite(LED_VERMELHO, HIGH);
    digitalWrite(LED_VERDE, LOW);
    conectarWiFi();
    return;
  }
  
  wifiConnected = true;
  
  // Processar WebSocket
  webSocket.loop();
  
  // Verificar timeout do modo administrativo
  if (adminReadingMode && (millis() - adminModeStartTime > ADMIN_MODE_TIMEOUT)) {
    Serial.println("Timeout do modo administrativo");
    sairModoAdministrativo();
  }
  
  // Verificar se há cartão presente
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    // Sem cartão - LED baseado no modo
    if (millis() - lastCardRead > 5000) {
      if (adminReadingMode) {
        // Modo admin - LED verde piscando
        static unsigned long lastBlink = 0;
        if (millis() - lastBlink > 500) {
          digitalWrite(LED_VERDE, !digitalRead(LED_VERDE));
          lastBlink = millis();
        }
        digitalWrite(LED_VERMELHO, LOW);
      } else {
        // Modo normal - LED vermelho fixo
        digitalWrite(LED_VERMELHO, HIGH);
        digitalWrite(LED_VERDE, LOW);
      }
    }
    delay(100);
    return;
  }
  
  // Debounce - evitar leituras múltiplas
  if (millis() - lastCardRead < DEBOUNCE_TIME) {
    return;
  }
  
  lastCardRead = millis();
  
  // Ler UID do cartão
  String rfidTag = lerUIDCartao();
  
  Serial.print("RFID detectado: ");
  Serial.println(rfidTag);
  Serial.print("Modo: ");
  Serial.println(adminReadingMode ? "Administrativo" : "Normal");
  
  // Piscar LED azul (simulado com verde rápido) para indicar leitura
  piscarLed(LED_VERDE, 3, 100);
  
  if (adminReadingMode) {
    // Modo administrativo - enviar via WebSocket
    enviarRFIDViaWebSocket(rfidTag);
    // Sair do modo administrativo após leitura bem-sucedida
    sairModoAdministrativo();
  } else {
    // Modo normal - processar via HTTP
    processarRFID(rfidTag);
  }
  
  // Parar comunicação com o cartão
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  delay(500);
}

void configurarWebSocket() {
  Serial.println("Configurando WebSocket...");
  
  webSocket.begin(websocketHost, websocketPort, "/socket.io/?EIO=4&transport=websocket");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  
  Serial.println("WebSocket configurado");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("WebSocket Desconectado");
      websocketConnected = false;
      break;
      
    case WStype_CONNECTED:
      Serial.printf("WebSocket Conectado: %s\n", payload);
      websocketConnected = true;
      
      // Enviar identificação do ESP32
      DynamicJsonDocument doc(512);
      doc["esp32Id"] = ESP32_ID;
      doc["type"] = "esp32_connected";
      doc["timestamp"] = millis();
      
      String message;
      serializeJson(doc, message);
      webSocket.sendTXT(message);
      break;
      
    case WStype_TEXT:
      Serial.printf("WebSocket Mensagem: %s\n", payload);
      processarComandoWebSocket((char*)payload);
      break;
      
    case WStype_ERROR:
      Serial.printf("WebSocket Erro: %s\n", payload);
      break;
      
    default:
      break;
  }
}

void processarComandoWebSocket(String message) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, message);
  
  String command = doc["command"];
  
  if (command == "startRFIDReading") {
    Serial.println("Comando recebido: Iniciar modo administrativo");
    entrarModoAdministrativo();
  } else if (command == "stopRFIDReading") {
    Serial.println("Comando recebido: Parar modo administrativo");
    sairModoAdministrativo();
  }
}

void entrarModoAdministrativo() {
  adminReadingMode = true;
  adminModeStartTime = millis();
  
  Serial.println("=== MODO ADMINISTRATIVO ATIVADO ===");
  Serial.println("Aguardando leitura de cartão RFID...");
  
  // Feedback visual e sonoro
  piscarLed(LED_VERDE, 5, 100);
  tocarBuzzer(3, 100);
  
  // Enviar confirmação via WebSocket
  if (websocketConnected) {
    DynamicJsonDocument doc(512);
    doc["type"] = "esp32Status";
    doc["esp32Id"] = ESP32_ID;
    doc["status"] = "admin_mode_active";
    doc["message"] = "Modo administrativo ativado";
    doc["timestamp"] = millis();
    
    String message;
    serializeJson(doc, message);
    webSocket.sendTXT(message);
  }
}

void sairModoAdministrativo() {
  adminReadingMode = false;
  
  Serial.println("=== MODO ADMINISTRATIVO DESATIVADO ===");
  
  // Feedback visual
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, HIGH);
  tocarBuzzer(1, 200);
  
  // Enviar confirmação via WebSocket
  if (websocketConnected) {
    DynamicJsonDocument doc(512);
    doc["type"] = "esp32Status";
    doc["esp32Id"] = ESP32_ID;
    doc["status"] = "admin_mode_inactive";
    doc["message"] = "Modo administrativo desativado";
    doc["timestamp"] = millis();
    
    String message;
    serializeJson(doc, message);
    webSocket.sendTXT(message);
  }
}

void enviarRFIDViaWebSocket(String rfidTag) {
  if (!websocketConnected) {
    Serial.println("WebSocket não conectado, não é possível enviar RFID");
    tocarBuzzer(3, 100); // Erro
    return;
  }
  
  Serial.println("Enviando RFID via WebSocket para admin...");
  
  DynamicJsonDocument doc(512);
  doc["type"] = "rfidRead";
  doc["rfidTag"] = rfidTag;
  doc["esp32Id"] = ESP32_ID;
  doc["source"] = "admin_reading";
  doc["timestamp"] = millis();
  
  String message;
  serializeJson(doc, message);
  
  webSocket.sendTXT(message);
  
  Serial.println("RFID enviado para admin com sucesso!");
  
  // Feedback de sucesso
  digitalWrite(LED_VERDE, HIGH);
  digitalWrite(LED_VERMELHO, LOW);
  tocarBuzzer(2, 150);
  delay(2000);
}

void conectarWiFi() {
  Serial.print("Conectando ao WiFi");
  WiFi.begin(ssid, password);
  
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
    
    // Indicar sucesso na conexão
    piscarLed(LED_VERDE, 3, 200);
    
    // Reconectar WebSocket se necessário
    if (!websocketConnected) {
      configurarWebSocket();
    }
  } else {
    Serial.println();
    Serial.println("Falha na conexão WiFi!");
    piscarLed(LED_VERMELHO, 5, 100);
  }
}

void testarConexaoServidor() {
  Serial.println("Testando conexão com servidor...");
  
  http.begin(String(serverURL) + "/api/health");
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.println("Servidor respondeu:");
    Serial.println(response);
    piscarLed(LED_VERDE, 2, 300);
  } else {
    Serial.print("Erro na conexão com servidor: ");
    Serial.println(httpCode);
    piscarLed(LED_VERMELHO, 3, 200);
  }
  
  http.end();
}

String lerUIDCartao() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uid += "0";
    }
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void processarRFID(String rfidTag) {
  Serial.println("Enviando RFID para servidor...");
  
  // Preparar JSON
  DynamicJsonDocument doc(1024);
  doc["rfidTag"] = rfidTag;
  doc["esp32Id"] = ESP32_ID;
  doc["tipo"] = "entrada"; // Pode ser 'entrada' ou 'saida'
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Fazer requisição HTTP
  http.begin(String(serverURL) + "/api/registros/rfid");
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("Resposta do servidor (");
    Serial.print(httpCode);
    Serial.print("): ");
    Serial.println(response);
    
    // Processar resposta
    processarRespostaServidor(response);
  } else {
    Serial.print("Erro na requisição: ");
    Serial.println(httpCode);
    // Erro - LED vermelho e buzzer de erro
    digitalWrite(LED_VERMELHO, HIGH);
    digitalWrite(LED_VERDE, LOW);
    tocarBuzzer(3, 100);
  }
  
  http.end();
}

void processarRespostaServidor(String response) {
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, response);
  
  String status = doc["status"];
  String ledColor = doc["ledColor"];
  bool buzzer = doc["buzzer"];
  String message = doc["message"];
  
  Serial.print("Status: ");
  Serial.println(status);
  Serial.print("Mensagem: ");
  Serial.println(message);
  
  // Controlar LEDs baseado na resposta
  if (ledColor == "green") {
    // Autorizado - LED verde
    digitalWrite(LED_VERMELHO, LOW);
    digitalWrite(LED_VERDE, HIGH);
    
    if (buzzer) {
      tocarBuzzer(2, 150); // 2 bips de sucesso
    }
    
    // Manter LED verde por 3 segundos
    delay(3000);
    
  } else if (ledColor == "red") {
    // Negado - LED vermelho
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_VERMELHO, HIGH);
    
    // Buzzer de erro
    tocarBuzzer(1, 500); // 1 bip longo de erro
    
    // Piscar vermelho
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_VERMELHO, LOW);
      delay(200);
      digitalWrite(LED_VERMELHO, HIGH);
      delay(200);
    }
  }
  
  // Voltar ao estado padrão após um tempo
  delay(1000);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, HIGH); // Estado padrão
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

// Função para debug - imprimir informações do sistema
void printSystemInfo() {
  Serial.println("=== Informações do Sistema ===");
  Serial.print("ESP32 ID: ");
  Serial.println(ESP32_ID);
  Serial.print("WiFi Status: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado");
  Serial.print("WebSocket Status: ");
  Serial.println(websocketConnected ? "Conectado" : "Desconectado");
  Serial.print("Modo Admin: ");
  Serial.println(adminReadingMode ? "Ativo" : "Inativo");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
  Serial.print("Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.print("Uptime: ");
  Serial.println(millis() / 1000);
  Serial.println("============================");
} 
 