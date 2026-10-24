# Push Button

Corrida para dois jogadores com Arduino Uno e duas matrizes WS2812B 8x8 em cadeia.
Instale **Adafruit NeoPixel** pelo gerenciador de bibliotecas da Arduino IDE e abra
`push-button.ino`, selecionando a placa Arduino Uno.

## Ligacoes

| Componente | Ligacao |
| --- | --- |
| Botao amarelo | D2 e GND |
| Botao azul | D3 e GND |
| DIN da matriz esquerda | D4, com resistor de 330 a 470 ohms em serie |
| DIN da matriz direita | DOUT da matriz esquerda |
| Alimentacao das matrizes | Fonte externa regulada de 5 V |
| GND | Comum entre fonte, matrizes e Arduino |

Os botoes usam `INPUT_PULLUP`, sem resistores externos. Em botoes de quatro pernas,
use contatos que se conectam somente ao pressionar (pernas do mesmo par ja sao ligadas).
Nao alimente os 128 LEDs pelo pino 5 V do Uno: dimensione a fonte para suas matrizes
(128 LEDs RGB podem consumir cerca de 7,7 A no branco maximo). O sketch inicia com
brilho limitado a 24/255. Use um capacitor de 500 a 1000 uF entre 5 V e GND na entrada
das matrizes, respeitando a polaridade.

## Jogo

- Ao ligar: 3, 2, 1 (um segundo cada), GO! (meio segundo).
- Depois que GO! apaga, cada aperto amarelo acende um LED na esquerda; cada aperto
  azul acende um na direita. Solte o botao para contar o proximo aperto.
- Vence quem preencher primeiro sua matriz de 64 LEDs. A partida para e o lado
  vencedor pisca tres vezes e permanece aceso, mantendo o placar do adversario.
  O reinicio fica disponivel depois das tres piscadas. Se ambos completarem na mesma
  leitura, e empate.
- Solte os dois botoes depois do resultado e pressione ambos para nova contagem.

O mapeamento padrao considera cada matriz por colunas no mesmo sentido, com o primeiro
LED em A8 (canto inferior esquerdo) e as duas placas na mesma orientacao. O amarelo
preenche A1 ate A8, depois B1 ate B8 e assim por diante; o azul preenche H1 ate H8,
depois G1 ate G8, seguindo em direcao ao centro. Ajuste
`SERPENTINE`, `LEFT_MATRIX_FIRST` ou `pixelAt()` se sua montagem for diferente.
O protocolo configurado e WS2812B (GRB, 800 kHz).

## Reset durante a disputa

Segure os dois botões continuamente por três segundos durante o jogo para zerar
os placares e reiniciar 3, 2, 1, GO. Soltar qualquer botão antes cancela. Solte
ambos para rearmar; manter pressionados não repete o reset.
