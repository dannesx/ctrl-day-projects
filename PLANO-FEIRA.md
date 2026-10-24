# Preparação dos projetos para a feira

## Resumo
Criar um painel Python/Pygame com duas colunas para acompanhar Jokenpo e jogo da velha por USB. Preparar instalador Windows, validade do treinamento, reset do Push Button e redesenho do controle em `smart-house-test`.
Preservar alterações locais do jogo da velha: botões físicos, reinício automático e aprendizado somente na RAM.

## Painel e comunicação USB
- Aplicação única em português; câmera, reconhecimento e serial sem bloquear interface.
- Identificação automática dos ESPs a 115200 baud, independente da ordem COM; seleção manual na configuração.
- JSON por linha versionado com projeto, dispositivo, inicialização, partida e tipo; logs separados.
- Compartilhar lógica entre USB e controles existentes. Jokenpo USB sem Wi-Fi/credenciais, mantendo HTTP quando configurado.
- Estados/resultados identificáveis, confirmação, reenvio durante a mesma inicialização e deduplicação no PC.
- Reconexão automática e indisponibilidade isolada de placas/câmera.

### Jokenpo
- Câmera espelhada com pontos/conexões verdes, gesto, contagem e resultado.
- KPIs: partidas, vitórias, empates, derrotas e taxa de vitória do jogador.
- Histórico categórico das últimas 30 partidas, escolhas do jogador e ESP no mesmo gráfico.
- Frequência acumulada de pedra/papel/tesoura.
- Vitórias e taxa por opção, com tentativas.

### Jogo da velha
- Partida, modo, aprendizado e validade restante mm:ss.
- KPIs: partidas, vitórias, empates e derrotas do jogador.
- Gráfico de vitórias/empates/derrotas agrupado por modo aleatório/treinado, contagens e percentuais.
- Excluir 200.000 simulações das estatísticas dos visitantes.

Persistir resultados/sessões em SQLite e continuar sessão ao reabrir. Organização discreta: câmera, portas e nova sessão preservando anteriores. Não salvar imagens.

## Validade do treino e reset
### Jogo da velha
- 200.000 episódios; validade de cinco minutos após concluir, controlada pela placa.
- Expiração durante partida treinada: “Última partida treinada”; próxima aleatória.
- Treinar interrompe partida sem contabilizar; conclusão abre substituta treinada com mesmo jogador inicial e preserva alternância seguinte.
- Novo treino reinicia validade após concluir; reboot retorna ao aleatório.
- OLED: ALEATORIO, TREINANDO, TREINADO e timer, tabuleiro legível; estados também serial/web.
### Push Button
- Durante partida, dois botões continuamente por três segundos zeram placares e reiniciam 3, 2, 1, GO.
- Soltar qualquer um cancela. Uma vez por pressão conjunta; exigir soltura para novo reset.
- Preservar debounce, pontuação e reinício após resultado.

## Casa: interface e efeitos
- Trabalhar em `smart-house-test`; três controles visuais de andar, energia grande, cores, brilho e cartões de efeitos.
- Uma tela em 100vw/100dvh, sem rolagem em 1366×768, 1920×1080 e 390×844.
- Estados explícitos, contraste, foco e rótulos acessíveis; conexão/erros compactos; preservar autenticação/OTA.
- Cinco modos: cores fixas, ciclo de cores, Neve, Halloween, Natal (`christmas`). Remover respiração, ondas, sequência e pisca-pisca da interface/API.
- Halloween: sombras roxas, velas irregulares, flashes/apagões com andares/intervalos/duração/quantidade sorteados, independentes da fase laranja; sem delay; respeitar desligados/brilho.
- Natal: vermelho/verde, pontos dourados/brancos cintilantes, transições entre andares.
- Prévia local com mesma lógica de efeitos do firmware.

## Instalação e validação
- Comando PowerShell instala Python 3.12 se necessário, venv, dependências fixadas, modelo e abre painel; reexecuções reaproveitam instalação.
- Offline após preparação inicial com internet. Captura Windows, webcam selecionável, diagnóstico de permissões e portas sem driver com indicação oficial.
- Configuração/banco/logs em %LOCALAPPDATA%, pasta móvel. Gravação dos firmwares separada em outro computador.
- Demonstração explicitamente ativada, dados separados dos reais.
- Validar reconhecimento/estabilidade existentes, serial/reconexão/repetições, persistência, gráficos vazios, timer/expiração, reset em três segundos, efeitos e limites visuais. Compilar quando ferramentas disponíveis.
- Guia da feira: sequência física de câmera, cabos/drivers USB, OLED, buzzer, botões/LEDs e comando único.

Padrões: painel público em duas colunas, tela cheia horizontal a partir de 1366×768; jogos por gestos/botões físicos; histórico persistente; offline após instalação.
