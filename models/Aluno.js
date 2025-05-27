const mongoose = require('mongoose');

const alunoSchema = new mongoose.Schema({
  nome: {
    type: String,
    required: true,
    trim: true
  },
  matricula: {
    type: String,
    required: true,
    unique: true,
    trim: true
  },
  rfidTag: {
    type: String,
    required: true,
    unique: true,
    trim: true
  },
  telefoneResponsavel: {
    type: String,
    required: true,
    trim: true
  },
  nomeResponsavel: {
    type: String,
    required: true,
    trim: true
  },
  endereco: {
    rua: String,
    numero: String,
    bairro: String,
    cidade: String,
    cep: String
  },
  van: {
    type: mongoose.Schema.Types.ObjectId,
    ref: 'Van',
    required: true
  },
  turno: {
    type: String,
    enum: ['manhã', 'tarde', 'noite'],
    required: true
  },
  ativo: {
    type: Boolean,
    default: true
  },
  dataCadastro: {
    type: Date,
    default: Date.now
  }
}, {
  timestamps: true
});

module.exports = mongoose.model('Aluno', alunoSchema); 