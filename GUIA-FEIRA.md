# Operação da feira

O plano está em [PLANO-FEIRA.md](PLANO-FEIRA.md). O painel acompanha os dois jogos em português; as partidas continuam nas placas se o PC sair. O primeiro preparo precisa de internet. Depois, câmera, reconhecimento e USB funcionam offline.

## Preparar o computador Windows

Na raiz desta pasta, execute no PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\iniciar-feira.ps1
```

O comando prepara Python 3.12, ambiente isolado, dependências fixadas e modelo oficial MediaPipe. Reexecutar reaproveita a instalação. Os arquivos ficam em `%LOCALAPPDATA%\CtrlFeira`: `config.json`, `results.sqlite3`, `demo.sqlite3`, `panel.log`, modelo e ambiente Python. A pasta do repositório pode ser movida. Nenhuma imagem da câmera é gravada.

Para revisar sem placas e sem câmera, ative a demonstração explicitamente:

```powershell
powershell -ExecutionPolicy Bypass -File .\iniciar-feira.ps1 -Demo -Windowed
```

Os dados de demonstração ficam em outro banco. A sessão real mais recente continua ao reabrir. **F2** abre a organização: **C** alterna câmera 0–7, **J/T** alternam portas de cada jogo (ou automático), **N** inicia uma sessão nova preservando as anteriores. **ESC** fecha. Os bancos SQLite podem ser copiados com o painel fechado para arquivamento/consulta de sessões anteriores.

O modo público usa tela cheia horizontal a partir de 1366×768. Os visitantes jogam por gestos e pelos botões físicos. Para Jokenpo, mantenha a mão inteira na câmera por aproximadamente 0,3 s; depois da confirmação, pode retirá-la. A escolha fica fixa durante JO/KEN/PO.

## USB e câmera

Use cabos USB **com dados**. Feche o monitor serial/Arduino antes de abrir o painel. As portas são identificadas pelo protocolo a 115200 baud, não pelo número COM. A organização permite forçar uma porta, mas o identificador do projeto continua sendo validado. Uma placa por projeto é exibida; com duas placas do mesmo jogo, escolha a porta manualmente.

Se a câmera falhar, confira **Configurações → Privacidade e segurança → Câmera → Permitir que aplicativos da área de trabalho acessem a câmera**, feche videoconferências e selecione outra câmera em F2. A captura usa DirectShow no Windows e tenta reabrir após falha. Reinicie o painel após corrigir uma instalação de modelo/dependências.

O iniciador consulta dispositivos USB com erro de driver e imprime a indicação conforme VID. Confira o chip/ID no Gerenciador de Dispositivos e instale somente o correspondente:

| Chip / VID | Fabricante oficial |
| --- | --- |
| CP210x / `10C4` | [Silicon Labs VCP](https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers) |
| CH340/CH341 / `1A86` | [WCH CH341SER](https://www.wch-ic.com/downloads/CH341SER_EXE.html) |
| FTDI / `0403` | [FTDI VCP](https://ftdichip.com/drivers/vcp-drivers/) |

Depois de instalar o driver, reconecte o cabo. Uma falha de placa/câmera não encerra o outro jogo. Uma placa é marcada indisponível após três segundos sem estado.

## Preparar firmwares em outro computador

Esta etapa é separada do iniciador do painel. Não é necessário gravar placas no computador da feira. Instale Arduino CLI, core ESP32 3.3.12, core AVR 1.8.8 e bibliotecas: ArduinoJson 7.4.2, Adafruit SSD1306 2.5.17, Adafruit GFX 1.12.6, Adafruit BusIO 1.17.4, Adafruit NeoPixel 1.15.5, FastLED 3.10.8.

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 jokenpo
arduino-cli compile --fqbn esp32:esp32:esp32 tic-tac-toe
arduino-cli compile --fqbn arduino:avr:uno push-button
arduino-cli compile --fqbn esp32:esp32:esp32 smart-house-test
```

Grave cada programa na placa correspondente, no computador de preparação. O Jokenpo USB compila sem `wifi_config.h`; o arquivo opcional continua habilitando HTTP. A casa requer seu `wifi_config.h` existente para autenticação/Wi-Fi/OTA. Não compartilhe esse arquivo nem binários com credenciais. Os dois arquivos `fair_serial.h` são cópias idênticas porque o empacotamento de sketches Arduino não inclui pastas irmãs.

## Treinamento e reset

No jogo da velha, segure os dois botões por um segundo para treinar (ou use a página web). Os 200.000 episódios são simulações e não entram nos resultados dos visitantes. Uma partida incompleta é descartada durante o treino. Ao concluir, começa uma substituta com o mesmo jogador inicial; a próxima mantém a alternância. O treinamento vale por **05:00** a partir da conclusão, mesmo sem o PC. Se expirar durante uma partida treinada, ela termina com a estratégia treinada e aparece como **Última partida treinada**; a seguinte usa aleatório. Novo treino renova o prazo após concluir. Reiniciar a placa descarta aprendizado e resultados ainda não confirmados pelo PC.

