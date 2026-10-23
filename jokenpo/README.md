# Jokenpo: webcam + ESP32

O computador reconhece a mão usando OpenCV e MediaPipe Gesture Recognizer. Os frames ficam no computador. O ESP32 controla a rodada e sorteia sua escolha no início, sem usar a escolha do jogador no sorteio. A comunicação usa HTTP na rede local.

## Como jogar

1. Mostre pedra, papel ou tesoura por 0,3 segundo para começar automaticamente.
2. O OLED mostra `JO`, `KEN`, `PO!!`, cada um durante 0,7 segundo, em fonte ampliada 5× e centralizada na faixa azul. A faixa amarela fica apagada durante a contagem.
3. A primeira escolha estável fica salva no ESP ao iniciar: punho fechado = pedra; mão aberta = papel; indicador e médio em V = tesoura. Pode retirar ou mudar a mão durante a contagem: a jogada confirmada não muda.
4. Ao completar 2,1 segundos, o ESP resolve a rodada com a escolha confirmada no início. Mostra as duas escolhas e vitória, derrota ou empate durante 3,5 segundos.
5. Volta ao standby. Após o resultado, manter qualquer opção por 0,3 segundo inicia outra rodada, mesmo sem retirar a mão ou mudar de gesto. O início só é enviado uma vez enquanto aguarda a resposta do ESP.

Sem uma escolha válida não se inicia a rodada. Depois que o ESP confirma a escolha, perder a imagem ou a conexão durante a contagem não apaga nem altera essa jogada. Cada nova rodada exige uma nova confirmação estável. Use uma mão, bem iluminada e completamente visível. Pedra, papel e tesoura são classificados pela posição dos dedos em 3D, aceitando palma ou costas da mão voltadas à câmera; qualquer uma das três opções inicia a rodada automaticamente. Somente pedra, papel e tesoura são opções: joinha e outros gestos reconhecidos pelo modelo são ignorados. Poses ambíguas não enviam jogada.

## Placa e display

ESP32 clássico + OLED SSD1306 I²C 128×64: VCC → 3V3, GND → GND, SDA → GPIO21, SCK/SCL → GPIO22. Detecta endereço 0x3C ou 0x3D. Cabeçalho em y=4 na faixa amarela; restante na faixa azul.

As mãos usam bitmaps de 32×28 pixels em `hands.h`, centralizados em cada coluna entre y=32 e y=59. Seguem a referência com contornos acesos, interior preto e tesoura inclinada para a direita. A tela de resultado preenche os 16 pixels da faixa amarela e escreve o resultado em preto; `VOCE` e `ESP32` ficam acima das mãos na faixa azul. Para editar os desenhos, altere `art/generate.py` e execute `python3 art/generate.py` (requer Pillow). Isso regenera o header e `art/preview.png`; a prévia amplia os pixels sem suavização e usa uma fonte ilustrativa para os textos.

`wifi_config.h` foi criado localmente usando a configuração do relógio, sem imprimir a senha. Está ignorado pelo Git. Para uma instalação nova, copie `wifi_config.example.h` para `wifi_config.h` e preencha. Computador e placa devem estar na mesma rede, sem isolamento entre clientes.

```bash
cd jokenpo
make upload
```

A gravação substitui o relógio na placa; os arquivos de `clock` continuam disponíveis. Se necessário, segure BOOT durante `Connecting...`. O IP aparece no OLED em standby e no monitor serial a 115200 baud.

## Buzzer

Buzzer passivo: positivo/sinal → D23 (GPIO23), negativo → GND.
A contagem toca JO em 660 Hz por 140 ms, KEN em 880 Hz por 140 ms
 e PO em 1320 Hz por 600 ms. O PWM não bloqueia a leitura dos gestos
nem o servidor HTTP. Vitória do jogador toca uma melodia ascendente; derrota,
uma descendente; empate, três notas iguais. Cada melodia dura 900 ms
e toca uma única vez ao mostrar o resultado. Sem jogada válida não há melodia. Um buzzer ativo pode não reproduzir tons diferentes.

## Computador

Crie um ambiente virtual com uma versão de Python suportada pelos pacotes. Python 3.11 ou 3.12 é uma opção prática; se o pip disser `No matching distribution` no Python 3.14, use uma dessas versões:

```bash
make setup PYTHON=python3.12
make run ESP=http://192.168.1.100
```

Substitua pelo IP exibido na placa. Pode também usar `make run` para tentar `http://jokenpo.local` via mDNS. A primeira execução baixa o modelo oficial (~8 MB); depois o reconhecimento funciona localmente. Não precisa de chave de API.

O comando `make run` prefere automaticamente a webcam externa USB. Sem webcam externa, usa a câmera padrão.

Para selecionar outra câmera manualmente:

```bash
.venv/bin/python webcam.py --esp http://192.168.1.100 --camera 1
```

Q ou Esc encerra. `make test` verifica estabilidade dos gestos e prevenção de rodadas repetidas. `make compile` compila sem gravar.

## Referências

- [MediaPipe Gesture Recognizer para Python](https://ai.google.dev/edge/mediapipe/solutions/vision/gesture_recognizer/python)
- [Servidor HTTP do Arduino ESP32](https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer/examples/HelloServer)

## Fluidez e reconhecimento

O preview, a inferência e a comunicação Wi-Fi agora trabalham separadamente. O processamento descarta frames pendentes antigos, usa imagens de 640 pixels de largura e tolera falhas de detecção inferiores a 150 ms. O preview desenha os pontos da mão e informa se a mão não foi detectada, se a pose está indefinida ou se a rodada está sendo iniciada. A classificação combina geometria 3D dos dedos (com tolerância à curvatura) e o modelo treinado como alternativa quando a geometria é ambígua e a confiança é de pelo menos 0,65. Poses geométricas não exibem porcentagens inventadas. O Wi-Fi do ESP fica sem economia de energia, e o cliente tolera respostas de até 1,2 segundo sem bloquear o preview.

Em computadores lentos, reduza a largura de inferência:

```bash
.venv/bin/python webcam.py --esp http://192.168.1.100 --inference-width 320
```

O limiar padrão de confiança é 0,50. Para uma exigência maior, use `--confidence 0.65`. A câmera solicita 640×360 em MJPEG a 30 FPS; a taxa efetiva depende dos modos suportados pela webcam e aparece no preview. Somente a imagem enviada ao modelo é reduzida. Mantenha a mão inteira visível e iluminada.
