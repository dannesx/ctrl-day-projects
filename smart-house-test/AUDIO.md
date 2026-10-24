# Voz da Smart House

Firmware `smart-house-voice-12`, ESP32 DevKit V1 e MAX98357A. Os áudios são fornecidos pelo usuário: nenhuma geração TTS ou serviço externo é necessário no ESP32.

## Ligações

Desligue a alimentação para conectar.

| MAX98357A | Destino |
| --- | --- |
| BCLK | GPIO26 / D26 |
| LRC / WS | GPIO25 / D25 |
| DIN | GPIO27 / D27 |
| VIN | Fonte regulada de 5 V |
| GND | GND comum da fonte e ESP32 |
| Saídas + e − | Terminais do falante de 4 Ω / 3 W |

Nenhum terminal do falante vai ao GND: a saída é em ponte. Use a fonte externa dimensionada para LEDs e amplificador. Preserve as ligações dos LEDs em GPIO14/12/13. Em módulos equivalentes ao breakout Adafruit, SD e GAIN podem ficar sem conexão (habilitado, mistura dos canais, ganho padrão); confira a identificação do seu módulo. O firmware envia a mesma voz nos dois canais. Referência: [pinagem do MAX98357A](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/pinouts).

Volume inicial: 25%; comece assim e aumente gradualmente. Volume é amplitude digital, não uma medida de potência do falante. Volume e silêncio voltam ao padrão após reiniciar.

## Gravações

Use a mesma voz em português brasileiro, ritmo natural e sem música. Um arquivo por frase, **WAV PCM mono, 16 bits, 16 kHz**. Limites: 128.000 bytes por arquivo (quase 4 segundos) e 1.200.000 bytes no conjunto (cerca de 37 segundos).

| Arquivo | Frase |
| --- | --- |
| inicio.wav | Casa inteligente pronta. |
| tudo_on.wav | Todas as luzes acesas. |
| tudo_off.wav | Todas as luzes apagadas. |
| superior_on.wav | Luzes do andar superior acesas. |
| superior_off.wav | Luzes do andar superior apagadas. |
| central_on.wav | Luzes do andar central acesas. |
| central_off.wav | Luzes do andar central apagadas. |
| terreo_on.wav | Luzes do térreo acesas. |
| terreo_off.wav | Luzes do térreo apagadas. |
| manual.wav | Modo cores fixas ativado. |
| ciclo.wav | Ciclo de cores ativado. |
| neve.wav | Modo neve ativado. |
| halloween.wav | Modo Halloween ativado. |
| natal.wav | Modo Natal ativado. |
| matrix.wav | Modo Matrix ativado. |

Se seu TTS exportar outro formato, instale FFmpeg e use nomes com o mesmo radical, por exemplo `inicio.mp3`. O conversor preserva os originais e rejeita gravações longas demais, sem cortá-las:

```powershell
python smart-house-test/prepare_audio.py C:/audios-originais C:/audios-esp
# Se necessário, acrescente --ffmpeg C:/caminho/ffmpeg.exe
```

## Gravar firmware e enviar áudios

Compile com Arduino ESP32 3.3.12 e FastLED; I2S e LittleFS já fazem parte do core. Mantenha o esquema de partições padrão, com dois slots OTA de `0x140000` e partição de dados `0x160000`.

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 --build-path .build/smart-house-audio smart-house-test
.\smart-house-test\update.ps1 -Address IP_DA_PLACA -Firmware .build/smart-house-audio/smart-house-test.ino.bin
```

Depois abra a interface autenticada → **Gravações** (`/audio`). Se necessário, clique **Preparar memória de áudio**: a confirmação informa que apagará a partição de dados. Não há formatação automática. Essa operação não apaga o firmware nem as credenciais compiladas nele.

Selecione os WAVs e envie. Cada arquivo é enviado separadamente; o anterior só é substituído após validação completa. A página lista ausentes e disponíveis. Falta de áudio não impede o controle das luzes. Reinicie para ouvir `inicio.wav`; para as outras falas, altere o estado correspondente. Os WAVs ficam em LittleFS e sobrevivem à atualização normal do firmware por OTA. Gravação com apagamento total da flash ou mudança de partições pode removê-los.

## Comportamento e API

- Anuncia somente alterações efetivas de energia geral, andar ou modo, depois da validação do comando. Ligar tudo gera uma única frase.
- Cor, brilho e neve não geram fala. Alterar a cor também muda para cores fixas, silenciosamente.
- Nova ação falada cancela a fala antiga, sem acumular fila. Arquivo ausente cancela uma confirmação antiga e permanece silencioso.
- `inicio.wav` toca uma vez ao iniciar, independentemente de Wi-Fi. Na primeira instalação ainda estará ausente.
- Player em tarefa FreeRTOS com I2S/DMA. Durante envio de WAV ou OTA, o áudio para; OTA bem-sucedido reinicia a placa.
- `POST /control`: `action=audio` com `volume=0..100` e/ou `muted=true/false`.
- `GET /status`: mantém `brightness` físico (0..191) e acrescenta `brightness_percent` (0..100) e `audio` com `storage_ready`, `ready`, `volume`, `muted`, `playing` (índice da tabela, -1 parado) e `files` (`name`, `available`, `size`). `ready` indica inicialização do player, não detecta se o amplificador está conectado.
- `POST /audio/upload`: multipart, um campo `audio`, nome exato da tabela. Exige autenticação e origem local (ou ausente para CLI). Respostas: 400 formato/nome, 403 origem, 409 memória não preparada, 413 tamanho, 507 escrita/espaço, 500 substituição.
- `POST /audio/initialize`: formulário `confirm=ERASE_AUDIO_DATA`; disponível somente se a montagem falhou. Mesma autenticação e proteção de origem.

## Validação

A prévia local simula arquivos em RAM e confirmações, **sem emitir som nem persistir arquivos**. Na placa, conferir todas as falas, ações rápidas, silêncio, volume, Wi-Fi desconectado, animações durante reprodução e OTA preservando arquivos. A montagem e a qualidade sonora precisam de teste físico.

Com a prévia rodando, `build-audio-tests.ps1 -Clang caminho/clang++.exe` compila o mesmo parser WAV do firmware para WebAssembly e executa `check-audio.cjs`. Se o compilador não encontrar os headers, use `-SysrootInclude caminho/sysroot/include`. Os testes verificam WAV malformado, nomes, origem, substituição, limites e anúncios. `check-preview.cjs` cobre também volume, silêncio, envio pela página de gravações e três resoluções de tela. A variável `PREVIEW_URL` permite usar outra porta local. Os testes de áudio alteram apenas a simulação em RAM.

## Gerar com ElevenLabs

Voz feminina padrão Sarah (`EXAVITQu4vr4xnSDxMaL`), modelo `eleven_v4`, idioma `pt` e textos em português brasileiro. O sotaque deve ser avaliado ouvindo os arquivos. A voz brasileira Carla foi recusada pela API no plano gratuito. A chave fica em `.env.local` na raiz, ignorado pelo Git, e nunca vai para o ESP32. Execute na raiz:

```powershell
npm install
npm run audio:generate
```

O script gera as 15 falas em `smart-house-test/audio-pt-br/`, diretamente em WAV PCM mono de 16 bits e 16 kHz. Usa créditos ElevenLabs somente para arquivos ausentes; preserva os existentes. O `manifest.json` registra os textos, a voz e a duração. Os limites por arquivo e do conjunto são conferidos. Envie apenas os arquivos `.wav` pela página **Gravações** da casa. O ESP32 reproduz os arquivos localmente, sem depender do ElevenLabs durante o uso.
