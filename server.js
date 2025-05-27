const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const cors = require('cors');
const helmet = require('helmet');
const rateLimit = require('express-rate-limit');
require('dotenv').config();

const connectDB = require('./config/database');

// Importar rotas
const motoristasRoutes = require('./routes/motoristas');
const vansRoutes = require('./routes/vans');
const alunosRoutes = require('./routes/alunos');
const registrosRoutes = require('./routes/registros');

const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
  cors: {
    origin: "*",
    methods: ["GET", "POST"]
  }
});

// Conectar ao MongoDB
connectDB();

// Middlewares
app.use(helmet({
  contentSecurityPolicy: false // Para desenvolvimento
}));

app.use(cors({
  origin: process.env.FRONTEND_URL || "*",
  credentials: true
}));

// Rate limiting
const limiter = rateLimit({
  windowMs: 15 * 60 * 1000, // 15 minutos
  max: 100 // máximo 100 requests por IP
});
app.use('/api/', limiter);

app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true }));

// Middleware para disponibilizar io nas rotas
app.use((req, res, next) => {
  req.io = io;
  next();
});

// Rotas da API
app.use('/api/motoristas', motoristasRoutes);
app.use('/api/vans', vansRoutes);
app.use('/api/alunos', alunosRoutes);
app.use('/api/registros', registrosRoutes);

// Rota de saúde da API
app.get('/api/health', (req, res) => {
  res.json({ 
    status: 'ok', 
    timestamp: new Date(),
    version: '1.0.0'
  });
});

// Servir arquivos estáticos (frontend)
app.use(express.static('public'));

