# Editor OLED — interface de operação

O desenho é a área principal. Navegação por telas à esquerda; grade com rolagem própria ao centro; prévia e exportações à direita. Em telas estreitas, os painéis se empilham e a coleção vira uma lista horizontal.

A interface usa superfícies escuras para que a luminosidade dos LEDs possa ser avaliada. Amarelo e azul representam exclusivamente as faixas físicas; ações principais usam verde claro neutro. Tipografia de interface do sistema; monoespaçada apenas para coordenadas, medidas e código.

Tokens em `style.css`: fundo `#16191b`, painel `#1d2124`, superfície elevada `#262c30`, borda `#3c454b`, texto `#f0f2ef`, secundário `#aeb8bd`, amarelo `#ffe353`, azul `#39caff`, ação `#dce9df`, alerta `#ffb5ac`. Espaçamentos de 4, 8, 12, 16, 24, 28 e 40 pixels. Botões com raio de 6px; sem animações no canvas.

O canvas é binário, sem suavização. A grade é apenas uma ajuda de edição, ausente na exportação. Estados explícitos de salvamento, falha, conflito entre abas, seleção, hover, foco e botões indisponíveis. A fonte real Adafruit GFX é usada nos exemplos gerados; não se redesenham textos com fontes do navegador.
