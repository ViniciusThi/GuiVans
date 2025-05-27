const mongoose = require('mongoose');
const bcrypt = require('bcryptjs');
require('dotenv').config();

// Importar modelos
const Motorista = require('./models/Motorista');
const Van = require('./models/Van');
const Aluno = require('./models/Aluno');

const setup = async () => {
  try {
    console.log('🚐 VansControl - Configuração Inicial');
    console.log('=====================================');

    // Conectar ao MongoDB
    await mongoose.connect(process.env.MONGODB_URI || 'mongodb://localhost:27017/vanscontrol', {
      useNewUrlParser: true,
      useUnifiedTopology: true,
    });
    console.log('✅ Conectado ao MongoDB');

    // Verificar se já existe um motorista
    const motoristaExistente = await Motorista.findOne();
    if (motoristaExistente) {
      console.log('⚠️  Já existe um motorista cadastrado no sistema');
      console.log('📧 Email:', motoristaExistente.email);
      process.exit(0);
    }

    // Criar motorista padrão
    const senhaHash = await bcrypt.hash('123456', 12);
    const motorista = new Motorista({
      nome: 'Motorista Teste',
      cnh: '12345678901',
      telefone: '(11) 99999-9999',
      email: 'motorista@teste.com',
      senha: senhaHash
    });
    await motorista.save();
    console.log('✅ Motorista padrão criado com sucesso!');

    // Criar van padrão
    const van = new Van({
      placa: 'VAN-0001',
      modelo: 'Sprinter',
      ano: 2020,
      capacidade: 15,
      esp32Id: 'ESP32_VAN_001',
      rota: 'Centro - Escola',
      motorista: motorista._id
    });
    await van.save();
    console.log('✅ Van padrão criada com sucesso!');

    // Associar van ao motorista
    motorista.van = van._id;
    await motorista.save();
    console.log('✅ Van associada ao motorista!');

    // Criar alguns alunos de exemplo
    const alunos = [
      {
        nome: 'João Silva',
        matricula: '2024001',
        rfidTag: 'A1B2C3D4',
        telefoneResponsavel: '(11) 88888-8888',
        nomeResponsavel: 'Maria Silva',
        endereco: {
          rua: 'Rua das Flores, 123',
          bairro: 'Centro',
          cidade: 'São Paulo',
          cep: '01000-000'
        },
        van: van._id,
        turno: 'manhã'
      },
      {
        nome: 'Ana Santos',
        matricula: '2024002',
        rfidTag: 'E5F6G7H8',
        telefoneResponsavel: '(11) 77777-7777',
        nomeResponsavel: 'Carlos Santos',
        endereco: {
          rua: 'Av. Principal, 456',
          bairro: 'Vila Nova',
          cidade: 'São Paulo',
          cep: '02000-000'
        },
        van: van._id,
        turno: 'manhã'
      }
    ];

    for (const alunoData of alunos) {
      const aluno = new Aluno(alunoData);
      await aluno.save();
      console.log(`✅ Aluno ${alunoData.nome} criado com RFID: ${alunoData.rfidTag}`);
    }

    console.log('\n🎉 Configuração inicial concluída!');
    console.log('===================================');
    console.log('📧 Email do motorista: motorista@teste.com');
    console.log('🔐 Senha: 123456');
    console.log('🚐 Van: VAN-0001 (ESP32_VAN_001)');
    console.log('🎫 RFID dos alunos:');
    console.log('   • João Silva: A1B2C3D4');
    console.log('   • Ana Santos: E5F6G7H8');
    console.log('\n🌐 Acesse: http://localhost:3000');
    console.log('===================================');

    process.exit(0);
  } catch (error) {
    console.error('❌ Erro na configuração:', error);
    process.exit(1);
  }
};

// Executar configuração
setup(); 