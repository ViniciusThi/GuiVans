# 🚐 VansControl

Sistema completo de controle de passageiros de van escolar com ESP32, RFID, Node.js e MongoDB.

## 📋 Funcionalidades

- **🎫 Controle de acesso por RFID**: Cada aluno possui um cartão RFID único
- **🚦 Indicadores visuais**: LEDs verde e vermelho para entrada/saída autorizada/negada
- **🔊 Feedback sonoro**: Buzzer para confirmação de ações
- **📱 Painel do motorista**: Interface web em tempo real
- **⚙️ Painel administrativo**: Gestão completa do sistema
- **🎯 Leitura RFID remota**: Cadastro de cartões via interface web
- **🌐 WebSocket**: Comunicação em tempo real entre ESP32 e painel
- **📊 Estatísticas**: Relatórios de entrada/saída por van
- **👥 Gestão completa**: Cadastro de motoristas, vans e alunos

## 🛠️ Componentes do Sistema

### Backend (Node.js)
- **Express.js** para API REST
- **Socket.IO** para comunicação em tempo real
- **MongoDB** com Mongoose para banco de dados
- **JWT** para autenticação
- **Bcrypt** para criptografia de senhas

### Hardware (ESP32)
- **ESP32** microcontrolador principal
- **MFRC522** sensor RFID
- **LEDs** verde e vermelho para indicação
- **Buzzer** para feedback sonoro
- **WebSocket Client** para modo administrativo

### Frontend
- **HTML5/CSS3** interface responsiva
- **Bootstrap 5** framework CSS
- **JavaScript vanilla** para interação
- **Socket.IO Client** para tempo real

## 🔧 Instalação

### 1. Clone o repositório
```bash
git clone https://github.com/seu-usuario/vanscontrol.git
cd vanscontrol
```

### 2. Instale as dependências
```bash
npm install
```

### 3. Configure o MongoDB
Certifique-se de que o MongoDB está rodando localmente:
```bash
# No Windows
mongod

# No Linux/Mac
sudo systemctl start mongod
```

### 4. Configure as variáveis de ambiente
Copie o arquivo `.env.example` para `.env` e configure:
```bash
cp .env.example .env
```

Edite o arquivo `.env` com suas configurações:
```env
PORT=3000
NODE_ENV=development
MONGODB_URI=mongodb://localhost:27017/vanscontrol
JWT_SECRET=sua_chave_secreta_aqui
FRONTEND_URL=http://localhost:3000
ESP32_TIMEOUT=30000
```

### 5. Configure dados iniciais (OPCIONAL)
Execute o script de configuração para criar dados de exemplo:
```bash
node setup.js
```

### 6. Inicie o servidor
```bash
# Modo desenvolvimento
npm run dev

# Modo produção
npm start
```

## 🔐 Acesso ao Sistema

### 📱 Painel do Motorista
**URL:** `http://localhost:3000`  
**📧 Email:** `motorista@teste.com`  
**🔐 Senha:** `123456`

### ⚙️ Painel Administrativo
**URL:** `http://localhost:3000/admin.html`  
Acesso direto sem login (para administradores)

### 📊 Dados de exemplo criados:
- **Van:** VAN-0001 (ESP32_VAN_001)
- **Alunos:** 
  - João Silva (RFID: A1B2C3D4)
  - Ana Santos (RFID: E5F6G7H8)

## 🔌 Configuração do ESP32

### 1. Bibliotecas necessárias
Instale as seguintes bibliotecas no Arduino IDE:
- **MFRC522** by GithubCommunity
- **ArduinoJson** by Benoit Blanchon
- **WebSockets** by Markus Sattler
- **WiFi** (já inclusa no ESP32)

### 2. Conexões de hardware
```
ESP32    →    MFRC522
3.3V     →    3.3V
GND      →    GND
21       →    SDA
22       →    RST
18       →    SCK
19       →    MISO
23       →    MOSI

ESP32    →    Componentes
2        →    LED Verde (+ resistor 220Ω)
4        →    LED Vermelho (+ resistor 220Ω)
5        →    Buzzer
GND      →    GND de todos os componentes
```

### 3. Configure o código
No arquivo `esp32/vansControl.ino`, edite:
```cpp
// Configurações de Rede
const char* ssid = "SUA_REDE_WIFI";
const char* password = "SUA_SENHA_WIFI";
const char* serverURL = "http://IP_DO_SERVIDOR:3000";
const char* websocketHost = "IP_DO_SERVIDOR";
const int websocketPort = 3000;

// ID único do ESP32 (único para cada van)
const String ESP32_ID = "ESP32_VAN_001";
```

### 4. Upload do código
Faça o upload do código para o ESP32 através do Arduino IDE.

## 📝 Uso do Sistema

### 1. Painel do Motorista
- Acesse `http://localhost:3000`
- Faça login com as credenciais
- Monitore entradas/saídas em tempo real
- Visualize estatísticas da van

