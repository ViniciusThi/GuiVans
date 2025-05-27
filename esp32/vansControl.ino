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

// Configurações de Rede
const char* ssid = "dlink - 5Ghz";
const char* password = "661705550232";
const char* serverURL = "http://192.168.0.113:3000";
const char* websocketHost = "192.168.0.113";
const int websocketPort = 3000;

// ID único do ESP32
const String ESP32_ID = "ESP32_VAN_001";

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
const unsigned long ADMIN_MODE_TIMEOUT = 60000;

// Declarações de função
void configurarWebSocket();
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void enviarIdentificacaoESP32();
void processarMensagemWebSocket(String message);
void processarComandoESP32(JsonObject data);
void entrarModoAdministrativo();
void sairModoAdministrativo();
void enviarRFIDViaWebSocket(String rfidTag);
void conectarWiFi();
String lerUIDCartao();
void processarRFID(String rfidTag);
void processarRespostaServidor(String response);
void tocarBuzzer(int vezes, int duracao);
void piscarLed(int led, int vezes, int duracao);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== VansControl ESP32 Simples ===");
  
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
  
  // Conectar WiFi
  conectarWiFi();
  
  // Configurar WebSocket
  configurarWebSocket();
  
  Serial.println("Sistema pronto!");
  tocarBuzzer(2, 200);
}

void loop() {
  // Verificar WiFi
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    websocketConnected = false;
    conectarWiFi();
    return;
  }
  
  wifiConnected = true;
  webSocket.loop();
  
  // Verificar timeout do modo admin
  if (adminReadingMode && (millis() - adminModeStartTime > ADMIN_MODE_TIMEOUT)) {
    Serial.println("Timeout do modo administrativo");
    sairModoAdministrativo();
  }
  
  // Verificar cartão RFID
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    // Indicação visual baseada no status
    if (adminReadingMode) {
      // LED verde piscando no modo admin
      static unsigned long lastBlink = 0;
      if (millis() - lastBlink > 500) {
        digitalWrite(LED_VERDE, !digitalRead(LED_VERDE));
        lastBlink = millis();
      }
      digitalWrite(LED_VERMELHO, LOW);
    } else {
      // LED vermelho fixo no modo normal
      digitalWrite(LED_VERMELHO, HIGH);
      digitalWrite(LED_VERDE, LOW);
    }
    delay(100);
    return;
  }
  
  // Debounce
  if (millis() - lastCardRead < DEBOUNCE_TIME) {
    return;
  }
  
  lastCardRead = millis();
  String rfidTag = lerUIDCartao();
  
  Serial.print("RFID detectado: ");
  Serial.println(rfidTag);
  Serial.print("Modo: ");
  Serial.println(adminReadingMode ? "Administrativo" : "Normal");
  
  // Piscar LED para indicar leitura
  piscarLed(LED_VERDE, 3, 100);
  
  if (adminReadingMode) {
    // Modo administrativo - enviar via WebSocket
    enviarRFIDViaWebSocket(rfidTag);
    sairModoAdministrativo();
  } else {
    // Modo normal - processar via HTTP
    processarRFID(rfidTag);
  }
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(500);
}

void configurarWebSocket() {
  Serial.println("Configurando WebSocket...");
  
  webSocket.beginSocketIO(websocketHost, websocketPort, "/socket.io/?EIO=4&transport=websocket");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);
  
  Serial.println("WebSocket configurado");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Desconectado");
      websocketConnected = false;
      break;
      
    case WStype_CONNECTED:
      Serial.printf("[WSc] Conectado: %s\n", payload);
      websocketConnected = true;
      enviarIdentificacaoESP32();
      break;
      
    case WStype_TEXT:
      Serial.printf("[WSc] Mensagem: %s\n", payload);
      processarMensagemWebSocket((char*)payload);
      break;
      
    case WStype_PING:
      Serial.println("[WSc] Ping recebido");
      break;
      
    case WStype_PONG:
      Serial.println("[WSc] Pong recebido");
      break;
      
    default:
      break;
  }
}

void enviarIdentificacaoESP32() {
  if (!websocketConnected) return;
  
  DynamicJsonDocument doc(512);
  doc[0] = "esp32_connected";
  
  JsonObject data = doc.createNestedObject(1);
  data["esp32Id"] = ESP32_ID;
  data["type"] = "esp32_connected";
  data["timestamp"] = millis();
  
  String message;
  serializeJson(doc, message);
  webSocket.sendTXT(message);
  
  Serial.println("[WSc] Identificação enviada");
}

void processarMensagemWebSocket(String message) {
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
      }
    }
  }
}

void processarComandoESP32(JsonObject data) {
  String command = data["command"];
  
  Serial.printf("Comando: %s\n", command.c_str());
  
  if (command == "startRFIDReading") {
    Serial.println("Iniciando modo administrativo");
    entrarModoAdministrativo();
  } else if (command == "stopRFIDReading") {
    Serial.println("Parando modo administrativo");
    sairModoAdministrativo();
  }
}

void entrarModoAdministrativo() {
  adminReadingMode = true;
  adminModeStartTime = millis();
  
  Serial.println("=== MODO ADMINISTRATIVO ATIVADO ===");
  
  piscarLed(LED_VERDE, 5, 100);
  tocarBuzzer(3, 100);
}

void sairModoAdministrativo() {
  adminReadingMode = false;
  
  Serial.println("=== MODO ADMINISTRATIVO DESATIVADO ===");
  
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, HIGH);
  tocarBuzzer(1, 200);
}

void enviarRFIDViaWebSocket(String rfidTag) {
  if (!websocketConnected) {
    Serial.println("WebSocket não conectado");
    tocarBuzzer(3, 100);
    return;
  }
  
  Serial.println("Enviando RFID via WebSocket...");
  
  DynamicJsonDocument doc(512);
  doc[0] = "rfidRead";
  
  JsonObject data = doc.createNestedObject(1);
  data["rfidTag"] = rfidTag;
  data["esp32Id"] = ESP32_ID;
  data["source"] = "admin_reading";
  data["timestamp"] = millis();
  
  String jsonMessage;
  serializeJson(doc, jsonMessage);
  webSocket.sendTXT("42" + jsonMessage);
  
  Serial.println("RFID enviado para admin!");
  
  digitalWrite(LED_VERDE, HIGH);
  digitalWrite(LED_VERMELHO, LOW);
  tocarBuzzer(2, 150);
  delay(2000);
}

void conectarWiFi() {
  Serial.print("Conectando WiFi");
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
    piscarLed(LED_VERDE, 3, 200);
  } else {
    Serial.println("Falha WiFi!");
    piscarLed(LED_VERMELHO, 5, 100);
  }
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
  
  DynamicJsonDocument doc(1024);
  doc["rfidTag"] = rfidTag;
  doc["esp32Id"] = ESP32_ID;
  doc["tipo"] = "entrada";
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  http.begin(String(serverURL) + "/api/registros/rfid");
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.print("Resposta (");
    Serial.print(httpCode);
    Serial.print("): ");
    Serial.println(response);
    
    processarRespostaServidor(response);
  } else {
    Serial.print("Erro HTTP: ");
    Serial.println(httpCode);
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