# Editor OLED 128×64

Editor local em HTML, CSS e JavaScript. Sem instalação, servidor remoto ou bibliotecas em tempo de execução.

## Abrir

Abra `index.html` no navegador. Para uma origem local consistente, prefira executar a partir da raiz do projeto:

```sh
make -C display dev
```

Acesse **http://localhost:8080**. Use sempre o mesmo endereço, navegador e perfil: o salvamento pertence à origem. Abrir por `file://`, mudar de porta ou trocar `localhost` por `127.0.0.1` pode mostrar outra coleção. Alguns navegadores restringem armazenamento em `file://`; nesse caso use o servidor local.

## Editar e salvar

- 128 colunas, 64 linhas: amarelo em y=0–15, azul em y=16–63. Cada LED é ligado ou desligado.
- Lápis (B), borracha (E), preenchimento de região (F), botão direito para apagar, zoom e grade opcional.
- Tamanho do lápis/borracha de 1×1 a 16×16 LEDs, com contorno do pincel no canvas. O preenchimento não usa esse tamanho. Espaço continua alternando um único LED; Enter aplica o pincel selecionado.
- Segure Shift e arraste para desenhar uma linha reta em qualquer direção, com prévia até soltar o mouse. Funciona com lápis e borracha, respeitando a espessura. Soltar Shift permite continuar à mão livre; cada arraste é uma única operação de desfazer.
- Guias de quadrantes em x=64 e y=32, formando quatro áreas de 64×32. Ative ou oculte em “Quadrantes”; as guias aparecem apenas no editor e não alteram PNG, código ou LEDs.
- “Tela cheia” amplia o editor com suas ferramentas e ajusta o zoom para caber. Saia pelo botão ou Esc; o zoom anterior é restaurado. Navegadores sem suporte à tela cheia usam toda a janela.
- Desenho contínuo por mouse, caneta ou toque. No canvas focado: setas movem o cursor, Espaço alterna o LED e Enter aplica a ferramenta.
- **Posicionar pixels:** escolha Selecionar (S) e arraste uma área, ou use Tela inteira / Faixa amarela / Faixa azul. Os quatro botões de seta deslocam os pixels em 1 px dentro da área. Centralizar pixels centraliza o retângulo que contém o desenho nos dois eixos (em diferenças ímpares, sobra 1 px à direita/embaixo). Para texto preto sobre amarelo, escolha Pixels apagados. Movimentos que cortariam pixels são recusados. No modo Selecionar, as setas do teclado movem o conteúdo; Shift + setas define uma área e Enter confirma; Escape volta à tela inteira. Cada ajuste tem desfazer/refazer e salvamento automático. A borda de seleção não aparece nas exportações.
- Desfazer/refazer por tela, até 60 operações; Ctrl/Cmd+Z e Ctrl/Cmd+Shift+Z. O histórico de desfazer dura a sessão; as telas persistem.
- Criar, renomear, duplicar e excluir telas. Limpar exige confirmação e permite desfazer.
- Salvamento automático em `localStorage`, com status visível e aviso de falha. Limite de 200 telas; a quota real depende do navegador. Exporte JSON para backup.
- Outra aba alterando o salvamento bloqueia gravações nesta aba: exporte as alterações, recarregue e importe as cópias necessárias.
- Dados locais corrompidos são preservados; exportar projeto também tenta baixar o salvamento original. Para recuperar, guarde esses arquivos, limpe o armazenamento deste site no navegador e importe um JSON válido.

Inclui 17 telas do firmware atual: JO, KEN, PO!!, nove resultados, conexão Wi-Fi, espera pelo joinha e três resultados sem jogada. A espera usa o IP ilustrativo `192.168.1.100`. Na atualização para a coleção v6 com resultados em negrito e telas de conexão/espera em peso normal, o editor adiciona as 17 telas como novas cópias com prefixo “Jokenpo ·” uma única vez, preservando a coleção anterior. A limpeza solicitada remove as versões anteriores do Jokenpo uma única vez, mantendo a cópia mais recente de cada tela, suas edições e desenhos próprios com outros nomes. Recarregar novamente não duplica telas; uma tela excluída não reaparece. Novos navegadores já começam com a coleção completa.

## Exportações

**PNG:** 128×64 pixels RGB, sem grade, margem ou cursor. Preto para LEDs desligados, amarelo e azul para ligados. É uma imagem de apresentação; o JSON é a fonte editável.

**JSON:** uma tela ou todo o projeto. A importação adiciona cópias com novos IDs e preserva as telas existentes. IDs, dimensões e LEDs são validados antes de importar. Limite de arquivo: 20 MB.

**C++ (.h):** bitmap de 1.024 bytes em PROGMEM, linhas consecutivas, bit mais significativo primeiro, para `Adafruit_GFX::drawBitmap`. Não é o buffer vertical interno do SSD1306. Inclua o header depois de declarar `Adafruit_SSD1306 display(...)`; depois de inicializar o display, chame a função `show_oled_...()` presente no arquivo. Os identificadores derivam do ID da tela para permitir vários headers no mesmo sketch. Não imprime a senha nem altera o firmware automaticamente.

## Editar junto com o assistente

1. Exporte a tela ou projeto em JSON e salve em `display/screens/`.
2. Peça a edição indicando esse arquivo e a tela. O assistente pode alterar a matriz e exportar a imagem/código pelo comando abaixo.
3. Importe o JSON atualizado no editor. Ele aparece como nova cópia, preservando a versão anterior.

O assistente não lê automaticamente o `localStorage` do seu navegador. `screens/jokenpo.json` é a coleção inicial editável em arquivo; alterações nela precisam ser importadas no navegador.

### Ferramenta de arquivo, sem dependências

A partir da raiz do projeto:

```sh
# Listar IDs e nomes
python3 display/oled.py display/screens/jokenpo.json

# Extrair PNG e header de uma tela
python3 display/oled.py display/screens/jokenpo.json --screen JO \
  --png /tmp/jo.png --header /tmp/jo.h

# Acender um LED, preservando o original
python3 display/oled.py display/screens/jokenpo.json --screen JO \
  --pixel 20 30 1 --output display/screens/jokenpo-editado.json
```

Cada tela contém `id`, `name` e `pixels`: exatamente 8.192 números 0/1, na ordem `pixels[y * 128 + x]`. O envelope tem `format: "oled-studio"`, `version: 1`, `width: 128`, `height: 64`, `activeId` e `screens`.

`build_seeds.py` regenera `seeds.js` e `screens/jokenpo.json` a partir de `../jokenpo/hands.h` e da fonte instalada em `~/Arduino/libraries/Adafruit_GFX_Library/glcdfont.c`. Execute somente quando quiser atualizar os exemplos; ele sobrescreve esses dois arquivos, nunca o navegador.

## Validação

```sh
make -C display test
# Testes de interação em DOM simulado; requer jsdom instalado ou JSDOM_PATH
JSDOM_PATH=/caminho/node_modules/jsdom node display/tests/dom.cjs
# Teste visual em navegador real; requer Playwright e seu Chromium
PLAYWRIGHT_PATH=/caminho/node_modules/playwright node display/tests/browser.cjs
```

O teste de DOM simula o canvas; não substitui inspeção visual. Nesta implementação, testes de lógica/DOM e compilação ESP32 de um header exportado passaram. O Chromium foi bloqueado pelo sandbox (`setsockopt: Operation not permitted`), portanto a validação visual e os downloads via navegador real permanecem pendentes.
