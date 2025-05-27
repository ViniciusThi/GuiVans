// Configuração do Socket.IO
const socket = io();

// Estado da aplicação
let isReadingRFID = false;
let rfidTimeout = null;

// Inicialização
document.addEventListener('DOMContentLoaded', function() {
    initializeAdmin();
    setupEventListeners();
    setupSocketListeners();
    loadInitialData();
});

// Configuração inicial
function initializeAdmin() {
    console.log('Painel Administrativo iniciado');
}

// Event Listeners
function setupEventListeners() {
    // Formulário de Motorista
    document.getElementById('formMotorista').addEventListener('submit', handleMotoristaSubmit);
    
    // Formulário de Van
    document.getElementById('formVan').addEventListener('submit', handleVanSubmit);
    
    // Formulário de Aluno
    document.getElementById('formAluno').addEventListener('submit', handleAlunoSubmit);
    
    // Botão de Ler RFID
    document.getElementById('btnLerRFID').addEventListener('click', handleLerRFID);
    
    // Tabs
    document.querySelectorAll('[data-bs-toggle="pill"]').forEach(tab => {
        tab.addEventListener('shown.bs.tab', function(e) {
            const target = e.target.getAttribute('href');
            if (target === '#registros') {
                loadRegistros();
            }
        });
    });
}

// Socket Listeners
function setupSocketListeners() {
    // Escutar leitura RFID do ESP32
    socket.on('rfidRead', function(data) {
        console.log('RFID lido:', data);
        handleRFIDRead(data);
    });
    
    // Escutar novos registros
    socket.on('novoRegistro', function(data) {
        console.log('Novo registro:', data);
        if (document.querySelector('#registros.active')) {
            loadRegistros();
        }
    });
    
    // Status de conexão
    socket.on('connect', function() {
        console.log('Conectado ao servidor');
        showNotification('Conectado ao servidor', 'success');
    });
    
    socket.on('disconnect', function() {
        console.log('Desconectado do servidor');
        showNotification('Desconectado do servidor', 'warning');
    });
}

// Carregar dados iniciais
function loadInitialData() {
    loadMotoristas();
    loadVans();
    loadAlunos();
    loadVansSelect();
}

// === MOTORISTAS ===
async function loadMotoristas() {
    try {
        const response = await fetch('/api/motoristas');
        const motoristas = await response.json();
        
        const tbody = document.getElementById('listaMotoristas');
        tbody.innerHTML = '';
        
        if (motoristas.length === 0) {
            tbody.innerHTML = '<tr><td colspan="5" class="text-center">Nenhum motorista cadastrado</td></tr>';
            return;
        }
        
        motoristas.forEach(motorista => {
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${motorista.nome}</td>
                <td>${motorista.cnh}</td>
                <td>${motorista.email}</td>
                <td>${motorista.van ? motorista.van.placa : '<span class="badge bg-secondary">Sem van</span>'}</td>
                <td>
                    <button class="btn btn-sm btn-warning" onclick="editMotorista('${motorista._id}')">
                        <i class="fas fa-edit"></i>
                    </button>
                    <button class="btn btn-sm btn-danger" onclick="deleteMotorista('${motorista._id}')">
                        <i class="fas fa-trash"></i>
                    </button>
                </td>
            `;
            tbody.appendChild(row);
        });
    } catch (error) {
        console.error('Erro ao carregar motoristas:', error);
        showNotification('Erro ao carregar motoristas', 'error');
    }
}

async function handleMotoristaSubmit(e) {
    e.preventDefault();
    
    const formData = new FormData(e.target);
    const data = Object.fromEntries(formData);
    
    try {
        const response = await fetch('/api/motoristas', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(data)
        });
        
        if (response.ok) {
            showNotification('Motorista cadastrado com sucesso!', 'success');
            e.target.reset();
            loadMotoristas();
        } else {
            const error = await response.json();
            showNotification(error.message || 'Erro ao cadastrar motorista', 'error');
        }
    } catch (error) {
        console.error('Erro:', error);
        showNotification('Erro ao cadastrar motorista', 'error');
    }
}

// === VANS ===
async function loadVans() {
    try {
        const response = await fetch('/api/vans');
        const vans = await response.json();
        
        const tbody = document.getElementById('listaVans');
        tbody.innerHTML = '';
        
        if (vans.length === 0) {
            tbody.innerHTML = '<tr><td colspan="6" class="text-center">Nenhuma van cadastrada</td></tr>';
            return;
        }
        
        vans.forEach(van => {
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${van.placa}</td>
                <td>${van.modelo} ${van.ano}</td>
                <td>${van.capacidade} lugares</td>
                <td><code>${van.esp32Id || 'Não definido'}</code></td>
                <td>${van.motorista ? van.motorista.nome : '<span class="badge bg-secondary">Sem motorista</span>'}</td>
                <td>
                    <button class="btn btn-sm btn-warning" onclick="editVan('${van._id}')">
                        <i class="fas fa-edit"></i>
                    </button>
                    <button class="btn btn-sm btn-danger" onclick="deleteVan('${van._id}')">
                        <i class="fas fa-trash"></i>
                    </button>
                </td>
            `;
            tbody.appendChild(row);
        });
    } catch (error) {
        console.error('Erro ao carregar vans:', error);
        showNotification('Erro ao carregar vans', 'error');
    }
}

