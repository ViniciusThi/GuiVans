const express = require('express');
const Aluno = require('../models/Aluno');
const Van = require('../models/Van');

const router = express.Router();

// GET /api/alunos - Listar todos os alunos
router.get('/', async (req, res) => {
  try {
    const { van, turno } = req.query;
    let filtro = { ativo: true };

    if (van) filtro.van = van;
    if (turno) filtro.turno = turno;

    const alunos = await Aluno.find(filtro)
      .populate('van', 'placa modelo')
      .sort({ nome: 1 });
    
    res.json(alunos);
  } catch (error) {
    res.status(500).json({ error: 'Erro ao buscar alunos' });
  }
});

// GET /api/alunos/:id - Buscar aluno específico
router.get('/:id', async (req, res) => {
  try {
    const aluno = await Aluno.findById(req.params.id)
      .populate('van', 'placa modelo motorista');
    
    if (!aluno) {
      return res.status(404).json({ error: 'Aluno não encontrado' });
    }

    res.json(aluno);
  } catch (error) {
    res.status(500).json({ error: 'Erro ao buscar aluno' });
  }
});

// GET /api/alunos/rfid/:rfidTag - Buscar aluno por RFID
router.get('/rfid/:rfidTag', async (req, res) => {
  try {
    const aluno = await Aluno.findOne({ 
      rfidTag: req.params.rfidTag, 
      ativo: true 
    }).populate('van', 'placa modelo _id');
    
    if (!aluno) {
      return res.status(404).json({ error: 'Aluno não encontrado' });
    }

    res.json(aluno);
  } catch (error) {
    res.status(500).json({ error: 'Erro ao buscar aluno por RFID' });
  }
});

// POST /api/alunos - Criar novo aluno
router.post('/', async (req, res) => {
  try {
    const { 
      nome, 
      matricula, 
      rfidTag, 
      telefoneResponsavel, 
      nomeResponsavel, 
      endereco, 
      van, 
      turno 
    } = req.body;

    // Verificar se aluno já existe
    const alunoExistente = await Aluno.findOne({ 
      $or: [{ matricula }, { rfidTag }] 
    });

    if (alunoExistente) {
      return res.status(400).json({ 
        error: 'Aluno já cadastrado com esta matrícula ou RFID' 
      });
    }

    // Verificar se a van existe
    const vanExiste = await Van.findById(van);
    if (!vanExiste) {
      return res.status(400).json({ error: 'Van não encontrada' });
    }

    // Verificar capacidade da van
    const alunosNaVan = await Aluno.countDocuments({ van, ativo: true });
    if (alunosNaVan >= vanExiste.capacidade) {
      return res.status(400).json({ error: 'Van já está na capacidade máxima' });
    }

    const aluno = new Aluno({
      nome,
      matricula,
      rfidTag,
      telefoneResponsavel,
      nomeResponsavel,
      endereco,
      van,
      turno
    });

    await aluno.save();
    
    const alunoResponse = await Aluno.findById(aluno._id)
      .populate('van', 'placa modelo');
    
    res.status(201).json(alunoResponse);
  } catch (error) {
    res.status(400).json({ error: 'Erro ao criar aluno' });
  }
});

// PUT /api/alunos/:id - Atualizar aluno
router.put('/:id', async (req, res) => {
  try {
    const { 
      nome, 
      matricula, 
      rfidTag, 
      telefoneResponsavel, 
      nomeResponsavel, 
      endereco, 
      van, 
      turno 
    } = req.body;

    // Se mudando van, verificar capacidade
    if (van) {
      const vanExiste = await Van.findById(van);
      if (!vanExiste) {
        return res.status(400).json({ error: 'Van não encontrada' });
      }

      const alunoAtual = await Aluno.findById(req.params.id);
      if (alunoAtual && alunoAtual.van.toString() !== van) {
        const alunosNaVan = await Aluno.countDocuments({ van, ativo: true });
        if (alunosNaVan >= vanExiste.capacidade) {
          return res.status(400).json({ error: 'Van já está na capacidade máxima' });
        }
      }
    }

    const aluno = await Aluno.findByIdAndUpdate(
      req.params.id,
      {
        nome,
        matricula,
        rfidTag,
        telefoneResponsavel,
        nomeResponsavel,
        endereco,
        van,
        turno
      },
      { new: true, runValidators: true }
    ).populate('van', 'placa modelo');

    if (!aluno) {
      return res.status(404).json({ error: 'Aluno não encontrado' });
    }

    res.json(aluno);
  } catch (error) {
    res.status(400).json({ error: 'Erro ao atualizar aluno' });
  }
});

// DELETE /api/alunos/:id - Desativar aluno
router.delete('/:id', async (req, res) => {
  try {
    const aluno = await Aluno.findByIdAndUpdate(
      req.params.id,
      { ativo: false },
      { new: true }
    );

    if (!aluno) {
      return res.status(404).json({ error: 'Aluno não encontrado' });
    }

    res.json({ message: 'Aluno desativado com sucesso' });
  } catch (error) {
    res.status(400).json({ error: 'Erro ao desativar aluno' });
  }
});

module.exports = router; 