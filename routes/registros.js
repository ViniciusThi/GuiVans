const express = require('express');
const Registro = require('../models/Registro');
const Aluno = require('../models/Aluno');
const Van = require('../models/Van');

const router = express.Router();

// GET /api/registros - Listar registros com filtros
router.get('/', async (req, res) => {
  try {
    const { van, aluno, dataInicio, dataFim, tipo } = req.query;
    let filtro = {};

    if (van) filtro.van = van;
    if (aluno) filtro.aluno = aluno;
    if (tipo) filtro.tipo = tipo;

    if (dataInicio || dataFim) {
      filtro.dataHora = {};
      if (dataInicio) filtro.dataHora.$gte = new Date(dataInicio);
      if (dataFim) filtro.dataHora.$lte = new Date(dataFim);
    }

    const registros = await Registro.find(filtro)
      .populate('aluno', 'nome matricula')
      .populate('van', 'placa modelo')
      .populate('motorista', 'nome')
      .sort({ dataHora: -1 })
      .limit(100);

    res.json(registros);
  } catch (error) {
    res.status(500).json({ error: 'Erro ao buscar registros' });
  }
});

// GET /api/registros/van/:vanId/hoje - Registros de hoje de uma van
router.get('/van/:vanId/hoje', async (req, res) => {
  try {
    const hoje = new Date();
    const inicioHoje = new Date(hoje.getFullYear(), hoje.getMonth(), hoje.getDate());
    const fimHoje = new Date(inicioHoje.getTime() + 24 * 60 * 60 * 1000);

    const registros = await Registro.find({
      van: req.params.vanId,
      dataHora: {
        $gte: inicioHoje,
        $lt: fimHoje
      }
    })
    .populate('aluno', 'nome matricula')
    .sort({ dataHora: -1 });

    res.json(registros);
  } catch (error) {
    res.status(500).json({ error: 'Erro ao buscar registros de hoje' });
  }
});

// POST /api/registros/rfid - Endpoint para ESP32 enviar leitura RFID
router.post('/rfid', async (req, res) => {
  try {
    const { rfidTag, esp32Id, tipo = 'entrada', localizacao } = req.body;

    console.log(`[ESP32 ${esp32Id}] RFID detectado: ${rfidTag} - Tipo: ${tipo}`);

    // Buscar a van pelo ESP32 ID
    const van = await Van.findOne({ esp32Id, ativo: true })
      .populate('motorista');

    if (!van) {
      return res.status(404).json({ 
        error: 'Van não encontrada para este ESP32',
        status: 'negado',
        ledColor: 'red',
        buzzer: false
      });
    }

    // Buscar o aluno pelo RFID
    const aluno = await Aluno.findOne({ 
      rfidTag, 
      ativo: true,
      van: van._id  // Verificar se o aluno pertence a esta van
    });

    if (!aluno) {
      console.log(`[ESP32 ${esp32Id}] Aluno não encontrado ou não autorizado para esta van`);
      
      // Registrar tentativa não autorizada
      const registroNegado = new Registro({
        van: van._id,
        motorista: van.motorista?._id,
        rfidTag,
        tipo,
        status: 'desconhecido',
        dataHora: new Date(),
        localizacao
      });
      
      await registroNegado.save();

      return res.json({
        status: 'negado',
        message: 'Aluno não autorizado',
        ledColor: 'red',
        buzzer: false
      });
    }

    // Criar registro de entrada/saída
    const registro = new Registro({
      aluno: aluno._id,
      van: van._id,
      motorista: van.motorista?._id,
      rfidTag,
      tipo,
      status: 'autorizado',
      dataHora: new Date(),
      localizacao
    });

    await registro.save();

    // Emitir evento via WebSocket para o motorista
    if (req.io) {
      req.io.to(`van_${van._id}`).emit('novoRegistro', {
        registro: {
          ...registro.toObject(),
          aluno: {
            _id: aluno._id,
            nome: aluno.nome,
            matricula: aluno.matricula
          }
        },
        timestamp: new Date()
      });
    }

    console.log(`[ESP32 ${esp32Id}] Registro autorizado: ${aluno.nome} - ${tipo}`);

    res.json({
      status: 'autorizado',
      message: `${tipo} autorizada para ${aluno.nome}`,
      aluno: {
        nome: aluno.nome,
        matricula: aluno.matricula
      },
      ledColor: 'green',
      buzzer: true
    });

  } catch (error) {
    console.error('Erro ao processar RFID:', error);
    res.status(500).json({ 
      error: 'Erro interno do servidor',
      status: 'erro',
      ledColor: 'red',
      buzzer: false
    });
  }
});

// GET /api/registros/estatisticas/:vanId - Estatísticas da van
router.get('/estatisticas/:vanId', async (req, res) => {
  try {
    const hoje = new Date();
    const inicioHoje = new Date(hoje.getFullYear(), hoje.getMonth(), hoje.getDate());
    const fimHoje = new Date(inicioHoje.getTime() + 24 * 60 * 60 * 1000);

    const [entradas, saidas, totalAlunos] = await Promise.all([
      Registro.countDocuments({
        van: req.params.vanId,
        tipo: 'entrada',
        status: 'autorizado',
        dataHora: { $gte: inicioHoje, $lt: fimHoje }
      }),
      Registro.countDocuments({
        van: req.params.vanId,
        tipo: 'saida',
        status: 'autorizado',
        dataHora: { $gte: inicioHoje, $lt: fimHoje }
      }),
      Aluno.countDocuments({
        van: req.params.vanId,
        ativo: true
      })
    ]);

    res.json({
      hoje: {
        entradas,
        saidas,
        presentesNaVan: entradas - saidas
      },
      totalAlunos
    });
  } catch (error) {
    res.status(500).json({ error: 'Erro ao buscar estatísticas' });
  }
});

module.exports = router; 