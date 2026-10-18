# Sonar com Arduino Uno e Pygame

Radar semicircular baseado na referência: fundo escuro, grade e feixe verdes,
ecos vermelhos, ângulo e distância em tempo real. O ângulo vem da contagem
de passos, não de um encoder; a distância vem do HC-SR04.

## Componentes

- Arduino Uno, conectado ao computador por USB.
- Sensor ultrassônico HC-SR04.
- Placa driver ULN2003 com conector para motor de cinco fios.
- Motor de passo 28BYJ-48.
- Fonte regulada para o motor, fios e, se necessário, protoboard.

**O esquema abaixo considera o 28BYJ-48 de 5 V.** Confira a tensão escrita
no motor antes de ligar: também existem versões de 12 V. Para a versão de
12 V, somente a alimentação do conjunto motor/driver deve usar essa tensão,
respeitando as especificações da placa. O HC-SR04 continua em 5 V.

## Pinagem

Esta tabela corresponde ao firmware [`sonar.ino`](sonar.ino).
Use os nomes impressos nos módulos para identificar os terminais.

| Origem | Destino | Função |
|---|---|---|
| Uno D5 | ULN2003 IN1 | Controle da bobina 1 |
| Uno D4 | ULN2003 IN2 | Controle da bobina 2 |
| Uno D3 | ULN2003 IN3 | Controle da bobina 3 |
| Uno D2 | ULN2003 IN4 | Controle da bobina 4 |
| Uno D6 | HC-SR04 TRIG | Pulso para iniciar a medição |
| Uno D7 | HC-SR04 ECHO | Leitura do pulso de retorno |
| Uno 5V | HC-SR04 VCC | Alimentação do sensor |
| Uno GND | HC-SR04 GND | Terra do sensor |
| Fonte externa +5 V | ULN2003 + / VCC | Alimentação do motor de 5 V |
| Fonte externa negativo | ULN2003 − / GND | Terra da alimentação do motor |
| Uno GND | ULN2003 − / GND | Referência comum dos sinais |
| Conector de cinco fios do 28BYJ-48 | Conector de motor do ULN2003 | Ligação das bobinas ao driver |

O código usa `PINOS[] = {5, 3, 4, 2}` para ordenar as fases do motor.
**A ligação física permanece IN1 → D5, IN2 → D4, IN3 → D3 e IN4 → D2**;
não use a ordem do array como ordem dos conectores IN1 a IN4.

O Uno clássico usa lógica de 5 V, por isso o ECHO pode ir diretamente ao D7.
Essa ligação não deve ser copiada diretamente para um ESP32 de lógica de 3,3 V.

## Esquema de conexão

Montagem com Uno alimentado por USB e motor por fonte externa de 5 V:

```text
COMPUTADOR
    |
    | USB (alimentação + comunicação)
    v
ARDUINO UNO                       HC-SR04
    5V -------------------------- VCC
    GND ------------------------- GND
    D6 -------------------------- TRIG
    D7 -------------------------- ECHO

ARDUINO UNO                       DRIVER ULN2003
    D5 -------------------------- IN1
    D4 -------------------------- IN2
    D3 -------------------------- IN3
    D2 -------------------------- IN4
    GND -------------------+----- GND / −
                           |
FONTE EXTERNA 5 V           |
    negativo (−) ----------+
    positivo (+) ---------------- VCC / +

                                  ULN2003
                              conector do motor
                                      |
                              plugue de 5 fios
                                      |
                                 28BYJ-48
                                    (5 V)
```

Os GNDs do Uno, sensor, driver e fonte precisam estar conectados entre si.
Neste esquema, **não conecte o positivo da fonte externa ao pino 5V do Uno**:
o USB alimenta o Uno e o sensor, e a fonte externa alimenta o motor pelo driver.
Não conecte os 5 V do motor ao VIN do Uno.

## Montagem

1. Desconecte o USB e desligue a fonte antes de montar ou trocar fios.
2. Encaixe o plugue de cinco fios do 28BYJ-48 no conector correspondente do
   ULN2003, respeitando a orientação do encaixe. Não ligue o motor diretamente
   aos pinos do Uno.
3. Conecte IN1 a IN4 e os quatro terminais do HC-SR04 conforme a tabela.
4. Ligue a fonte ao driver e una os GNDs. Confira a polaridade e a tensão
   antes de energizar. A fonte precisa suportar o consumo do motor; não
   alimente as bobinas nem o VCC do sensor por um pino digital.
5. Fixe o sensor no suporte do motor e deixe folga nos fios para a varredura
   de 180°, sem enroscar ou puxar os conectores.
6. Com o motor desenergizado, ajuste o suporte para o centro da varredura
   (90°). Depois ligue a fonte e o USB e execute as instruções abaixo.

O firmware não tem sensor de posição: o ângulo é estimado pela contagem de
passos. A referência central deve ser refeita após reiniciar. Ao pausar,
as bobinas podem continuar energizadas; desligue a fonte antes de ajustar
o mecanismo manualmente.

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
