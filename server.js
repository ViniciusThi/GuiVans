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

  // ESP32 envia leitura RFID (tanto para admin quanto para uso normal)
  socket.on('rfidRead', (data) => {
    console.log('RFID lido pelo ESP32:', data);
    
    // Validar dados recebidos
    if (!data.rfidTag || !data.esp32Id) {
      console.error('Dados RFID inválidos:', data);
      return;
    }
    
    // Se há admin aguardando leitura, enviar para ele
    const adminSockets = Array.from(io.sockets.sockets.values())
      .filter(s => s.isAdminReading);
    
    if (adminSockets.length > 0) {
      console.log('Enviando RFID para admin em modo de leitura');
      adminSockets.forEach(adminSocket => {
        adminSocket.emit('rfidRead', {
          rfidTag: data.rfidTag,
          esp32Id: data.esp32Id,
          timestamp: new Date(),
          source: 'admin_reading'
        });
      });
    } else {
      // Processamento normal do RFID (entrada/saída de alunos)
      console.log('Processando RFID para controle de acesso normal');
      
      // Emitir para todos os clientes conectados (painel do motorista, etc.)
      io.emit('rfidRead', {
        rfidTag: data.rfidTag,
        esp32Id: data.esp32Id,
        timestamp: new Date(),
        source: 'access_control'
      });
    }
  });

  // ESP32 reporta status
  socket.on('esp32Status', (data) => {
    console.log('Status ESP32:', data);
    
    // Emitir status para admins
    socket.broadcast.emit('esp32Status', {
      ...data,
      timestamp: new Date()
    });
  });

  // ESP32 reporta erro
  socket.on('esp32Error', (data) => {
    console.error('Erro ESP32:', data);
    
    // Emitir erro para admins
    socket.broadcast.emit('esp32Error', {
      ...data,
      timestamp: new Date()
    });
  });

  // Ping/Pong para manter conexão viva
  socket.on('ping', () => {
    socket.emit('pong');
  });

  // Desconexão
  socket.on('disconnect', () => {
    console.log(`Cliente desconectado: ${socket.id}`);
    
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