// WebSocket - Gerenciamento de conexões
io.on('connection', (socket) => {
  console.log(`Cliente conectado: ${socket.id}`);

  // Motorista se conecta a sua van
  socket.on('joinVan', (vanId) => {
    socket.join(`van_${vanId}`);
    console.log(`Socket ${socket.id} entrou na sala van_${vanId}`);
    
    socket.emit('joinedVan', { vanId, message: 'Conectado à van com sucesso' });
  });

  // Motorista sai da van
  socket.on('leaveVan', (vanId) => {
    socket.leave(`van_${vanId}`);
    console.log(`Socket ${socket.id} saiu da sala van_${vanId}`);
  });

  // === FUNCIONALIDADES RFID ADMIN ===
  
  // Admin solicita início de leitura RFID
  socket.on('startRFIDReading', (data) => {
    console.log('Admin solicitou início de leitura RFID:', data);
    
    // Marcar este socket como admin em modo de leitura
    socket.isAdminReading = true;
    socket.join('admin_rfid_reading');
    
    // Notificar todos os ESP32s para entrar em modo de leitura
    io.emit('esp32Command', {
      command: 'startRFIDReading',
      timestamp: new Date(),
      adminSocketId: socket.id
    });
    
    console.log('Comando enviado para ESP32s: iniciar leitura RFID');
  });

  // Admin cancela leitura RFID
  socket.on('stopRFIDReading', (data) => {
    console.log('Admin cancelou leitura RFID:', data);
    
    // Remover marcação de admin em leitura
    socket.isAdminReading = false;
    socket.leave('admin_rfid_reading');
    
    // Notificar todos os ESP32s para sair do modo de leitura
    io.emit('esp32Command', {
      command: 'stopRFIDReading',
      timestamp: new Date(),
      adminSocketId: socket.id
    });
    
    console.log('Comando enviado para ESP32s: parar leitura RFID');
  });

  // === EVENTOS ESP32 ===
  
  // ESP32 se conecta e se identifica
  socket.on('esp32_connected', (data) => {
    console.log('ESP32 conectado:', data);
    
    // Marcar socket como ESP32
    socket.isESP32 = true;
    socket.esp32Id = data.esp32Id;
    socket.esp32Version = data.version || '2.0';
    socket.esp32Features = data.features || '';
    
    // Entrar na sala dos ESP32s
    socket.join('esp32_devices');
    
    // Confirmar conexão
    socket.emit('esp32_connected_ack', {
      status: 'connected',
      serverId: socket.id,
      timestamp: new Date()
    });
    
    console.log(`ESP32 ${data.esp32Id} registrado com sucesso`);
  });

  // ESP32 envia leitura RFID (tanto para admin quanto para uso normal)
  socket.on('rfidRead', (data) => {
    console.log('RFID lido pelo ESP32:', data);
    
    // Validar dados recebidos
    if (!data.rfidTag || !data.esp32Id) {
      console.error('Dados RFID inválidos:', data);
      return;
    }
    
    // Adicionar informações do servidor
    const enrichedData = {
      ...data,
      serverId: socket.id,
      serverTimestamp: new Date(),
      processed: true
    };
    
    // Se há admin aguardando leitura, enviar para ele
    const adminSockets = Array.from(io.sockets.sockets.values())
      .filter(s => s.isAdminReading);
    
    if (adminSockets.length > 0 && data.source === 'admin_reading') {
      console.log('Enviando RFID para admin em modo de leitura');
      adminSockets.forEach(adminSocket => {
        adminSocket.emit('rfidRead', enrichedData);
      });
    } else {
      // Processamento normal do RFID (entrada/saída de alunos)
      console.log('Processando RFID para controle de acesso normal');
      
      // Emitir para todos os clientes conectados (painel do motorista, etc.)
      io.emit('rfidRead', enrichedData);
    }
  });

  // ESP32 reporta status
  socket.on('esp32Status', (data) => {
    console.log('Status ESP32:', data);
    
    // Atualizar informações do socket
    if (socket.isESP32) {
      socket.esp32Status = data.status;
      socket.esp32LastSeen = new Date();
    }
    
    // Emitir status para admins e clientes interessados
    socket.broadcast.emit('esp32Status', {
      ...data,
      serverId: socket.id,
      serverTimestamp: new Date()
    });
  });

  // ESP32 reporta erro
  socket.on('esp32Error', (data) => {
    console.error('Erro ESP32:', data);
    
    // Emitir erro para admins
    socket.broadcast.emit('esp32Error', {
      ...data,
      serverId: socket.id,
      serverTimestamp: new Date()
    });
  });

  // ESP32 responde a ping
  socket.on('pong', (data) => {
    console.log('Pong recebido do ESP32:', data);
    
    if (socket.isESP32) {
      socket.esp32LastPong = new Date();
    }
  });

  // === COMANDOS PARA ESP32 ===
  
  // Enviar comando específico para ESP32
  socket.on('sendESP32Command', (data) => {
    console.log('Comando para ESP32:', data);
    
    if (data.esp32Id) {
      // Enviar para ESP32 específico
      const targetSocket = Array.from(io.sockets.sockets.values())
        .find(s => s.isESP32 && s.esp32Id === data.esp32Id);
      
      if (targetSocket) {
        targetSocket.emit('esp32Command', data);
        console.log(`Comando enviado para ESP32 ${data.esp32Id}`);
      } else {
        console.log(`ESP32 ${data.esp32Id} não encontrado`);
        socket.emit('commandError', { 
          error: 'ESP32 não encontrado',
          esp32Id: data.esp32Id 
        });
      }
    } else {
      // Enviar para todos os ESP32s
      io.to('esp32_devices').emit('esp32Command', data);
      console.log('Comando enviado para todos os ESP32s');
    }
  });

  // Listar ESP32s conectados
  socket.on('listESP32s', () => {
    const esp32List = Array.from(io.sockets.sockets.values())
      .filter(s => s.isESP32)
      .map(s => ({
        socketId: s.id,
        esp32Id: s.esp32Id,
        version: s.esp32Version,
        features: s.esp32Features,
        status: s.esp32Status,
        lastSeen: s.esp32LastSeen,
        lastPong: s.esp32LastPong,
        connected: true
      }));
    
    socket.emit('esp32List', esp32List);
    console.log(`Lista de ESP32s enviada: ${esp32List.length} dispositivos`);
  });

  // Ping/Pong para manter conexão viva
  socket.on('ping', () => {
    socket.emit('pong');
  });

  // Desconexão
  socket.on('disconnect', () => {
    console.log(`Cliente desconectado: ${socket.id}`);
    
    // Se era um ESP32, remover da lista
    if (socket.isESP32) {
      console.log(`ESP32 ${socket.esp32Id} desconectado`);
      
      // Notificar outros clientes sobre desconexão do ESP32
      socket.broadcast.emit('esp32Disconnected', {
        esp32Id: socket.esp32Id,
        socketId: socket.id,
        timestamp: new Date()
      });
    }
    
    // Se era um admin em modo de leitura, cancelar
    if (socket.isAdminReading) {
      console.log('Admin em modo de leitura desconectou, cancelando leitura RFID');
      io.emit('esp32Command', {
        command: 'stopRFIDReading',
        timestamp: new Date(),
        reason: 'admin_disconnected'
      });
    }
  });
});

// Middleware de erro global
app.use((err, req, res, next) => {
  console.error('Erro:', err.stack);
  res.status(500).json({ 
    error: 'Erro interno do servidor',
    message: process.env.NODE_ENV === 'development' ? err.message : undefined
  });
});

// Rota 404
app.use('*', (req, res) => {
  res.status(404).json({ error: 'Rota não encontrada' });
});

const PORT = process.env.PORT || 3000;

server.listen(PORT, '0.0.0.0', () => {
  console.log(`
🚐 VansControl Server iniciado!
📡 Servidor rodando na porta ${PORT}
🔗 API: http://localhost:${PORT}/api
💾 MongoDB: Conectando...
🌐 WebSocket: Ativo
  `);
});

// Graceful shutdown
process.on('SIGTERM', () => {
  console.log('SIGTERM recebido. Fechando servidor...');
  server.close(() => {
    console.log('Servidor fechado.');
  });
});

module.exports = app; 