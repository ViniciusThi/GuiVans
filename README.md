# 🚐 VansControl

Sistema completo de controle de passageiros de van escolar com ESP32, RFID, Node.js e MongoDB.

## 📋 Funcionalidades

- **🎫 Controle de acesso por RFID**: Cada aluno possui um cartão RFID único
- **🚦 Indicadores visuais**: LEDs verde e vermelho para entrada/saída autorizada/negada
- **🔊 Feedback sonoro**: Buzzer para confirmação de ações
- **📱 Painel do motorista**: Interface web em tempo real
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

## 🔐 Login Padrão

Após executar o script `setup.js`, você terá acesso com:

**📧 Email:** `motorista@teste.com`  
**🔐 Senha:** `123456`

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

// ID único do ESP32 (único para cada van)
const String ESP32_ID = "ESP32_VAN_001";
```

### 4. Upload do código
Faça o upload do código para o ESP32 através do Arduino IDE.

## 📝 Uso do Sistema

### 1. Acesse o sistema
Abra o navegador e vá para `http://localhost:3000`

### 2. Faça login
Use as credenciais padrão ou crie um novo motorista via API

### 3. Teste com RFID
Use os cartões RFID de exemplo:
- **A1B2C3D4** (João Silva)
- **E5F6G7H8** (Ana Santos)

## 🔍 API Endpoints

### Motoristas
- `GET /api/motoristas` - Listar motoristas
- `POST /api/motoristas` - Criar motorista
- `POST /api/motoristas/login` - Login
- `PUT /api/motoristas/:id/van` - Associar van

### Vans
- `GET /api/vans` - Listar vans
- `POST /api/vans` - Criar van
- `GET /api/vans/:id` - Detalhes da van
- `PUT /api/vans/:id` - Atualizar van
- `PUT /api/vans/:id/esp32` - Associar ESP32

### Alunos
- `GET /api/alunos` - Listar alunos
- `POST /api/alunos` - Criar aluno
- `GET /api/alunos/:id` - Detalhes do aluno
- `GET /api/alunos/rfid/:rfidTag` - Buscar por RFID
- `PUT /api/alunos/:id` - Atualizar aluno

### Registros
- `GET /api/registros` - Listar registros
- `POST /api/registros/rfid` - Processar RFID (ESP32)
- `GET /api/registros/van/:vanId/hoje` - Registros de hoje
- `GET /api/registros/estatisticas/:vanId` - Estatísticas

## 🌐 WebSocket Events

### Cliente → Servidor
- `joinVan(vanId)` - Entrar na sala da van
- `leaveVan(vanId)` - Sair da sala da van
- `ping` - Verificar conexão

### Servidor → Cliente
- `joinedVan(data)` - Confirmação de entrada na van
- `novoRegistro(data)` - Novo registro de entrada/saída
- `pong` - Resposta ao ping

## 🚨 Segurança

- Senhas criptografadas com bcrypt
- Autenticação JWT
- Rate limiting nas APIs
- Validação de dados de entrada
- Helmet.js para headers de segurança

## 🐛 Troubleshooting

### ESP32 não conecta ao WiFi
1. Verifique as credenciais de rede
2. Certifique-se de que a rede é 2.4GHz
3. Verifique se o ESP32 está no alcance do WiFi

### MongoDB não conecta
1. Verifique se o MongoDB está rodando
2. Confirme a string de conexão no `.env`
3. Verifique as permissões de rede

### RFID não detecta cartões
1. Verifique as conexões SPI
2. Teste com cartões diferentes
3. Verifique a alimentação do sensor

### WebSocket não conecta
1. Verifique se o servidor está rodando
2. Confirme as configurações de CORS
3. Teste em navegadores diferentes

### Porta 3000 já está em uso
```bash
# Windows
netstat -ano | findstr :3000
taskkill /PID <PID_NUMBER> /F

# Linux/Mac
lsof -ti:3000 | xargs kill -9
```

## 📦 Estrutura do Projeto

```
vanscontrol/
├── config/
│   └── database.js          # Configuração MongoDB
├── esp32/
│   └── vansControl.ino      # Código Arduino ESP32
├── models/
│   ├── Aluno.js            # Modelo de dados do Aluno
│   ├── Motorista.js        # Modelo de dados do Motorista
│   ├── Registro.js         # Modelo de dados do Registro
│   └── Van.js              # Modelo de dados da Van
├── public/
│   ├── index.html          # Interface web
│   └── app.js              # JavaScript frontend
├── routes/
│   ├── alunos.js           # Rotas API dos Alunos
│   ├── motoristas.js       # Rotas API dos Motoristas
│   ├── registros.js        # Rotas API dos Registros
│   └── vans.js             # Rotas API das Vans
├── .env.example            # Exemplo de variáveis de ambiente
├── .gitignore              # Arquivos ignorados pelo Git
├── package.json            # Dependências do projeto
├── README.md               # Documentação
└── server.js               # Servidor principal
```

## 📄 Licença

MIT License - veja o arquivo LICENSE para detalhes.

## 👥 Contribuição

1. Fork o projeto
2. Crie uma branch para sua feature (`git checkout -b feature/AmazingFeature`)
3. Commit suas mudanças (`git commit -m 'Add some AmazingFeature'`)
4. Push para a branch (`git push origin feature/AmazingFeature`)
5. Abra um Pull Request

## 📞 Suporte

Para suporte, entre em contato:
- Email: vinicius@email.com
- GitHub Issues: [Issues](https://github.com/seu-usuario/vanscontrol/issues)

---

**VansControl** - Sistema inteligente de controle de passageiros 🚐✨ 