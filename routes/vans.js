const express = require('express');
const Van = require('../models/Van');
const Aluno = require('../models/Aluno');

const router = express.Router();

// GET /api/vans - Listar todas as vans
router.get('/', async (req, res) => {
  try {
    const vans = await Van.find({ ativo: true })
      .populate('motorista', 'nome email telefone');
    res.json(vans);
  } catch (error) {
    res.status(500).json({ error: 'Erro ao buscar vans' });
  }
});

// GET /api/vans/:id - Buscar van específica
router.get('/:id', async (req, res) => {
  try {
    const van = await Van.findById(req.params.id)
      .populate('motorista', 'nome email telefone');
    
    if (!van) {
      return res.status(404).json({ error: 'Van não encontrada' });
    }

    // Buscar alunos da van
    const alunos = await Aluno.find({ van: van._id, ativo: true });
    
    res.json({
      ...van.toObject(),
      alunos,
      totalAlunos: alunos.length
    });
  } catch (error) {
    res.status(500).json({ error: 'Erro ao buscar van' });
  }
});

// POST /api/vans - Criar nova van
router.post('/', async (req, res) => {
  try {
    const { placa, modelo, ano, capacidade, esp32Id, rota } = req.body;

    // Verificar se van já existe
    const vanExistente = await Van.findOne({ placa });
    if (vanExistente) {
      return res.status(400).json({ error: 'Van já cadastrada com esta placa' });
    }

    // Verificar ESP32 ID único se fornecido
    if (esp32Id) {
      const esp32Existente = await Van.findOne({ esp32Id });
      if (esp32Existente) {
        return res.status(400).json({ error: 'ESP32 ID já está em uso' });
      }
    }

    const van = new Van({
      placa: placa.toUpperCase(),
      modelo,
      ano,
      capacidade,
      esp32Id,
      rota
    });

    await van.save();
    res.status(201).json(van);
  } catch (error) {
    res.status(400).json({ error: 'Erro ao criar van' });
  }
});

// PUT /api/vans/:id - Atualizar van
router.put('/:id', async (req, res) => {
  try {
    const { placa, modelo, ano, capacidade, esp32Id, rota } = req.body;
    
    const van = await Van.findByIdAndUpdate(
      req.params.id,
      {
        placa: placa?.toUpperCase(),
        modelo,
        ano,
        capacidade,
        esp32Id,
        rota
      },
      { new: true, runValidators: true }
    );

    if (!van) {
      return res.status(404).json({ error: 'Van não encontrada' });
    }

    res.json(van);
  } catch (error) {
    res.status(400).json({ error: 'Erro ao atualizar van' });
  }
});

// PUT /api/vans/:id/esp32 - Associar ESP32 à van
router.put('/:id/esp32', async (req, res) => {
  try {
    const { esp32Id } = req.body;

    // Verificar se ESP32 ID já está em uso
    const esp32EmUso = await Van.findOne({ 
      esp32Id, 
      _id: { $ne: req.params.id } 
    });

    if (esp32EmUso) {
      return res.status(400).json({ error: 'ESP32 ID já está em uso' });
    }

    const van = await Van.findByIdAndUpdate(
      req.params.id,
      { esp32Id },
      { new: true }
    );

    if (!van) {
      return res.status(404).json({ error: 'Van não encontrada' });
    }

    res.json(van);
  } catch (error) {
    res.status(400).json({ error: 'Erro ao associar ESP32' });
  }
});

// DELETE /api/vans/:id - Desativar van
router.delete('/:id', async (req, res) => {
  try {
    const van = await Van.findByIdAndUpdate(
      req.params.id,
      { ativo: false },
      { new: true }
    );

    if (!van) {
      return res.status(404).json({ error: 'Van não encontrada' });
    }

    res.json({ message: 'Van desativada com sucesso' });
  } catch (error) {
    res.status(400).json({ error: 'Erro ao desativar van' });
  }
});

module.exports = router; 