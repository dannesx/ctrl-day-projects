# Smart House — primeira versão

Controle de iluminação de uma maquete de três andares com ESP32 DevKit V1 e 30 LEDs WS2812B de 5 V. Cada um dos seis cômodos tem cor e estado independentes. Controle pelo Monitor Serial, sem Wi-Fi nesta versão.

## Ligações

| Andar | GPIO → DIN | LEDs na ordem da fita (índices a partir de zero) |
| --- | --- | --- |
| Superior (3º) | 14 (D14) | QUARTO_SUP: 0–7; SACADA: 8–11 |
| Central (2º) | 12 (D12) | QUARTO_MEIO: 0–5; VARANDA: 6–8 |
| Térreo (1º) | 13 (D13) | SALA/cozinha: 0–5; BANHEIRO: 6–8 |

Use três segmentos com dados independentes. Em cada segmento, siga a seta DIN → DOUT; conecte o primeiro DIN ao GPIO correspondente. Alimente os LEDs com fonte externa regulada de 5 V, dimensionada para a fita, e una o GND da fonte, das fitas e do ESP32. Não alimente as fitas pelo pino 3V3. Evite ligar em paralelo a saída de 5 V da fonte e o USB da placa sem verificar o circuito de alimentação da sua DevKit.

Para dados de 3,3 V do ESP32 em LEDs de 5 V, use conversor de nível adequado, como 74AHCT125 alimentado em 5 V, com entradas não utilizadas definidas. Coloque resistor de 330–470 Ω em série em cada DIN e capacitor de 500–1000 µF (tensão nominal de pelo menos 6,3 V, respeitando polaridade) entre 5 V e GND perto da alimentação das fitas. Faça as ligações com a alimentação desligada. O limite de brilho no programa não substitui o dimensionamento da fonte e dos fios.

## Gravar e usar

1. Na Arduino IDE, instale o suporte de placas **esp32 by Espressif Systems** e a biblioteca **FastLED** pelo Gerenciador de Bibliotecas.
2. Abra `smart-house.ino`, selecione **ESP32 Dev Module** e a porta da placa e envie o programa.
3. Abra o Monitor Serial em **115200 baud**, com final de linha **Nova linha** ou **Ambos NL e CR**.

Ao iniciar, a demonstração acende os cômodos em sequência e deixa todos ligados. O brilho começa em 80 na escala de 0–255. O banheiro começa branco; os demais cômodos, em branco quente RGB (255, 160, 75).

```text
AJUDA
LISTA
LIGAR SALA
DESLIGAR BANHEIRO
COR QUARTO_SUP 255 0 0
BRILHO 60
TUDO ON
TUDO OFF
DEMO
```

`LISTA` mostra estado, cor e quantidade de LEDs por cômodo. `COR` também liga o cômodo. `BRILHO` aceita inteiros de 0 a 80 e muda todos os andares; brilho zero preserva os estados ligados/desligados. Comandos aceitam letras minúsculas e espaços extras. Valores inválidos, argumentos extras e linhas acima de 100 caracteres são rejeitados sem alterar as luzes.

A demonstração leva aproximadamente 4,3 segundos e bloqueia o processamento de comandos nesse período. Estados e cores ficam na memória e voltam aos valores iniciais ao reiniciar.

### Arduino CLI

Com core ESP32 e FastLED instalados, a partir da raiz do repositório:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 smart-house
arduino-cli upload --port COM3 --fqbn esp32:esp32:esp32 smart-house
arduino-cli monitor --port COM3 --config baudrate=115200
```

Troque `COM3` pela porta da placa. Em Linux/macOS com `make`, também é possível usar `make -C smart-house compile`, `upload PORT=/dev/ttyUSB0` e `monitor PORT=/dev/ttyUSB0`.

### Conferência na maquete

Execute `TUDO OFF`, ligue cada cômodo individualmente e confira os limites entre os segmentos. Teste `COR SALA 255 0 0`, `COR SALA 0 255 0` e `COR SALA 0 0 255` para conferir a ordem GRB. Teste `BRILHO 0`, `BRILHO 80` e `LISTA`. `BRILHO abc`, `COR SALA 256 0 0` e `LIGAR SALA EXTRA` devem exibir erro sem mudar o estado.
