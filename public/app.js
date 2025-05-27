class VansControlApp {
    constructor() {
        this.socket = null;
        this.token = localStorage.getItem('vansControlToken');
        this.motorista = null;
        this.van = null;
        this.registros = [];
        
        this.init();
    }

    init() {
        console.log('🚐 VansControl App iniciado');
        
        // Verificar se já está logado
        if (this.token) {
            this.verificarToken();
        } else {
            this.mostrarLogin();
        }

        // Event listeners
        this.setupEventListeners();
    }

    setupEventListeners() {
        // Form de login
        document.getElementById('loginForm').addEventListener('submit', (e) => {
            e.preventDefault();
            this.fazerLogin();
        });
    }

    async verificarToken() {
        try {
            const response = await fetch('/api/motoristas', {
                headers: {
                    'Authorization': `Bearer ${this.token}`
                }
            });

            if (response.ok) {
                const motoristas = await response.json();
                // Assumindo que retorna os dados do motorista logado
                this.mostrarPainel();
            } else {
                localStorage.removeItem('vansControlToken');
                this.mostrarLogin();
            }
        } catch (error) {
            console.error('Erro ao verificar token:', error);
            this.mostrarLogin();
        }
    }

    async fazerLogin() {
        const email = document.getElementById('email').value;
        const senha = document.getElementById('senha').value;

        try {
            const response = await fetch('/api/motoristas/login', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ email, senha })
            });

            const data = await response.json();

            if (response.ok) {
                this.token = data.token;
                this.motorista = data.motorista;
                localStorage.setItem('vansControlToken', this.token);
                
                console.log('✅ Login realizado com sucesso');
                this.mostrarPainel();
            } else {
                this.mostrarAlerta('Erro no login: ' + data.error, 'danger');
            }
        } catch (error) {
            console.error('Erro no login:', error);
            this.mostrarAlerta('Erro de conexão', 'danger');
        }
    }

    mostrarLogin() {
        document.getElementById('loginScreen').style.display = 'block';
        document.getElementById('mainPanel').style.display = 'none';
        
        // Desconectar socket se estiver conectado
        if (this.socket) {
            this.socket.disconnect();
            this.socket = null;
        }
    }

    async mostrarPainel() {
        document.getElementById('loginScreen').style.display = 'none';
        document.getElementById('mainPanel').style.display = 'block';

        // Carregar dados do motorista e van
        await this.carregarDadosMotorista();
        
        if (this.van) {
            // Conectar WebSocket
            this.conectarWebSocket();
            
            // Carregar dados iniciais
            await this.carregarEstatisticas();
            await this.carregarRegistros();
            
            // Atualizar dados periodicamente
            setInterval(() => {
                this.carregarEstatisticas();
            }, 30000); // A cada 30 segundos
        }
    }

    async carregarDadosMotorista() {
        try {
            // Se não temos dados do motorista, buscar pela API
            if (!this.motorista) {
                const response = await fetch('/api/motoristas', {
                    headers: {
                        'Authorization': `Bearer ${this.token}`
                    }
                });
                
                if (response.ok) {
                    const motoristas = await response.json();
                    // Aqui assumimos que a API retorna apenas o motorista logado
                    this.motorista = motoristas[0];
                }
            }

            if (this.motorista && this.motorista.van) {
                this.van = this.motorista.van;
                
                // Atualizar interface
                document.getElementById('motoristaName').textContent = this.motorista.nome;
                document.getElementById('vanPlaca').textContent = this.van.placa;
            } else {
                this.mostrarAlerta('Motorista não possui van associada', 'warning');
            }
        } catch (error) {
            console.error('Erro ao carregar dados do motorista:', error);
        }
    }

    conectarWebSocket() {
        this.socket = io();
        
        this.socket.on('connect', () => {
            console.log('🔌 WebSocket conectado');
            this.atualizarStatus('online');
            
            // Entrar na sala da van
            if (this.van) {
                this.socket.emit('joinVan', this.van._id);
            }
        });

        this.socket.on('disconnect', () => {
            console.log('🔌 WebSocket desconectado');
            this.atualizarStatus('offline');
        });

        this.socket.on('joinedVan', (data) => {
            console.log('✅ Conectado à van:', data.vanId);
        });

        // Escutar novos registros em tempo real
        this.socket.on('novoRegistro', (data) => {
            console.log('📝 Novo registro:', data);
            this.adicionarNovoRegistro(data.registro);
            this.atualizarEstatisticas();
        });

        this.socket.on('connect_error', (error) => {
            console.error('Erro na conexão WebSocket:', error);
            this.atualizarStatus('offline');
        });
    }

    atualizarStatus(status) {
        const indicator = document.getElementById('statusIndicator');
        const text = document.getElementById('statusText');
        
        if (status === 'online') {
            indicator.className = 'status-indicator status-online';
            text.textContent = 'Online';
        } else {
            indicator.className = 'status-indicator status-offline';
            text.textContent = 'Offline';
        }
    }

    async carregarEstatisticas() {
        if (!this.van) return;

        try {
            const response = await fetch(`/api/registros/estatisticas/${this.van._id}`, {
                headers: {
                    'Authorization': `Bearer ${this.token}`
                }
            });

            if (response.ok) {
                const stats = await response.json();
                this.atualizarEstatisticas(stats);
            }
        } catch (error) {
            console.error('Erro ao carregar estatísticas:', error);
        }
    }

    atualizarEstatisticas(stats = null) {
        if (stats) {
            document.getElementById('totalEntradas').textContent = stats.hoje.entradas;
            document.getElementById('totalSaidas').textContent = stats.hoje.saidas;
            document.getElementById('presentesNaVan').textContent = stats.hoje.presentesNaVan;
            document.getElementById('totalAlunos').textContent = stats.totalAlunos;
        } else {
            // Recalcular baseado nos registros carregados
            this.carregarEstatisticas();
        }
    }

    async carregarRegistros() {
        if (!this.van) return;

        try {
            const response = await fetch(`/api/registros/van/${this.van._id}/hoje`, {
                headers: {
                    'Authorization': `Bearer ${this.token}`
                }
            });

            if (response.ok) {
                this.registros = await response.json();
                this.renderizarRegistros();
            }
        } catch (error) {
            console.error('Erro ao carregar registros:', error);
        }
    }

    renderizarRegistros() {
        const container = document.getElementById('registrosList');
        
        if (this.registros.length === 0) {
            container.innerHTML = '<p class="text-muted text-center">Nenhum registro hoje</p>';
            return;
        }

        const html = this.registros.map(registro => {
            const data = new Date(registro.dataHora);
            const hora = data.toLocaleTimeString('pt-BR', { 
                hour: '2-digit', 
                minute: '2-digit' 
            });
            
            const tipoClass = registro.tipo === 'saida' ? 'saida' : '';
            const statusClass = registro.status !== 'autorizado' ? 'negado' : '';
            const icon = registro.tipo === 'entrada' ? 'fa-arrow-up text-success' : 'fa-arrow-down text-danger';
            
            return `
                <div class="registro-item ${tipoClass} ${statusClass}">
                    <div class="row align-items-center">
                        <div class="col-md-1">
                            <i class="fas ${icon} fa-lg"></i>
                        </div>
                        <div class="col-md-6">
                            <strong>${registro.aluno?.nome || 'Desconhecido'}</strong>
                            <br>
                            <small class="text-muted">Matrícula: ${registro.aluno?.matricula || 'N/A'}</small>
                        </div>
                        <div class="col-md-3">
                            <span class="badge bg-${registro.tipo === 'entrada' ? 'success' : 'danger'}">
                                ${registro.tipo.toUpperCase()}
                            </span>
                            ${registro.status !== 'autorizado' ? `<span class="badge bg-warning">NEGADO</span>` : ''}
                        </div>
                        <div class="col-md-2 text-end">
                            <small class="text-muted">${hora}</small>
                        </div>
                    </div>
                </div>
            `;
        }).join('');

        container.innerHTML = html;
    }

    adicionarNovoRegistro(registro) {
        // Adicionar no início da lista
        this.registros.unshift(registro);
        
        // Manter apenas os 50 mais recentes
        if (this.registros.length > 50) {
            this.registros = this.registros.slice(0, 50);
        }
        
        this.renderizarRegistros();
        
        // Mostrar notificação
        this.mostrarNotificacao(registro);
    }

    mostrarNotificacao(registro) {
        const nomeAluno = registro.aluno?.nome || 'Aluno desconhecido';
        const tipo = registro.tipo === 'entrada' ? 'ENTROU' : 'SAIU';
        const cor = registro.status === 'autorizado' ? 'success' : 'warning';
        
        this.mostrarAlerta(`${nomeAluno} ${tipo} da van`, cor, 3000);
    }

    mostrarAlerta(mensagem, tipo = 'info', duracao = 5000) {
        // Criar elemento de alerta
        const alertDiv = document.createElement('div');
        alertDiv.className = `alert alert-${tipo} alert-dismissible fade show position-fixed`;
        alertDiv.style.cssText = 'top: 20px; right: 20px; z-index: 9999; min-width: 300px;';
        
        alertDiv.innerHTML = `
            ${mensagem}
            <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
        `;
        
        document.body.appendChild(alertDiv);
        
        // Remover automaticamente
        setTimeout(() => {
            if (alertDiv.parentNode) {
                alertDiv.remove();
            }
        }, duracao);
    }

    async atualizarRegistros() {
        await this.carregarRegistros();
        await this.carregarEstatisticas();
        this.mostrarAlerta('Registros atualizados', 'success', 2000);
    }

    logout() {
        localStorage.removeItem('vansControlToken');
        this.token = null;
        this.motorista = null;
        this.van = null;
        this.registros = [];
        
        if (this.socket) {
            this.socket.disconnect();
            this.socket = null;
        }
        
        this.mostrarLogin();
        this.mostrarAlerta('Logout realizado com sucesso', 'info', 2000);
    }
}

// Funções globais para serem chamadas pelo HTML
function logout() {
    app.logout();
}

function atualizarRegistros() {
    app.atualizarRegistros();
}

// Inicializar aplicação quando DOM estiver pronto
document.addEventListener('DOMContentLoaded', () => {
    window.app = new VansControlApp();
}); 