const mongoose = require('mongoose');

const vanSchema = new mongoose.Schema({
  placa: {
    type: String,
    required: true,
    unique: true,
    trim: true,
    uppercase: true
  },
  modelo: {
    type: String,
    required: true,
    trim: true
  },
  ano: {
    type: Number,
    required: true
  },
  capacidade: {
    type: Number,
    required: true,
    min: 1
  },
  motorista: {
    type: mongoose.Schema.Types.ObjectId,
    ref: 'Motorista',
    default: null
  },
  esp32Id: {
    type: String,
    unique: true,
    sparse: true,
    trim: true
  },
  rota: {
    type: String,
    trim: true
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

module.exports = mongoose.model('Van', vanSchema); 