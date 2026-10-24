# Controle web

Abra http://192.168.137.173/ no computador que fornece o hotspot ou em um dispositivo na mesma rede do ESP32. Entre com o usuário e a senha OTA configurados em `wifi_config.h`. O IP pode mudar; `http://smart-house.local/` depende de mDNS.

O painel controla os três andares: ligar/desligar e cor de cada andar, brilho geral de 0 a 100%, ligar/desligar tudo e ciclo HSL sincronizado de 8 segundos. Escolher uma cor liga o andar e ativa as cores fixas. O estado é consultado a cada 2,5 segundos.

Ao reiniciar, a casa volta ao ciclo de 8 segundos, todos os ambientes ligados e brilho de 100%. Ajustes não são gravados em flash. A atualização de firmware continua em `/update`.

Mapeamento: SUPERIOR = D14 (12 LEDs); CENTRAL = D12 (9 LEDs); TERREO = D13 (9 LEDs). Cada comando afeta toda a fita do andar.

API autenticada: `GET /status`; `POST /control` com formulário URL-encoded:

| action | Campos |
| --- | --- |
| all | `value=on` ou `off` |
| brightness | `value=0..100` |
| mode | `value=manual`, `hsl_cycle`, `breathing`, `color_wave`, `chase`, `snow`, `halloween` ou `blink` |
| floor | `id` e `on=true/false` ou `color=#RRGGBB` |
| snow | `speed=1..5` e/ou `density=1..8` |

IDs: `SUPERIOR`, `CENTRAL`, `TERREO`. Valores inválidos retornam 400 sem alterar o estado. Origem externa retorna 403.

Para compilar e atualizar: `.\smart-house-test\update.ps1 -Address 192.168.137.173`.

## Efeitos

No seletor de atmosfera:

- Respiração: cada andar pulsa suavemente na sua cor, com período de 4 segundos.
- Ondas de cores: os andares percorrem matizes com diferença de 120 graus, em um ciclo de 8 segundos.
- Sequência: a luz sobe e desce entre os andares, com transição de intensidade e 900 ms por etapa.

O ciclo sincronizado de 8 segundos e as cores fixas continuam disponíveis. Os efeitos respeitam os andares desligados e o brilho geral. Respiração e sequência usam as cores individuais; escolher uma cor retorna ao modo fixo. `/status` inclui `rendered_color` com a cor enviada a cada fita no último quadro.

A cor de cada andar agora usa um controle de matiz de 0 a 359 graus, com saturacao de 100% e luminosidade HSL de 50%. O valor aparece em graus. Ao soltar o controle, a cor e enviada para o andar e o modo de cores fixas e ativado.

## Neve

Selecione Neve em Atmosfera: pontos brancos com rastro suave descem do andar superior ao terreo, sobre azul escuro. Velocidade (1 a 5) e quantidade de flocos (1 a 8) aparecem neste modo. O brilho geral e os andares desligados sao respeitados. Padrao: velocidade 3, quatro flocos. A API aceita `mode=snow` e `action=snow` com `speed` e/ou `density`; em neve o estado inclui a lista `pixels` por andar.

## Halloween

Selecione Halloween em Atmosfera: fundo roxo com pulsos laranja escalonados entre os andares em 6 segundos, com variacao suave de intensidade por LED simulando velas. Respeita o brilho geral e os andares desligados. API: `action=mode&value=halloween`; `/status` inclui `pixels` neste modo.

## Efeitos piscantes

Halloween agora inclui dois flashes laranja de 120 ms por andar, intercalados com sombras e iluminacao de velas. Cada andar repete a sequencia em 3 segundos, com deslocamento de 1 segundo entre andares. Pisca-pisca (`blink`) alterna as cores escolhidas e apagado a cada 450 ms, com deslocamento de 300 ms entre andares. Ambos respeitam brilho e andares desligados.
