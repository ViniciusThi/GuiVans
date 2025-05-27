const mongoose = require('mongoose');

const registroSchema = new mongoose.Schema({
  aluno: {
    type: mongoose.Schema.Types.ObjectId,
    ref: 'Aluno',
    required: false
  },
  van: {
    type: mongoose.Schema.Types.ObjectId,
    ref: 'Van',
    required: true
  },
  motorista: {
    type: mongoose.Schema.Types.ObjectId,
    ref: 'Motorista',
    required: false
  },
  rfidTag: {
    type: String,
    required: true,
    trim: true
  },
  tipo: {
    type: String,
    enum: ['entrada', 'saida'],
    required: true
  },
  dataHora: {
    type: Date,
    default: Date.now
  },
  status: {
    type: String,
    enum: ['autorizado', 'negado', 'desconhecido'],
    default: 'autorizado'
  },
  localizacao: {
    latitude: Number,
    longitude: Number
  }
}, {
  timestamps: true
});

// Index para otimizar consultas por van e data
registroSchema.index({ van: 1, dataHora: -1 });
registroSchema.index({ aluno: 1, dataHora: -1 });
registroSchema.index({ rfidTag: 1, dataHora: -1 });

module.exports = mongoose.model('Registro', registroSchema); 