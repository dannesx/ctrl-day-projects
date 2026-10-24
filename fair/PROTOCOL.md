# Protocolo USB da feira, versão 1

115200 baud, JSON UTF-8 por linha (`\n`). Linhas de diagnóstico não JSON são ignoradas. A placa publica `hello` ao iniciar e responde a `{"v":1,"type":"hello"}` sem exigir Wi-Fi. Publica estado a cada 500 ms; resultados podem chegar fora da ordem de partidas.

Todo evento de placa tem `v:1`, `project` (`jokenpo` ou `tic-tac-toe`), `device` (eFuse MAC), `boot` (nonce de 64 bits por inicialização), `game` (contador por boot) e `type` (`hello`, `state`, `result`). `hello` é identificação, não substitui o estado ativo.

Jokenpo: `state` (`standby`, `countdown`, `result`), `elapsed_ms`, `player`, `computer` (-1 antes da revelação), `result` (texto). Opções: 0 pedra, 1 papel, 2 tesoura.

Velha: `board` (nove caracteres `-XO`), `result` (`-`, `X`, `O`, `D`), `turn`, `mode` (`random`/`trained`, fixo durante a partida), `training`, `episodes`, `remaining_ms`, `last_trained`. Treinamento não gera resultados de partidas simuladas. Uma substituição após treino recebe outro `game`.

Eventos `result` incluem `outcome`: `win`, `draw`, `loss`, sempre do ponto de vista do visitante. O PC grava com chave única `(project,device,boot,game)` em transação SQLite **antes** de confirmar:

```json
{"v":1,"type":"ack","project":"jokenpo","device":"abc","boot":"nonce","game":12}
```

A placa retém resultados em RAM e reenvia um pendente por tick de estado em rodízio até receber ACK do mesmo dispositivo/boot/partida. Retransmissões são confirmadas novamente, sem duplicar contagens ou migrar para uma sessão nova. Fechar o PC/desconectar USB preserva os pendentes enquanto a placa continuar alimentada. Reinicializar a placa perde os pendentes; não há garantia através de corte de energia. Pendentes ocupam RAM: conecte o painel durante a feira para mantê-los confirmados.

Comandos usam o envelope completo da identificação atual (`v`, `project`, `device`, `boot`, `game`). `type:start` e `move:0|1|2` iniciam Jokenpo somente em standby. A jogada é consumida uma vez; inícios ambíguos não são repetidos pelo PC. `type:train` inicia treino no jogo da velha; a interação pública continua nos botões físicos. Comandos dirigidos a outra inicialização são ignorados.
