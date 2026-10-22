# Jogo da velha — ESP32

O jogador é **X** e o ESP32 é **O**. Você começa a primeira partida após ligar a placa.
A cada **Nova partida**, o início alterna entre você e o ESP, mesmo se a
partida anterior foi interrompida. Reiniciar a placa volta o início para você. O ESP32 sorteia uma casa livre
sem estratégia. A página web e o OLED mostram a mesma partida. Vitória e
empate encerram a rodada; **Nova partida** limpa o tabuleiro.

## Hardware

Mesmo SSD1306 I²C 128×64 do Jokenpo:

| OLED | ESP32 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

Detecta automaticamente `0x3C` ou `0x3D`. Sem OLED, o jogo web continua.
O cabeçalho ocupa os 16 pixels superiores, compatível com o display bicolor.

## Compilar e gravar

Requer Arduino CLI, core `esp32:esp32`, Adafruit SSD1306 e Adafruit GFX
(com suas dependências), como no Jokenpo.

```sh
make -C tic-tac-toe compile
make -C tic-tac-toe upload PORT=/dev/ttyUSB0
make -C tic-tac-toe monitor PORT=/dev/ttyUSB0
```

A gravação substitui o programa atualmente instalado no ESP32.

## Jogar

1. Ligue o ESP32 e conecte o celular/computador à rede **ESP32-TicTacToe**.
2. Senha: **jogodavelha**. Aceite permanecer na rede mesmo sem internet.
3. Abra **http://192.168.4.1** no navegador (HTTP, não HTTPS).
4. Aguarde sua vez e toque em uma casa vazia. O ESP joga após cerca de 450 ms,
   inclusive quando é a vez dele começar.

A página funciona sem internet, aplicativos ou arquivos externos. A rede é
criada pela própria placa; não precisa das credenciais do Jokenpo. Nome e
senha estão nas constantes `AP_SSID` e `AP_PASSWORD` do sketch.

Há uma única partida compartilhada: todos os navegadores conectados controlam
o mesmo jogador. O firmware rejeita casas ocupadas, movimentos fora de turno
e comandos de uma versão antiga do tabuleiro. A página sincroniza a cada
700 ms. Reiniciar a placa apaga a partida; não há placar persistente.

## Aprendizado no ESP32

**TREINAR** executa 200.000 partidas simuladas de autojogo na própria placa.
A web mostra animação e progresso real; o OLED mostra pontos animados e uma
barra de progresso. Jogadas e reinícios ficam bloqueados durante o treino.
Ao terminar, a partida atual é retomada em **Modo treinado**. Treinar não
altera a alternância de quem inicia. **TREINAR NOVAMENTE** refaz o modelo.

O algoritmo é Q-learning tabular sobre *afterstates*: cada ação é representada
pelo tabuleiro resultante, normalizado para quem acabou de jogar. A tabela tem
19.683 valores int16 (39.366 bytes). O treino explora ações aleatórias com taxa
decrescente de 70% a 15%, recompensa vitória com +1 e empate com 0, desconta
recompensas futuras por 0,98 e atualiza valores com taxa 0,25. O valor do melhor
movimento do adversário entra com sinal negativo. Não há minimax ou regras de
bloqueio/vitória embutidas no jogador treinado: ele escolhe o maior valor
aprendido entre movimentos legais, com desempate determinístico.

O modelo é salvo em `/ttt-model.bin` no LittleFS com versão e checksum, usando
um arquivo temporário antes da substituição. Esta versão inicializa/formata a
partição LittleFS caso ela não possa ser montada. Se salvar falhar, a interface
informa que o aprendizado só vale até desligar. Um modelo válido é carregado
ao ligar; **Nova partida** não apaga o aprendizado. A partida em si não é salva.

O treino ocorre em lotes de aproximadamente 4 ms para continuar atendendo HTTP.
O tempo total depende da placa. Fechar o navegador não interrompe o treino.
Aprendizado não é garantia de jogo perfeito contra todos os adversários.

### Testes locais

```sh
make -C tic-tac-toe test
```

Requer Node.js e compilador C++. Inclui regressão do botão durante polling e
avaliação do mesmo algoritmo C++ usado no firmware contra adversário aleatório.
