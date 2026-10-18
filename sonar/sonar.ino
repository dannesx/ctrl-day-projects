const int PASSOS_POR_VOLTA = 2048;
const byte TRIG = 6;
const byte ECHO = 7;
// Ordem eletrica equivalente a Stepper(2048, 5, 3, 4, 2).
const byte PINOS[] = {5, 3, 4, 2};
const byte FASES[][4] = {{1, 0, 1, 0}, {0, 1, 1, 0},
                          {0, 1, 0, 1}, {1, 0, 0, 1}};
const unsigned long INTERVALO_PASSO_US = 60000000UL / (2048UL * 10);
const unsigned long INTERVALO_MEDICAO_US = 65000UL;
const unsigned long TIMEOUT_ECO_US = 25000UL;
int posicao = PASSOS_POR_VOLTA / 4;
int sentido = 1;
byte fase = 0;
bool ativo = false;
unsigned long ultimoPasso = 0;
unsigned long ultimaMedicao = 0;
unsigned long inicioPulso = 0;
unsigned long inicioEco = 0;
int posicaoMedida = 0;
enum EstadoSensor { OCIOSO, TRIGGER_ALTO, ESPERA_SUBIDA, ESPERA_DESCIDA };
EstadoSensor estadoSensor = OCIOSO;

void cancelarMedicao() {
  digitalWrite(TRIG, LOW);
  estadoSensor = OCIOSO;
}

void publicar(unsigned long duracao) {
  float distancia = duracao * 0.0343f / 2.0f;
  // Uma linha tem menos de 24 bytes. Nunca esperar pelo buffer serial.
  if (Serial.availableForWrite() >= 24) {
    Serial.print(posicaoMedida * 360.0f / PASSOS_POR_VOLTA, 1);
    Serial.print(',');
    if (duracao == 0 || distancia < 2 || distancia > 400) Serial.println(-1);
    else Serial.println(distancia, 1);
  }
  estadoSensor = OCIOSO;
}

void atualizarSensor(unsigned long agora) {
  switch (estadoSensor) {
    case OCIOSO:
      if (agora - ultimaMedicao >= INTERVALO_MEDICAO_US) {
        ultimaMedicao = agora;
        posicaoMedida = posicao;
        digitalWrite(TRIG, HIGH);
        inicioPulso = agora;
        estadoSensor = TRIGGER_ALTO;
      }
      break;
    case TRIGGER_ALTO:
      if (agora - inicioPulso >= 10) {
        digitalWrite(TRIG, LOW);
        inicioPulso = agora;
        estadoSensor = ESPERA_SUBIDA;
      }
      break;
    case ESPERA_SUBIDA:
      if (agora - inicioPulso >= TIMEOUT_ECO_US) publicar(0);
      else if (digitalRead(ECHO) == HIGH) {
        inicioEco = agora;
        estadoSensor = ESPERA_DESCIDA;
      }
      break;
    case ESPERA_DESCIDA:
      if (agora - inicioEco >= TIMEOUT_ECO_US) publicar(0);
      else if (digitalRead(ECHO) == LOW) publicar(agora - inicioEco);
      break;
  }
}

void atualizarMotor(unsigned long agora) {
  if (agora - ultimoPasso < INTERVALO_PASSO_US) return;
  // Nao compensar atrasos com rajadas de passos.
  ultimoPasso = agora;
  if (posicao >= PASSOS_POR_VOLTA / 2) sentido = -1;
  if (posicao <= 0) sentido = 1;
  fase = (fase + (sentido > 0 ? 1 : 3)) % 4;
  for (byte i = 0; i < 4; ++i) digitalWrite(PINOS[i], FASES[fase][i]);
  posicao += sentido;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  digitalWrite(TRIG, LOW);
  for (byte i = 0; i < 4; ++i) pinMode(PINOS[i], OUTPUT);
  Serial.println("READY");
}

void loop() {
  // Um comando por iteracao para nao atrasar o motor/eco.
  if (Serial.available()) {
    char comando = Serial.read();
    if (comando == 'S' || comando == 'R') {
      if (comando == 'S') {
        // O usuario estabeleceu fisicamente a referencia de 90 graus.
        posicao = PASSOS_POR_VOLTA / 4;
        sentido = 1;
      }
      ativo = true;
      cancelarMedicao();
      ultimoPasso = micros();
      ultimaMedicao = ultimoPasso - INTERVALO_MEDICAO_US;
    } else if (comando == 'P') {
      ativo = false;
      cancelarMedicao();
    }
  }
  if (!ativo) return;
  atualizarSensor(micros());
  atualizarMotor(micros());
}
