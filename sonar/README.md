# Sonar com Arduino Uno e Pygame

Radar semicircular baseado na referência: fundo escuro, grade e feixe verdes,
ecos vermelhos, ângulo e distância em tempo real. O ângulo vem da contagem
de passos, não de um encoder; a distância vem do HC-SR04.

## Ligações

Desligue a alimentação antes de alterar os fios.

| Componente | Uno |
|---|---|
| Driver IN1 / IN2 / IN3 / IN4 | D5 / D4 / D3 / D2 |
| HC-SR04 TRIG / ECHO | D6 / D7 |
| HC-SR04 VCC / GND | 5V / GND |

Use driver para o motor. O sensor pode compartilhar o barramento 5V;
a fonte deve suportar o consumo total. Com fonte separada para o motor,
una os GNDs. Não alimente o sensor por um pino digital.

## Executar

Requer `uv`, Python 3.12, `arduino-cli`, core `arduino:avr`.

```sh
make setup
make upload                  # compila, grava e verifica o Uno
make                         # abre o radar real
```

Outra porta: `make upload PORT=/dev/ttyACM0` e `make PORT=/dev/ttyACM0`.
Feche o radar e qualquer monitor serial antes de gravar.

Ao conectar, o Uno normalmente reinicia. O novo firmware aguarda parado.
Posicione fisicamente o conjunto em **90°**, com o motor desenergizado se
precisar ajustar mecanicamente. Ligue, abra o radar e pressione **Espaço**.
O movimento começa em +90°, chegando a 180°, e repete −180° / +180°.
Não force o eixo energizado. A cada reset/reconexão, a referência de 90°
precisa ser estabelecida novamente: não há fim de curso nem homing automático.

O motor avanca um passo a cada aproximadamente 2,93 ms (10 RPM), com
agendamento por `micros()`. O sensor mede a cada 65 ms, sem `pulseIn()` nem
esperas bloqueantes. O angulo enviado corresponde ao inicio da medicao;
o motor continua andando durante o eco. Sem eco valido, envia -1.
A inversao nos extremos continua imediata, sem rampa de aceleracao.

- **Espaço:** iniciar, pausar ou retomar sem perder a posição da sessão.
- **+ / −:** alcance de 20, 50, 100, 200 ou 400 cm (inicial: 100 cm).
- **C:** limpar os rastros.
- **F11:** alternar tela cheia, preservando a conexão e a varredura.
- **Esc:** voltar à janela; em modo janela, sair e solicitar pausa do motor.

Para iniciar em tela cheia: `.venv/bin/python radar.py --fullscreen`.
Textos, margens, pontos e traços acompanham o tamanho da janela.

O ângulo 0° fica à esquerda e 180° à direita. Os ecos desaparecem após 22 s.
Em desconexão, o aplicativo tenta reabrir a porta, sem iniciar o motor sozinho.
Uma interrupção abrupta do programa ou do USB não garante a pausa se o Uno
continuar alimentado por outra fonte.

## Demonstração e prévia

```sh
make demo                    # dados simulados, identificados na tela
make preview                 # gera preview.png sem precisar de uma janela
```

O modo real nunca substitui dados ausentes por dados simulados.
Protocolo: linhas `angulo,distancia_cm` em 115200 baud; `READY` ao iniciar.
Comandos: `S` assume posição 90° e inicia; `P` pausa; `R` retoma.
