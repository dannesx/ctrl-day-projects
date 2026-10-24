# Validação da implementação — 24/10/2026

## Verificado neste computador

- 20 testes existentes de Jokenpo: gesto, geometria dos dedos, estabilidade e trabalhadores em segundo plano.
- 4 testes novos: sessões SQLite, reabertura, duplicatas/reboot/dispositivos distintos, descarte de mensagens inválidas e descoberta/reconexão serial simulada com portas invertidas, mensagens fragmentadas e ACK.
- Regressão existente do polling web do jogo da velha: passou.
- Teste C++ existente de aprendizado, compilado com Emscripten e executado no Node: três sementes, 200.000 episódios por treino e 10.000 partidas de avaliação por semente. Resultados: 8.996/9.040/9.031 vitórias do ESP, zero derrotas e 1.004/960/969 empates contra o adversário aleatório do teste. Isso não mede desempenho contra visitantes.
- Código de timer/reset usado nos firmwares, compilado para WebAssembly: validade inicial, expiração em 300.000 ms, renovação, rollover de `millis`, reset em 3.000 ms, cancelamento antes do prazo e trava até soltura.
- Motor de efeitos compartilhado C++/Wasm: desligados nos cinco modos, flashes/apagões Halloween em todos os andares, cores e cintilação do Natal.
- Prévia HTTP local e Edge: cinco modos × três resoluções (1366×768, 1920×1080, 390×844), sem rolagem nem controles fora da tela; comandos de andar/cor e rejeição dos modos removidos. Capturas em `.build/casa-*.png`.
- Painel Pygame em modo headless: demonstração persistente e estado vazio com placas/modelo indisponíveis. Captura sintética de demonstração em `.build/panel-demo.png`. O painel só permite essa captura de teste em demonstração; não grava câmera.
- MediaPipe 0.10.21: modelo oficial carregado e inferência executada em imagem sintética preta, retornando zero mãos.
- Dependências instaladas no ambiente isolado, `pip check` sem conflitos; sintaxe Python/PowerShell/Node e `git diff --check` sem erros.

## Compilações finais

Arduino CLI com core ESP32 3.3.12 e AVR 1.8.8. Sem gravação ou envio OTA.

| Firmware | Alvo | Flash usada | RAM global |
| --- | --- | ---: | ---: |
| Jokenpo | ESP32 | 1.018.375 bytes (77%) | 51.324 bytes (15%) |
| Jogo da velha | ESP32 | 1.023.860 bytes (78%) | 88.448 bytes (26%) |
| Push Button | Uno | 4.314 bytes (13%) | 102 bytes (4%) |
| Casa | ESP32 | 1.048.391 bytes (79%) | 52.008 bytes (15%) |

Artefatos ficam em `.build`, ignorada pelo Git. A casa foi compilada com a configuração Wi-Fi local existente; não distribua o binário com credenciais. Os cabeçalhos do protocolo dos dois jogos foram comparados e são idênticos.

## Conferência posterior necessária

Sem ensaio físico nesta implementação: câmera real/iluminação, gestos em movimento, drivers/cabos e reconexão USB real, OLED, buzzer, debounce dos botões e LEDs. Expiração durante partida e alternância após treino foram implementadas e revisadas; a sequência completa na placa deve seguir [o guia](GUIA-FEIRA.md).

O iniciador teve sintaxe e seus componentes verificados (ambiente, dependências, modelo e abertura do painel); a instalação automática de Python em um Windows limpo, por winget ou instalador oficial, não foi executada neste computador já equipado. A autenticação e OTA reais foram preservadas no código e compiladas, mas não houve chamada à maquete nesta execução. O script `check-web.cjs` permite essa validação explícita posterior.