No Push Button, durante a disputa, segure os dois botões continuamente por **três segundos**. Os placares zeram e começa 3, 2, 1, GO. Soltar qualquer um antes cancela. Solte ambos para permitir outro reset. O reinício normal após resultado continua disponível.

## Casa e prévia local

O controle fica em `smart-house-test`, com autenticação e OTA preservadas. Modos: `manual`, `hsl_cycle`, `snow`, `halloween`, `christmas`, `matrix`. Os identificadores antigos `breathing`, `color_wave`, `chase`, `blink` retornam HTTP 400. As ligações e os comandos atuais estão em [WEB.md](smart-house-test/WEB.md).

Na mesma rede da placa, abra o IP informado por ela ou `http://smart-house.local/` e entre com o usuário e senha OTA de `wifi_config.h`. Ao reiniciar, a casa volta ao ciclo de cores, com todos os andares ligados e brilho de 100% na interface, limitado a 191/255 no hardware. Os ajustes não são persistidos.

### Preparar o som da casa

O firmware atual inclui voz com MAX98357A e alto-falante de 4 Ω / 3 W. Com a alimentação desligada, ligue BCLK → GPIO26, LRC/WS → GPIO25, DIN → GPIO27, VIN → fonte regulada de 5 V e GND → GND comum da fonte e ESP32. Os dois terminais do falante vão às saídas + e − do amplificador; nenhum vai ao GND. Veja [AUDIO.md](smart-house-test/AUDIO.md) para os detalhes de montagem e gravação do firmware.

Antes da feira, abra **Gravações** (`/audio`) na interface autenticada e confira os 15 arquivos disponíveis, incluindo `inicio.wav` e `matrix.wav`. Envie os ausentes em WAV PCM mono, 16 bits, 16 kHz, conforme a tabela de [frases e nomes](smart-house-test/AUDIO.md). Se a memória ainda não estiver preparada, siga a confirmação da página: a preparação apaga a partição de dados de áudio.

Comece com volume de 25%. Reinicie para conferir a fala inicial e altere energia, andares e modos para conferir os anúncios. Teste silêncio e ações rápidas. A reprodução é local e não precisa de internet; arquivos ausentes deixam a ação sem fala, mas não impedem o controle das luzes. O volume e o silêncio voltam ao padrão após reiniciar.

### Prévia sem maquete

Para revisar sem maquete, com Python e Node.js instalados:

```powershell
python .\smart-house-test\preview.py
```

Abra `http://127.0.0.1:8765`. A prévia é local, não tem OTA e usa a mesma página e o mesmo motor C++ `effects.h`, pré-compilado em `effects.wasm`. O áudio é simulado em RAM: não emite som nem persiste os arquivos enviados. O brilho é aplicado pela apresentação na prévia e pelo FastLED na fita; cores não equivalem à calibração física dos LEDs. Após alterar o motor, recompile com `smart-house-test/build-preview.ps1 -Clang caminho\clang++.exe`. Nenhum efeito exige internet.

## Conferência física antes de abrir

1. Confira alimentação, GND comum e ligações conforme os READMEs de cada montagem. Confira cabos USB com dados e drivers.
2. Abra o painel real e confirme as duas identificações; desconecte/reconecte cada cabo separadamente. O restante deve continuar funcionando.
3. Confira webcam selecionada, imagem espelhada, pontos verdes e pedra/papel/tesoura. Verifique contagem, OLED, buzzer e resultado. Reabra o painel e confira que o total continua.
4. Jogue da velha pelos botões: seleção, confirmação, início alternado e reinício automático. Treine durante uma partida e confira substituição, 200.000 episódios, timer e próxima partida aleatória após expiração. Simulações não devem aumentar KPIs.
5. No Push Button, teste debounce/pontos, pressão de 2,9 s (cancela ao soltar), 3 s (zera), manutenção dos botões (não repete) e soltura/rearme; confira resultado e reinício normal.
6. Na casa, confira cada andar, cor, brilho, Ciclo de cores, Neve, Halloween, Natal e Matrix; desligue um andar durante cada efeito. Confira as 15 gravações, fala inicial, anúncios de energia/andar/modo, volume, silêncio e comandos rápidos durante a reprodução. Confira login, conexão e página OTA sem enviar atualização durante a feira.
7. Inicie uma sessão real nova em F2/N quando a conferência terminar.

## Verificações automatizadas

```powershell
python -m unittest discover -s jokenpo -p 'test_*.py'
python -m unittest discover -s fair -p 'test_*.py'
node tic-tac-toe/tests/polling.cjs
node smart-house-test/check-effects.cjs
# Com preview.py em execução:
node smart-house-test/check-preview.cjs
```

Os testes de serial usam placas simuladas: não substituem o ensaio com os cabos reais. O teste de reconhecimento sem câmera verifica carregamento/inferência do modelo; o reconhecimento dos gestos e a estabilidade na iluminação da feira exigem a conferência física.
