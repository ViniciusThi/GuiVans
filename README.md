# 🚐 VansControl - Sistema de Controle de Passageiros

Sistema completo para controle de acesso de passageiros em vans escolares usando RFID, ESP32 e painel administrativo web.

## 🎯 **Funcionalidades**

- ✅ **Controle de Acesso RFID**: ESP32 com leitor RFID para entrada/saída
- ✅ **Painel Administrativo**: Interface web para gerenciar motoristas, vans e alunos
- ✅ **Leitura RFID Remota**: Cadastro de alunos com leitura automática do cartão
- ✅ **Tempo Real**: WebSocket para comunicação instantânea
- ✅ **Registros**: Histórico completo de entradas e saídas

## 📁 **Estrutura do Projeto**

```
VansControl/
├── esp32/
│   ├── vansControl.ino          # Código principal ESP32
│   ├── INSTALL_LIBRARIES.md     # Guia de instalação de bibliotecas
│   └── README.md                # Instruções ESP32
├── models/                      # Modelos MongoDB
│   ├── Aluno.js
│   ├── Van.js
│   ├── Motorista.js
│   └── Registro.js
├── routes/                      # Rotas da API
│   ├── alunos.js
│   ├── vans.js
│   ├── motoristas.js
│   └── registros.js
├── config/
│   └── database.js              # Configuração MongoDB
├── public/                      # Interface web
│   ├── index.html               # Painel do motorista
│   ├── app.js                   # JavaScript do painel
│   ├── admin.html               # Painel administrativo
│   └── admin.js                 # JavaScript do admin
├── server.js                    # Servidor principal
├── package.json                 # Dependências Node.js
├── setup.js                     # Script de configuração inicial
└── .env                         # Variáveis de ambiente
```

## 🚀 **Instalação e Configuração**

### **1. Servidor Node.js**

```bash
# Instalar dependências
npm install

# Configurar banco de dados
node setup.js

# Iniciar servidor
npm start
```

### **2. ESP32**

1. Instale as bibliotecas necessárias (veja `esp32/INSTALL_LIBRARIES.md`)
2. Configure WiFi no arquivo `esp32/vansControl.ino`
3. Faça upload do código para o ESP32

### **3. Hardware ESP32**

```
ESP32 Pinout:
├── RFID RC522
│   ├── SDA  → GPIO 21
│   ├── SCK  → GPIO 18
│   ├── MOSI → GPIO 23
│   ├── MISO → GPIO 19
│   ├── RST  → GPIO 22
│   └── 3.3V → 3.3V
├── LED Verde  → GPIO 2
├── LED Vermelho → GPIO 4
└── Buzzer → GPIO 5
```

## 💻 **Como Usar**

### **Painel Administrativo** (`/admin.html`)
- Cadastrar motoristas, vans e alunos
- Usar "Ler Cartão" para vincular RFID automaticamente
- Visualizar registros em tempo real

### **Painel do Motorista** (`/index.html`)
- Monitorar entradas e saídas em tempo real
- Ver status dos alunos
- Controlar ESP32 remotamente

### **ESP32 - Modos de Operação**

**Modo Normal:**
- LED vermelho fixo
- Lê RFID e verifica acesso via HTTP
- Controla LEDs e buzzer baseado na resposta

**Modo Administrativo:**
- LED verde piscando
- Ativado pelo painel admin
- Envia RFID via WebSocket para cadastro

## 🔧 **Configuração**

### **Variáveis de Ambiente (.env)**
```env
PORT=3000
MONGODB_URI=mongodb://localhost:27017/vanscontrol
NODE_ENV=development
```

### **WiFi ESP32**
```cpp
const char* ssid = "SEU_WIFI";
const char* password = "SUA_SENHA";
const char* serverURL = "http://IP_SERVIDOR:3000";
```

## 📡 **API Endpoints**

- `GET /api/alunos` - Listar alunos
- `POST /api/alunos` - Cadastrar aluno
- `GET /api/vans` - Listar vans
- `POST /api/vans` - Cadastrar van
- `GET /api/motoristas` - Listar motoristas
- `POST /api/motoristas` - Cadastrar motorista
- `POST /api/registros/rfid` - Processar RFID
- `GET /api/registros` - Listar registros

## 🌐 **WebSocket Events**

### **ESP32 → Servidor**
- `esp32_connected` - ESP32 conectado
- `rfidRead` - RFID lido

### **Servidor → ESP32**
- `esp32Command` - Comandos para ESP32
- `startRFIDReading` - Iniciar modo admin
- `stopRFIDReading` - Parar modo admin

## 🛠️ **Tecnologias**

- **Backend**: Node.js, Express, MongoDB, Socket.IO
- **Frontend**: HTML5, CSS3, JavaScript, Bootstrap 5
- **Hardware**: ESP32, RFID RC522, LEDs, Buzzer
- **Bibliotecas ESP32**: WiFi, WebSocketsClient_Generic, ArduinoJson, MFRC522

## 📞 **Suporte**

Para problemas ou dúvidas:
1. Verifique os logs do servidor
2. Monitore o Serial Monitor do ESP32
3. Teste a conectividade WiFi e WebSocket

---

**VansControl v2.1** - Sistema completo de controle de passageiros com RFID 