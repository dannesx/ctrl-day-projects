# Ctrl Day Projects

Projetos com ESP32, Arduino e interfaces para experimentos de eletrônica e robótica.

- **clock/**: relógio com sincronização NTP e fuso UTC−3.
- **jokenpo/**: jogo com reconhecimento de gestos pela webcam. Veja [as instruções](jokenpo/README.md).
- **display/**: editor de telas para o OLED. Veja [as instruções](display/README.md).
- **sonar/**: radar com Arduino Uno, sensor ultrassônico e interface Pygame. Veja [as instruções](sonar/README.md).

## Relógio

Ligações no ESP32 clássico: VCC → 3V3, GND → GND, SDA → GPIO21 e SCL → GPIO22.

Copie `clock/wifi_config.example.h` para `clock/wifi_config.h` e preencha sua rede e senha. O arquivo de credenciais não é versionado.

```sh
cd clock
make setup
make compile
make upload PORT=/dev/ttyUSB0
```

O projeto utiliza Arduino CLI. Consulte os Makefiles e os READMEs de cada projeto para suas dependências.
