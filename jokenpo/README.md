# Jokenpo: webcam + ESP32

O computador reconhece a mão usando OpenCV e MediaPipe Gesture Recognizer. Os frames ficam no computador. O ESP32 controla a rodada e sorteia sua escolha no início, antes de receber a escolha do jogador. A comunicação usa HTTP na rede local.

## Como jogar

1. Segure um 👍 por 0,6 segundo para começar.
2. O OLED mostra `JO`, `KEN`, `PO!!`, cada um durante 1 segundo, em fonte ampliada 5× e centralizada na faixa azul. A faixa amarela fica apagada durante a contagem.
3. Durante o `PO!!`, mostre a escolha e mantenha até terminar: punho fechado = pedra; palma aberta = papel; indicador e médio em V = tesoura.
4. Ao completar 3 segundos, o ESP usa o último gesto estável e recente. Mostra as duas escolhas e vitória, derrota ou empate durante 5 segundos.
5. Volta ao standby. Solte o joinha por pelo menos meio segundo antes de fazer outro.

Sem gesto válido ao terminar, a rodada mostra `SEM JOGADA`. Imagens ruins ou conexão perdida não reutilizam a escolha de uma rodada anterior. Use uma mão, bem iluminada e completamente visível, voltada para a câmera.

## Placa e display

ESP32 clássico + OLED SSD1306 I²C 128×64: VCC → 3V3, GND → GND, SDA → GPIO21, SCK/SCL → GPIO22. Detecta endereço 0x3C ou 0x3D. Cabeçalho em y=4 na faixa amarela; restante na faixa azul.

As mãos usam bitmaps de 32×28 pixels em `hands.h`, centralizados em cada coluna entre y=32 e y=59. Seguem a referência com contornos acesos, interior preto e tesoura inclinada para a direita. A tela de resultado preenche os 16 pixels da faixa amarela e escreve o resultado em preto; `VOCE` e `ESP32` ficam acima das mãos na faixa azul. Para editar os desenhos, altere `art/generate.py` e execute `python3 art/generate.py` (requer Pillow). Isso regenera o header e `art/preview.png`; a prévia amplia os pixels sem suavização e usa uma fonte ilustrativa para os textos.

`wifi_config.h` foi criado localmente usando a configuração do relógio, sem imprimir a senha. Está ignorado pelo Git. Para uma instalação nova, copie `wifi_config.example.h` para `wifi_config.h` e preencha. Computador e placa devem estar na mesma rede, sem isolamento entre clientes.

```bash
cd jokenpo
make upload
```

A gravação substitui o relógio na placa; os arquivos de `clock` continuam disponíveis. Se necessário, segure BOOT durante `Connecting...`. O IP aparece no OLED em standby e no monitor serial a 115200 baud.

## Computador

Crie um ambiente virtual com uma versão de Python suportada pelos pacotes. Python 3.11 ou 3.12 é uma opção prática; se o pip disser `No matching distribution` no Python 3.14, use uma dessas versões:

```bash
make setup PYTHON=python3.12
make run ESP=http://192.168.1.100
```

Substitua pelo IP exibido na placa. Pode também usar `make run` para tentar `http://jokenpo.local` via mDNS. A primeira execução baixa o modelo oficial (~8 MB); depois o reconhecimento funciona localmente. Não precisa de chave de API.

Para selecionar outra câmera:

```bash
.venv/bin/python webcam.py --esp http://192.168.1.100 --camera 1
```

Q ou Esc encerra. `make test` verifica estabilidade dos gestos e prevenção de rodadas repetidas. `make compile` compila sem gravar.

## Referências

- [MediaPipe Gesture Recognizer para Python](https://ai.google.dev/edge/mediapipe/solutions/vision/gesture_recognizer/python)
- [Servidor HTTP do Arduino ESP32](https://github.com/espressif/arduino-esp32/tree/master/libraries/WebServer/examples/HelloServer)

## Fluidez e reconhecimento

O preview, a inferência e a comunicação Wi-Fi agora trabalham separadamente. O processamento descarta frames pendentes antigos, usa imagens de 480 pixels de largura e tolera falhas de detecção inferiores a 150 ms. O preview mostra FPS e confiança para facilitar a avaliação na webcam real.

Em computadores lentos, reduza a largura de inferência:

```bash
.venv/bin/python webcam.py --esp http://192.168.1.100 --inference-width 320
```

O limiar padrão de confiança é 0,50. Para uma exigência maior, use `--confidence 0.65`. A câmera continua em 640×480; somente a imagem enviada ao modelo é reduzida. Mantenha a mão inteira visível e iluminada.