async function loadVansSelect() {
    try {
        const response = await fetch('/api/vans');
        const vans = await response.json();
        
        const select = document.getElementById('selectVan');
        select.innerHTML = '<option value="">Selecione uma van</option>';
        
        vans.forEach(van => {
            const option = document.createElement('option');
            option.value = van._id;
            option.textContent = `${van.placa} - ${van.modelo}`;
            select.appendChild(option);
        });
    } catch (error) {
        console.error('Erro ao carregar vans para select:', error);
    }
}

async function handleVanSubmit(e) {
    e.preventDefault();
    
    const formData = new FormData(e.target);
    const data = Object.fromEntries(formData);
    
    try {
        const response = await fetch('/api/vans', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(data)
        });
        
        if (response.ok) {
            showNotification('Van cadastrada com sucesso!', 'success');
            e.target.reset();
            loadVans();
            loadVansSelect();
        } else {
            const error = await response.json();
            showNotification(error.message || 'Erro ao cadastrar van', 'error');
        }
    } catch (error) {
        console.error('Erro:', error);
        showNotification('Erro ao cadastrar van', 'error');
    }
}

// === ALUNOS ===
async function loadAlunos() {
    try {
        const response = await fetch('/api/alunos');
        const alunos = await response.json();
        
        const tbody = document.getElementById('listaAlunos');
        tbody.innerHTML = '';
        
        if (alunos.length === 0) {
            tbody.innerHTML = '<tr><td colspan="6" class="text-center">Nenhum aluno cadastrado</td></tr>';
            return;
        }
        
        alunos.forEach(aluno => {
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${aluno.nome}</td>
                <td>${aluno.matricula}</td>
                <td><code>${aluno.rfidTag}</code></td>
                <td>${aluno.van ? aluno.van.placa : '<span class="badge bg-secondary">Sem van</span>'}</td>
                <td><span class="badge bg-info">${aluno.turno}</span></td>
                <td>
                    <button class="btn btn-sm btn-warning" onclick="editAluno('${aluno._id}')">
                        <i class="fas fa-edit"></i>
                    </button>
                    <button class="btn btn-sm btn-danger" onclick="deleteAluno('${aluno._id}')">
                        <i class="fas fa-trash"></i>
                    </button>
                </td>
            `;
            tbody.appendChild(row);
        });
    } catch (error) {
        console.error('Erro ao carregar alunos:', error);
        showNotification('Erro ao carregar alunos', 'error');
    }
}

async function handleAlunoSubmit(e) {
    e.preventDefault();
    
    const formData = new FormData(e.target);
    const data = Object.fromEntries(formData);
    
    // Validar se RFID foi lido
    if (!data.rfidTag) {
        showNotification('Por favor, leia o cartão RFID antes de salvar', 'warning');
        return;
    }
    
    try {
        const response = await fetch('/api/alunos', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(data)
        });
        
        if (response.ok) {
            showNotification('Aluno cadastrado com sucesso!', 'success');
            e.target.reset();
            document.getElementById('rfidTagInput').value = '';
            hideRFIDStatus();
            loadAlunos();
        } else {
            const error = await response.json();
            showNotification(error.message || 'Erro ao cadastrar aluno', 'error');
        }
    } catch (error) {
        console.error('Erro:', error);
        showNotification('Erro ao cadastrar aluno', 'error');
    }
}

// === LEITURA RFID ===
function handleLerRFID() {
    if (isReadingRFID) {
        // Cancelar leitura
        cancelRFIDReading();
        return;
    }
    
    // Iniciar leitura
    startRFIDReading();
}

function startRFIDReading() {
    isReadingRFID = true;
    
    const btn = document.getElementById('btnLerRFID');
    const status = document.getElementById('rfidStatus');
    
    // Atualizar botão
    btn.classList.add('reading');
    btn.innerHTML = '<i class="fas fa-stop me-2"></i>Cancelar';
    
    // Mostrar status
    status.className = 'rfid-status rfid-waiting';
    status.style.display = 'block';
    status.innerHTML = '<i class="fas fa-spinner fa-spin me-2"></i>Aguardando leitura do cartão RFID...';
    
    // Enviar comando para ESP32
    socket.emit('startRFIDReading', { action: 'start' });
    
    // Timeout de 30 segundos
    rfidTimeout = setTimeout(() => {
        cancelRFIDReading();
        showRFIDError('Timeout: Nenhum cartão foi detectado em 30 segundos');
    }, 30000);
    
    console.log('Iniciando leitura RFID...');
}

function cancelRFIDReading() {
    isReadingRFID = false;
    
    const btn = document.getElementById('btnLerRFID');
    
    // Restaurar botão
    btn.classList.remove('reading');
    btn.innerHTML = '<i class="fas fa-credit-card me-2"></i>Ler Cartão';
    
    // Limpar timeout
    if (rfidTimeout) {
        clearTimeout(rfidTimeout);
        rfidTimeout = null;
    }
    
    // Enviar comando para ESP32
    socket.emit('stopRFIDReading', { action: 'stop' });
    
    hideRFIDStatus();
    console.log('Leitura RFID cancelada');
}

function handleRFIDRead(data) {
    if (!isReadingRFID) return;
    
    console.log('RFID recebido:', data);
    
    // Verificar se o cartão já está cadastrado
    checkRFIDExists(data.rfidTag).then(exists => {
        if (exists) {
            showRFIDError(`Cartão ${data.rfidTag} já está cadastrado para outro aluno`);
        } else {
            showRFIDSuccess(data.rfidTag);
            document.getElementById('rfidTagInput').value = data.rfidTag;
        }
        cancelRFIDReading();
    });
}

async function checkRFIDExists(rfidTag) {
    try {
        const response = await fetch(`/api/alunos/rfid/${rfidTag}`);
        return response.ok;
    } catch (error) {
        console.error('Erro ao verificar RFID:', error);
        return false;
    }
}

function showRFIDSuccess(rfidTag) {
    const status = document.getElementById('rfidStatus');
    status.className = 'rfid-status rfid-success';
    status.style.display = 'block';
    status.innerHTML = `<i class="fas fa-check me-2"></i>Cartão lido com sucesso: <strong>${rfidTag}</strong>`;
    
    setTimeout(() => {
        hideRFIDStatus();
    }, 5000);
}

function showRFIDError(message) {
    const status = document.getElementById('rfidStatus');
    status.className = 'rfid-status rfid-error';
    status.style.display = 'block';
    status.innerHTML = `<i class="fas fa-exclamation-triangle me-2"></i>${message}`;
    
    setTimeout(() => {
        hideRFIDStatus();
    }, 5000);
}

function hideRFIDStatus() {
    const status = document.getElementById('rfidStatus');
    status.style.display = 'none';
}

// === REGISTROS ===
async function loadRegistros() {
    try {
        const response = await fetch('/api/registros');
        const registros = await response.json();
        
        const tbody = document.getElementById('listaRegistros');
        tbody.innerHTML = '';
        
        if (registros.length === 0) {
            tbody.innerHTML = '<tr><td colspan="6" class="text-center">Nenhum registro encontrado</td></tr>';
            return;
        }
        
        registros.slice(0, 50).forEach(registro => { // Mostrar apenas os 50 mais recentes
            const row = document.createElement('tr');
            const dataHora = new Date(registro.dataHora).toLocaleString('pt-BR');
            const tipoClass = registro.tipo === 'entrada' ? 'success' : 'warning';
            const statusClass = registro.status === 'autorizado' ? 'success' : 'danger';
            
            row.innerHTML = `
                <td>${dataHora}</td>
                <td>${registro.aluno ? registro.aluno.nome : 'Desconhecido'}</td>
                <td>${registro.van ? registro.van.placa : 'N/A'}</td>
                <td><span class="badge bg-${tipoClass}">${registro.tipo}</span></td>
                <td><span class="badge bg-${statusClass}">${registro.status}</span></td>
                <td><code>${registro.rfidTag}</code></td>
            `;
            tbody.appendChild(row);
        });
    } catch (error) {
        console.error('Erro ao carregar registros:', error);
        showNotification('Erro ao carregar registros', 'error');
    }
}

// === FUNÇÕES DE EDIÇÃO E EXCLUSÃO ===
function editMotorista(id) {
    showNotification('Funcionalidade de edição em desenvolvimento', 'info');
}

function deleteMotorista(id) {
    if (confirm('Tem certeza que deseja excluir este motorista?')) {
        fetch(`/api/motoristas/${id}`, { method: 'DELETE' })
            .then(response => {
                if (response.ok) {
                    showNotification('Motorista excluído com sucesso', 'success');
                    loadMotoristas();
                } else {
                    showNotification('Erro ao excluir motorista', 'error');
                }
            })
            .catch(error => {
                console.error('Erro:', error);
                showNotification('Erro ao excluir motorista', 'error');
            });
    }
}

function editVan(id) {
    showNotification('Funcionalidade de edição em desenvolvimento', 'info');
}

function deleteVan(id) {
    if (confirm('Tem certeza que deseja excluir esta van?')) {
        fetch(`/api/vans/${id}`, { method: 'DELETE' })
            .then(response => {
                if (response.ok) {
                    showNotification('Van excluída com sucesso', 'success');
                    loadVans();
                    loadVansSelect();
                } else {
                    showNotification('Erro ao excluir van', 'error');
                }
            })
            .catch(error => {
                console.error('Erro:', error);
                showNotification('Erro ao excluir van', 'error');
            });
    }
}

function editAluno(id) {
    showNotification('Funcionalidade de edição em desenvolvimento', 'info');
}

function deleteAluno(id) {
    if (confirm('Tem certeza que deseja excluir este aluno?')) {
        fetch(`/api/alunos/${id}`, { method: 'DELETE' })
            .then(response => {
                if (response.ok) {
                    showNotification('Aluno excluído com sucesso', 'success');
                    loadAlunos();
                } else {
                    showNotification('Erro ao excluir aluno', 'error');
                }
            })
            .catch(error => {
                console.error('Erro:', error);
                showNotification('Erro ao excluir aluno', 'error');
            });
    }
}

// === NOTIFICAÇÕES ===
function showNotification(message, type = 'info') {
    // Remover notificações existentes
    const existingNotifications = document.querySelectorAll('.notification');
    existingNotifications.forEach(n => n.remove());
    
    const notification = document.createElement('div');
    notification.className = `notification alert alert-${type === 'error' ? 'danger' : type} alert-dismissible fade show`;
    notification.style.cssText = `
        position: fixed;
        top: 20px;
        right: 20px;
        z-index: 9999;
        min-width: 300px;
        box-shadow: 0 4px 12px rgba(0,0,0,0.15);
    `;
    
    notification.innerHTML = `
        ${message}
        <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
    `;
    
    document.body.appendChild(notification);
    
    // Auto-remover após 5 segundos
    setTimeout(() => {
        if (notification.parentNode) {
            notification.remove();
        }
    }, 5000);
} 