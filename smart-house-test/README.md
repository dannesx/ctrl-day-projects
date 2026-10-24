# Versão atual: luzes e voz

O firmware atual inclui a interface web, limite físico de brilho em 75% e suporte ao MAX98357A. Consulte [WEB.md](WEB.md) e [AUDIO.md](AUDIO.md) para uso, ligações e envio das gravações. O texto abaixo registra as etapas anteriores do teste; valores de 100% físico se referem às versões antigas.

# Controle de luzes da casa

**Versão atual: controle pelo navegador para a feira.** Consulte [o painel web, prévia local e comandos](WEB.md) e [o guia de operação](../GUIA-FEIRA.md). Seis modos: cores fixas, ciclo de cores, Neve, Halloween, Natal e Matrix. A configuração atual usa 10 LEDs por andar (3×10), nos GPIOs 14, 12 e 13. O histórico abaixo descreve as etapas anteriores de teste.

Acende os 30 LEDs dos tr?s andares: 12 em D14 / GPIO14, 9 em D12 / GPIO12 e 9 em D13 / GPIO13. Todos mostram a mesma cor em um ciclo cont?nuo de matiz HSL de **8 segundos**, com brilho global **255/255 (100%)**.

A saturação HSL é 100% e a luminosidade HSL é 50%, para obter cores saturadas. Isso é independente do brilho global da fita, que está em 100%; luminosidade HSL de 100% seria branco. A animação atualiza a cada 20 ms, sem bloquear o atendimento OTA. `/status` informa a matiz efetivamente desenhada no último quadro.

Para o teste inicial via USB, foi escolhido brilho reduzido de 10%. A ligação informada é 5 V da fita em VIN, GND em GND e DIN em D14. Para ampliar a iluminação, use alimentação externa de 5 V e GND comum ao ESP32, conforme as [ligações da Smart House](../smart-house/README.md).

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 smart-house-test
arduino-cli upload --port COM4 --fqbn esp32:esp32:esp32 smart-house-test
```

O teste permanece aceso até desligar ou gravar outro programa. Na serial a 115200 baud, enviar qualquer texto mostra a configuração ativa. Para retomar o controle por cômodo, grave novamente `smart-house/smart-house.ino`.

## Atualização pela mesma rede Wi-Fi

Este teste inclui atualização OTA pelo navegador. Copie `wifi_config.example.h` para `wifi_config.h`, configure a rede de 2,4 GHz e uma senha de atualização. O arquivo real é ignorado pelo Git; não compartilhe o firmware compilado, que contém as credenciais. O usuário e a senha da página estão em `OTA_USER` e `OTA_PASSWORD` nesse arquivo.

A primeira instalação é por USB. Com a fonte desligada, desconecte seu positivo do VIN e desconecte a fita antes de ligar o USB. Após gravar, retire o USB e restaure a alimentação pela fonte. Não una as duas saídas de 5 V.

Ao conectar à rede, o ESP32 imprime seu IP na serial. Abra `http://IP_DO_ESP32/` ou `http://smart-house.local/` (se mDNS funcionar). Entre com a credencial OTA e envie somente o arquivo `smart-house-test.ino.bin` gerado pela compilação. Aguarde a confirmação e o reinício sem desligar a fonte. O nome local e o IP dependem da rede; o roteador pode mudar o IP ao reconectar.

No PowerShell, também é possível compilar e enviar sem expor a senha na linha de comando:

```powershell
.\smart-house-test\update.ps1 -Address 192.168.1.123
```

Troque o IP pelo endereço mostrado pela placa. Para enviar um binário já compilado, acrescente `-Firmware .\.build\smart-house-test\smart-house-test.ino.bin`.

O computador pode permanecer no seu Wi-Fi habitual. Redes de convidados/escolas podem isolar dispositivos: nesse caso, o ESP32 conecta ao Wi-Fi, mas a página não abre no computador; o responsável pela rede precisa permitir a comunicação local.

### Usar o hotspot do Windows mantendo a internet

Em **Configurações → Rede e Internet → Hotspot móvel**, compartilhe a conexão Wi-Fi por Wi-Fi e configure a banda **2,4 GHz**. O computador permanece na rede principal com internet; somente o ESP32 conecta ao hotspot. Configure em `wifi_config.h` o nome e a senha desse hotspot. Mantenha o hotspot ativo enquanto usar OTA; se o Windows oferecer desligamento automático por economia de energia, desative essa opção para evitar quedas.

No teste local, o hotspot do computador usa `192.168.137.1` e o ESP32 recebeu `192.168.137.13`. Esses endereços podem mudar. Use a página `http://192.168.137.13/` ou o script:

```powershell
.\smart-house-test\update.ps1 -Address 192.168.137.13
```

O acesso sem credenciais deve retornar HTTP 401. `/status` autenticado informa a versão, o brilho e o tempo desde o último reinício. A conexão TCP da placa ao próprio IP não é usada como prova de funcionamento: valide o acesso a partir do computador.

Validação realizada no hardware: página autenticada com HTTP 200; consulta e envio sem senha rejeitados com HTTP 401; firmware `smart-house-test-ota-3` atualizado para `smart-house-test-ota-4` por Wi-Fi, com confirmação HTTP, reinício e consulta da versão nova. Os 10 LEDs em D14 mantiveram a configuração de brilho 26/255. O script desativa `Expect: 100-continue`, necessário para concluir o envio neste teste.

O teste continua funcionando enquanto o Wi-Fi reconecta. Toda versão enviada deve manter `ota_support.h`, `wifi_config.h`, `otaSetup()` e `otaLoop()` para permitir futuras atualizações sem USB. O firmware principal em `../smart-house` ainda não contém OTA: gravá-lo remove essa função.