### 2. Painel Administrativo
- Acesse `http://localhost:3000/admin.html`
- **Aba Motoristas**: Cadastre e gerencie motoristas
- **Aba Vans**: Cadastre vans e associe ESP32s
- **Aba Alunos**: Cadastre alunos com leitura RFID remota
- **Aba Registros**: Visualize histórico completo

### 3. Cadastro de Aluno com RFID
1. No painel admin, vá para aba "Alunos"
2. Preencha os dados do aluno
3. Clique em "Ler Cartão" no campo RFID
4. O ESP32 entrará em modo administrativo (LED verde piscando)
5. Aproxime o cartão RFID do sensor
6. O ID será automaticamente preenchido
7. Complete o cadastro e salve

### 4. Teste com RFID
Use os cartões RFID de exemplo:
- **A1B2C3D4** (João Silva)
- **E5F6G7H8** (Ana Santos)

## 🔍 API Endpoints

### Motoristas
- `GET /api/motoristas` - Listar motoristas
- `POST /api/motoristas` - Criar motorista
- `POST /api/motoristas/login` - Login
- `PUT /api/motoristas/:id/van` - Associar van
- `DELETE /api/motoristas/:id` - Excluir motorista

### Vans
- `GET /api/vans` - Listar vans
- `POST /api/vans` - Criar van
- `GET /api/vans/:id` - Detalhes da van
- `PUT /api/vans/:id` - Atualizar van
- `DELETE /api/vans/:id` - Excluir van
- `PUT /api/vans/:id/esp32` - Associar ESP32

### Alunos
- `GET /api/alunos` - Listar alunos
- `POST /api/alunos` - Criar aluno
- `GET /api/alunos/:id` - Detalhes do aluno
- `GET /api/alunos/rfid/:rfidTag` - Buscar por RFID
- `PUT /api/alunos/:id` - Atualizar aluno
- `DELETE /api/alunos/:id` - Excluir aluno

### Registros
- `GET /api/registros` - Listar registros
- `POST /api/registros/rfid` - Processar RFID (ESP32)
- `GET /api/registros/van/:vanId/hoje` - Registros de hoje
- `GET /api/registros/estatisticas/:vanId` - Estatísticas

## 🌐 WebSocket Events

### Cliente → Servidor
- `joinVan(vanId)` - Entrar na sala da van
- `leaveVan(vanId)` - Sair da sala da van
- `startRFIDReading()` - Iniciar modo administrativo RFID
- `stopRFIDReading()` - Parar modo administrativo RFID
- `ping` - Verificar conexão

### Servidor → Cliente
- `joinedVan(data)` - Confirmação de entrada na van
- `novoRegistro(data)` - Novo registro de entrada/saída
- `rfidRead(data)` - RFID lido (modo admin ou normal)
- `esp32Command(data)` - Comando para ESP32
- `esp32Status(data)` - Status do ESP32
- `pong` - Resposta ao ping

### ESP32 → Servidor
- `rfidRead(data)` - Envio de RFID lido
- `esp32Status(data)` - Status do dispositivo
- `esp32Error(data)` - Erro reportado

## 🎯 Modos de Operação do ESP32

### Modo Normal
- **LED vermelho fixo**: Aguardando cartão
- **Leitura RFID**: Envia via HTTP para controle de acesso
- **LED verde**: Acesso autorizado
- **LED vermelho piscando**: Acesso negado

### Modo Administrativo
- **Ativação**: Via painel admin (botão "Ler Cartão")
- **LED verde piscando**: Aguardando cartão para cadastro
- **Comunicação**: Via WebSocket em tempo real
- **Timeout**: 60 segundos automático
- **Feedback**: Buzzer e LEDs para confirmação

## 🚨 Segurança

- Senhas criptografadas com bcrypt
- Autenticação JWT
- Rate limiting nas APIs
- Validação de dados de entrada
- CORS configurado
- Helmet para headers de segurança

## 🔧 Troubleshooting

### Servidor não inicia (porta 3000 em uso)
```bash
# Windows
netstat -ano | findstr :3000
taskkill /PID <PID_NUMBER> /F

# Linux/Mac
lsof -ti:3000 | xargs kill -9
```

### ESP32 não conecta ao WebSocket
- Verifique IP do servidor no código
- Confirme que o servidor está rodando
- Teste conectividade de rede
- Verifique firewall/antivírus

### RFID não é detectado no modo admin
- Certifique-se que o ESP32 está conectado via WebSocket
- Verifique se o modo administrativo foi ativado
- Teste a leitura RFID no modo normal primeiro
- Verifique conexões do MFRC522

## 📄 Licença

Este projeto está sob a licença MIT. Veja o arquivo [LICENSE](LICENSE) para mais detalhes.

## 🤝 Contribuição

1. Faça um fork do projeto
2. Crie uma branch para sua feature (`git checkout -b feature/AmazingFeature`)
3. Commit suas mudanças (`git commit -m 'Add some AmazingFeature'`)
4. Push para a branch (`git push origin feature/AmazingFeature`)
5. Abra um Pull Request

## 📞 Suporte

Para suporte, abra uma issue no GitHub ou entre em contato via email.

---

**Desenvolvido com ❤️ para facilitar o transporte escolar seguro e eficiente.** 