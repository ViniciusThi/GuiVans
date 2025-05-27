const express = require('express');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const Motorista = require('../models/Motorista');
const Van = require('../models/Van');

const router = express.Router();

// GET /api/motoristas - Listar todos os motoristas
router.get('/', async (req, res) => {
  try {
    const motoristas = await Motorista.find({ ativo: true })
      .populate('van', 'placa modelo')
      .select('-senha');
    res.json(motoristas);
  } catch (error) {
    res.status(500).json({ error: 'Erro ao buscar motoristas' });
  }
});

// POST /api/motoristas - Criar novo motorista
router.post('/', async (req, res) => {
  try {
    const { nome, cnh, telefone, email, senha } = req.body;

    // Verificar se motorista já existe
    const motoristaExistente = await Motorista.findOne({ 
      $or: [{ email }, { cnh }] 
    });

    if (motoristaExistente) {
      return res.status(400).json({ 
        error: 'Motorista já cadastrado com este email ou CNH' 
      });
    }

    // Hash da senha
    const senhaHash = await bcrypt.hash(senha, 12);

    const motorista = new Motorista({
      nome,
      cnh,
      telefone,
      email,
      senha: senhaHash
    });

    await motorista.save();
    
    // Retornar sem a senha
    const motoristaResponse = await Motorista.findById(motorista._id).select('-senha');
    res.status(201).json(motoristaResponse);
  } catch (error) {
    res.status(400).json({ error: 'Erro ao criar motorista' });
  }
});

// POST /api/motoristas/login - Login do motorista
router.post('/login', async (req, res) => {
  try {
    const { email, senha } = req.body;

    const motorista = await Motorista.findOne({ email, ativo: true })
      .populate('van');

    if (!motorista) {
      return res.status(401).json({ error: 'Credenciais inválidas' });
    }

    const senhaValida = await bcrypt.compare(senha, motorista.senha);
    if (!senhaValida) {
      return res.status(401).json({ error: 'Credenciais inválidas' });
    }

    const token = jwt.sign(
      { 
        motoristaId: motorista._id,
        vanId: motorista.van?._id 
      },
      process.env.JWT_SECRET || 'secretkey123',
      { expiresIn: '24h' }
    );

    res.json({
      token,
      motorista: {
        _id: motorista._id,
        nome: motorista.nome,
        email: motorista.email,
        van: motorista.van
      }
    });
  } catch (error) {
    res.status(500).json({ error: 'Erro no login' });
  }
});

// PUT /api/motoristas/:id/van - Associar van ao motorista
router.put('/:id/van', async (req, res) => {
  try {
    const { vanId } = req.body;
    const motoristaId = req.params.id;

    // Verificar se a van existe e não está associada a outro motorista
    const van = await Van.findById(vanId);
    if (!van) {
      return res.status(404).json({ error: 'Van não encontrada' });
    }

    if (van.motorista && van.motorista.toString() !== motoristaId) {
      return res.status(400).json({ error: 'Van já está associada a outro motorista' });
    }

    // Atualizar motorista
    const motorista = await Motorista.findByIdAndUpdate(
      motoristaId,
      { van: vanId },
      { new: true }
    ).populate('van');

    // Atualizar van
    await Van.findByIdAndUpdate(vanId, { motorista: motoristaId });

    res.json(motorista);
  } catch (error) {
    res.status(400).json({ error: 'Erro ao associar van' });
  }
});

module.exports = router; 