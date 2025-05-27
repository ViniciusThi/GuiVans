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

  // Ping/Pong para manter conexão viva
  socket.on('ping', () => {
    socket.emit('pong');
  });

  // Desconexão
  socket.on('disconnect', () => {
    console.log(`Cliente desconectado: ${socket.id}`);
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