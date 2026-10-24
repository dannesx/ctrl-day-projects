# Controle da casa

Abra o IP informado pela placa ou `http://smart-house.local/` na mesma rede. Use o usuário e senha OTA de `wifi_config.h`. A página continua autenticada; a atualização continua em `/update`. Veja [o guia da feira](../GUIA-FEIRA.md).

Três cartões controlam energia e cor por andar. Energia geral, brilho de 0 a 100% e seis cartões de atmosfera ficam na mesma tela. Os controles foram dimensionados para 1366×768, 1920×1080 e celular 390×844, inclusive com os dois ajustes de Neve abertos. A conexão e erros ocupam uma área compacta.

| Modo | Identificador | Comportamento |
| --- | --- | --- |
| Cores fixas | `manual` | Cores individuais; escolher cor liga o andar |
| Ciclo de cores | `hsl_cycle` | Ciclo HSL sincronizado de 8 segundos |
| Neve | `snow` | Flocos brancos descem pelos andares sobre azul escuro |
| Halloween | `halloween` | Sombras roxas, velas irregulares, flashes/apagões sorteados |
| Matrix | `matrix` | Rastros verdes horizontais, pontas claras e ritmos independentes nos três andares |
| Natal | `christmas` | Vermelho/verde, transições entre andares e pontos dourados/brancos |

Todos respeitam andares desligados e brilho geral. Halloween sorteia andar, intervalo, quantidade e duração dos flashes, sem depender da fase das velas. O renderizador atualiza sem `delay`. Os modos `breathing`, `color_wave`, `chase`, `blink` foram removidos da API e retornam 400.

Ao reiniciar: ciclo de cores, todos ligados, brilho 100% na interface, limitado a 191/255 no FastLED (aproximadamente 75%). O mapeamento é `round(percentual × 191 / 100)`; 50% corresponde a 96/255. Ajustes não são persistidos em flash. O mapeamento permanece: SUPERIOR GPIO14 / 10 LEDs, CENTRAL GPIO12 / 10 LEDs, TERREO GPIO13 / 10 LEDs.

O painel de voz controla volume (inicial 25%) e silêncio. O link **Gravações** permite preparar a memória e enviar os WAVs. Veja [ligações, frases e formato de áudio](AUDIO.md). Energia e modos anunciam somente mudanças efetivas; cor, brilho e neve são silenciosos.

## API

Todas as rotas reais exigem autenticação HTTP. `GET /status` e `POST /control` com formulário URL-encoded:

| action | Campos |
| --- | --- |
| all | `value=on` ou `off` |
| brightness | `value=0..100` |
| mode | `value=manual`, `hsl_cycle`, `snow`, `halloween`, `christmas`, `matrix` |
| floor | `id=SUPERIOR/CENTRAL/TERREO` e um de `on=true/false` ou `color=#RRGGBB` |
| snow | `speed=1..5` e/ou `density=1..8` |
| audio | `volume=0..100` e/ou `muted=true/false` |

Valores inválidos: 400. Origem externa: 403. Sem credenciais: 401. O estado inclui cor configurada, `rendered_color` e, nos efeitos Neve/Halloween/Natal/Matrix, `pixels` por andar. Valores RGB reportados são anteriores ao brilho global do FastLED. `brightness` continua representando a saída física, agora limitada a 191; `brightness_percent` mantém o valor de 0 a 100 da interface. `audio` informa controles, estado do player e disponibilidade dos arquivos. A página consulta a cada 200 ms, sem acumular consultas; comandos são sequenciais.

## Prévia e validação

```powershell
python .\smart-house-test\preview.py
# Em outro terminal:
node .\smart-house-test\check-preview.cjs
node .\smart-house-test\check-effects.cjs
```

Acesse `http://127.0.0.1:8765`. A prévia usa a mesma página e o mesmo motor `effects.h`, compilado para WebAssembly em `effects.wasm`; Node.js executa esse motor. Não há maquete, autenticação ou OTA na prévia, que escuta somente em localhost. Para regenerar após alterar o C++:

```powershell
.\smart-house-test\build-preview.ps1 -Clang caminho\clang++.exe
```

O compilador precisa incluir `wasm-ld`. O código de adaptação está em `preview/engine.cpp`; essa pasta não é compilada no sketch Arduino. O teste de navegador gera capturas em `.build/casa-*.png`. Para testar autenticação e comandos na placa real já preparada, execute explicitamente `node smart-house-test/check-web.cjs IP_DA_PLACA`; ele lê as credenciais locais e restaura as configurações ao final.

### Matrix horizontal

Configuração atual: três linhas de 10 LEDs, superior no GPIO14, central no GPIO12 e térreo no GPIO13. O índice 0 de cada fita corresponde à esquerda; os índices crescem para a direita. O efeito percorre cada andar separadamente, com ponta verde-clara, cauda verde que perde brilho e um intervalo preto entre passagens. Selecione **Matrix** no painel ou envie `action=mode&value=matrix` para `/control`. O modo respeita o brilho e o estado ligado/desligado de cada andar; usa a gravação `matrix.wav` para anunciar a ativação.
