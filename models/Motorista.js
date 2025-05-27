const mongoose = require('mongoose');

const motoristaSchema = new mongoose.Schema({
  nome: {
    type: String,
    required: true,
    trim: true
  },
  cnh: {
    type: String,
    required: true,
    unique: true,
    trim: true
  },
  telefone: {
    type: String,
    required: true,
    trim: true
  },
  email: {
    type: String,
    required: true,
    unique: true,
    trim: true,
    lowercase: true
  },
  senha: {
    type: String,
    required: true
  },
  van: {
    type: mongoose.Schema.Types.ObjectId,
    ref: 'Van',
    default: null
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

module.exports = mongoose.model('Motorista', motoristaSchema); 