#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_random.h>
#include <LittleFS.h>
#include "learning.h"

// Independent access point: no router or external web assets required.
const char* AP_SSID = "ESP32-TicTacToe";
const char* AP_PASSWORD = "jogodavelha";
Adafruit_SSD1306 display(128, 64, &Wire, -1);
WebServer server(80);
bool oled = false;
Learner learner;
bool training=false, trained=false, modelSaved=false, storageReady=false;
uint32_t lastTrainingDraw=0;

uint32_t modelHash() {
  uint32_t hash=2166136261U;
  const uint8_t* bytes=reinterpret_cast<const uint8_t*>(learner.q);
  for(size_t i=0;i<sizeof(learner.q);++i)hash=(hash^bytes[i])*16777619U;
  return hash;
}
bool saveModel() {
  if(!storageReady)return false;
  File file=LittleFS.open("/ttt-model.tmp","w");
  if(!file)return false;
  uint32_t header[3]={0x54545101, Learner::EPISODES, modelHash()};
  bool ok=file.write(reinterpret_cast<uint8_t*>(header),sizeof(header))==sizeof(header);
  ok=ok && file.write(reinterpret_cast<uint8_t*>(learner.q),sizeof(learner.q))==sizeof(learner.q);
  file.close();
  return ok && LittleFS.rename("/ttt-model.tmp","/ttt-model.bin");
}
void loadModel() {
  // This sketch owns its LittleFS partition; format only if mounting fails.
  storageReady=LittleFS.begin(true);
  if(!storageReady)return;
  File file=LittleFS.open("/ttt-model.bin","r");
  if(!file || file.size()!=sizeof(learner.q)+12)return;
  uint32_t header[3];
  bool ok=file.read(reinterpret_cast<uint8_t*>(header),sizeof(header))==sizeof(header);
  ok=ok && header[0]==0x54545101 && header[1]==Learner::EPISODES;
  ok=ok && file.read(reinterpret_cast<uint8_t*>(learner.q),sizeof(learner.q))==sizeof(learner.q);
  if(ok && modelHash()==header[2]) {trained=true;modelSaved=true;learner.episodes=Learner::EPISODES;}
  else learner.reset(1);
}
char board[10] = "---------";
char result = '-'; // X, O, D (draw), or - (playing).
bool espTurn = false;
bool nextEspStarts = false; // Player starts the first game after boot.
uint32_t espAt = 0, revision = 0;
const uint8_t lines[8][3] = {
  {0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}
};

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Jogo da velha · ESP32</title>
<style>
:root{color-scheme:light;--paper:#f6f3e9;--ink:#213c33;--line:#213c33;--x:#176343;--o:#a33d23}
*{box-sizing:border-box}
body{margin:0;background:var(--paper);color:var(--ink);font:18px/1.5 system-ui,sans-serif;min-height:100svh;display:grid;place-items:center;padding:32px 20px}
main{width:min(100%,440px);text-align:center}
h1{font-size:clamp(34px,8.5vw,52px);font-weight:900;line-height:1.05;letter-spacing:-.035em;margin:0 0 14px;text-wrap:balance}
p{margin:8px 0}header p{font-size:16px;line-height:1.5}
.players{display:flex;justify-content:center;gap:48px;margin:26px 0 12px;font-size:18px;font-weight:800}
.x{color:var(--x)}.o{color:var(--o)}
#board{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:5px;background:var(--line);width:min(100%,360px);margin:22px auto 26px}
button{font:inherit;cursor:pointer;touch-action:manipulation}
#board button{display:grid;place-items:center;aspect-ratio:1;min-width:0;border:0;background:var(--paper);font-size:clamp(48px,16vw,80px);line-height:1;font-weight:900;padding:0}
#board button:enabled:hover{background:#e3eadd}
#board button:enabled:active{background:#d7e6cb}
button:focus-visible{outline:3px solid var(--x);outline-offset:-6px}
#board button:disabled{cursor:default;opacity:1}
#status{font-size:clamp(18px,4.8vw,24px);line-height:1.4;font-weight:800;min-height:34px}
#restart{display:block;width:min(100%,360px);min-height:54px;border:0;border-radius:8px;padding:14px 20px;color:var(--paper);background:var(--ink);margin:0 auto;font-weight:800;font-size:18px}
#restart:enabled:hover{background:var(--x)}#restart:enabled:active{background:#152b23}
#restart:disabled{opacity:.55;cursor:wait}#restart:focus-visible{outline-color:var(--paper)}
#error{color:#9b2d20;font-size:15px;font-weight:600;margin:14px 0 0}#error:empty{display:none}
footer{font-size:13px;line-height:1.6;margin-top:20px}::selection{background:#d7e6b3}
@media(max-height:740px){body{padding-block:20px}.players{margin-top:18px}#board{width:min(100%,300px);margin-block:16px 20px}h1{font-size:36px}footer{margin-top:16px}}
#train{display:block;width:min(100%,360px);margin:12px auto 0;padding:14px;min-height:52px;border:2px solid var(--ink);border-radius:8px;background:transparent;color:var(--ink);font-weight:900}
#train:enabled:hover{background:#e3eadd}#train:disabled{opacity:.55;cursor:default}
#mode{font-weight:800;margin-top:18px}#training{padding:24px 0;margin:22px auto;min-height:250px;align-content:center}
#training[hidden]{display:none}progress{display:block;width:min(100%,320px);height:18px;margin:20px auto;accent-color:var(--x)}
.orbit{width:56px;height:56px;border:6px solid #d7e6cb;border-top-color:var(--ink);border-radius:50%;margin:0 auto 20px;animation:spin 1s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}@media(prefers-reduced-motion:reduce){.orbit{animation:none}}
</style>
<main><header><h1>Jogo da velha</h1><p>Você contra o ESP32.<br>Escolha uma casa para jogar.</p></header>
<div class="players"><span class="x">X · Você</span><span class="o">O · ESP</span></div>
<p id="status" role="status" aria-live="polite">Conectando…</p>
<div id="board" role="group" aria-label="Tabuleiro de jogo da velha"></div>
<section id="training" hidden aria-label="Treinamento"><div class="orbit" aria-hidden="true"></div><strong>Aprendendo a jogar</strong><progress id="progress" max="200000" value="0" aria-label="Progresso do treino"></progress><p id="training-count"></p><small>O ESP está jogando partidas simuladas.</small></section>
<button id="restart" disabled>Nova partida</button><button id="train" disabled>TREINAR</button><p id="mode" role="status">Modo aleatório</p><p id="error" role="alert"></p>
<footer>Início alternado · Você é X.<br>O tabuleiro também aparece no OLED.</footer></main>
<script>
const grid=document.querySelector('#board'),status=document.querySelector('#status'),error=document.querySelector('#error'),restart=document.querySelector('#restart');
const train=document.querySelector('#train'),trainingPanel=document.querySelector('#training'),progress=document.querySelector('#progress'),trainingCount=document.querySelector('#training-count'),mode=document.querySelector('#mode');
let state=null,busy=false,online=false,polling=null;
const cells=Array.from({length:9},(_,i)=>{const b=document.createElement('button');b.type='button';b.disabled=true;b.onclick=()=>request('/move',{cell:i,revision:state.revision});grid.append(b);return b});
function render(){cells.forEach((b,i)=>{const v=state?state.board[i]:'-';b.textContent=v==='-'?'':v;b.className=v.toLowerCase();b.setAttribute('aria-label',`Linha ${Math.floor(i/3)+1}, coluna ${i%3+1}: ${v==='-'?'vazia':v}`);b.disabled=!online||busy||!state||state.training||state.result!=='-'||state.turn!=='X'||v!=='-'});restart.disabled=busy||!online||!!state?.training;train.disabled=busy||!online||!!state?.training;
if(state){
  trainingPanel.hidden=!state.training;grid.style.display=state.training?'none':'';
  progress.value=state.episodes||0;
  trainingCount.textContent=`${Math.floor((state.episodes||0)/2000)}% · ${(state.episodes||0).toLocaleString('pt-BR')} / 200.000 partidas`;
  mode.textContent=state.training?'Modo treinamento':state.trained?'Modo treinado'+(state.saved?' · Aprendizado salvo':' · Não salvo; será perdido ao desligar'):'Modo aleatório';
  train.textContent=state.training?'TREINANDO…':state.trained?'TREINAR NOVAMENTE':'TREINAR';
}
if(state)status.textContent=state.training?'Treinando no ESP32…':state.result==='X'?'Você ganhou!':state.result==='O'?'O ESP ganhou!':state.result==='D'?'Deu empate.':state.turn==='O'?'Vez do ESP…':'Sua vez. Escolha uma casa.'}
async function exchange(path,data){try{const response=await fetch(path,{method:data?'POST':'GET',headers:data?{'Content-Type':'application/x-www-form-urlencoded'}:{},body:data?new URLSearchParams(data):undefined,signal:AbortSignal.timeout(5000),cache:'no-store'});const next=await response.json();if(!next.board)throw Error('Resposta inválida');state=next;online=true;error.textContent=response.ok?'':'A partida mudou. Confira o tabuleiro e tente novamente.'}catch(e){online=false;error.textContent='Sem conexão. Conecte-se ao Wi-Fi do ESP32; tentaremos novamente.'}}
async function request(path,data){
  if(busy)return;
  if(!data){
    if(polling)return;
    polling=exchange(path);
    try{await polling}finally{polling=null;render()}
    return;
  }
  busy=true;render();
  try{
    // Finish an in-flight read before sending a move/reset with its latest revision.
    if(polling)await polling;
    if(online&&state)await exchange(path,{...data,revision:state.revision});
  }finally{busy=false;render()}
}
train.onclick=()=>request('/train',{revision:state.revision});
restart.onclick=()=>request('/reset',{revision:state.revision});
request('/state');setInterval(()=>request('/state'),700);
</script></html>)HTML";

void evaluate() {
  result = '-';
  for (auto &line : lines) {
    if (board[line[0]] != '-' && board[line[0]] == board[line[1]] && board[line[1]] == board[line[2]]) {
      result = board[line[0]]; return;
    }
  }
  for (uint8_t i = 0; i < 9; ++i) if (board[i] == '-') return;
  result = 'D';
}

// Bold 5x7 text, optically centered inside its own column.
void oledLabel(const char* label, int left, int width, int y) {
  int length = strlen(label);
  int x = left + (width - length * 6) / 2;
  display.setCursor(x, y); display.print(label);
  display.setCursor(x + 1, y); display.print(label);
}

void draw() {
  if (!oled) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextWrap(false);
  if(training){
    oledLabel("TREINANDO",0,128,4);
    int frame=(millis()/150)%4;
    for(int i=0;i<4;++i){int x=43+i*14;if(i==frame)display.fillCircle(x,25,4,SSD1306_WHITE);else display.drawCircle(x,25,3,SSD1306_WHITE);}
    char caption[24];snprintf(caption,sizeof(caption),"%lu / 200000",(unsigned long)learner.episodes);
    oledLabel(caption,0,128,37);
    display.drawRect(8,50,112,10,SSD1306_WHITE);
    display.fillRect(10,52,108*learner.episodes/Learner::EPISODES,6,SSD1306_WHITE);
    display.display();return;
  }
  const char* heading = result == 'X' ? "VOCE GANHOU!" : result == 'O' ? "ESP GANHOU!"
    : result == 'D' ? "EMPATE!" : espTurn ? "VEZ DO ESP" : "SUA VEZ";
  oledLabel(heading, 0, 128, 4);

  // Center the 48x48 board in the blue band; leave the yellow band for status.
  const int boardX = 40, boardY = 16;
  for (int n = 1; n < 3; ++n) {
    display.fillRect(boardX + n*16 - 1, boardY, 2, 48, SSD1306_WHITE);
    display.fillRect(boardX, boardY + n*16 - 1, 48, 2, SSD1306_WHITE);
  }
  for (int i = 0; i < 9; ++i) {
    int x = boardX + (i%3)*16 + 7, y = boardY + (i/3)*16 + 7;
    if (board[i] == 'X') {
      for (int offset = 0; offset < 2; ++offset) {
        display.drawLine(x-4+offset,y-4,x+4+offset,y+4,SSD1306_WHITE);
        display.drawLine(x-4+offset,y+4,x+4+offset,y-4,SSD1306_WHITE);
      }
    } else if (board[i] == 'O') {
      display.fillCircle(x,y,5,SSD1306_WHITE);
      display.fillCircle(x,y,3,SSD1306_BLACK);
    }
  }
  oledLabel("VOCE", 0, 36, 24);
  oledLabel("ESP", 92, 36, 24);
  display.setCursor(95,17);display.print(trained?"TREINO":"ACASO");
  display.setTextSize(2);
  display.setCursor(12,37); display.print("X");
  display.setCursor(104,37); display.print("O");
  // Underline the active player, or the winner when the round ends.
  if (result == 'X' || (result == '-' && !espTurn)) display.fillRect(8,57,20,2,SSD1306_WHITE);
  if (result == 'O' || (result == '-' && espTurn)) display.fillRect(100,57,20,2,SSD1306_WHITE);
  display.display();
}

void startGame() {
  for (int i = 0; i < 9; ++i) board[i] = '-';
  result = '-';
  espTurn = nextEspStarts;
  nextEspStarts = !nextEspStarts;
  espAt = millis();
  ++revision;
  draw();
}

void sendState(int code = 200) {
  server.sendHeader("Cache-Control", "no-store");
  String body = "{\"board\":\"" + String(board) + "\",\"result\":\"" + String(result);
  body += "\",\"turn\":\"" + String(espTurn ? 'O' : 'X') + "\",\"revision\":" + String(revision);
  body += ",\"training\":" + String(training?"true":"false") + ",\"trained\":" + String(trained?"true":"false");
  body += ",\"saved\":" + String(modelSaved?"true":"false") + ",\"episodes\":" + String(learner.episodes) + "}";
  server.send(code,"application/json",body);
}

bool currentRequest() {
  return server.hasArg("revision") && server.arg("revision") == String(revision);
}

void setup() {
  Serial.begin(115200);
  loadModel();
  Wire.begin(21,22);
  for (uint8_t address : {0x3C,0x3D}) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      oled = display.begin(SSD1306_SWITCHCAPVCC,address,true,false);
      if (oled) break;
    }
  }
  if (!oled) Serial.println("OLED indisponivel; jogo web continua ativo.");
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID,AP_PASSWORD)) {
    Serial.println("Falha ao iniciar Wi-Fi. Reinicie a placa.");
    if (oled) {
      display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,0); display.println("FALHA NO WI-FI"); display.println("Reinicie a placa."); display.display();
    }
    return;
  }
  Serial.printf("Wi-Fi: %s\nSenha: %s\n",AP_SSID,AP_PASSWORD);
  Serial.print("Abra http://"); Serial.println(WiFi.softAPIP());
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",PAGE);});
  server.on("/state",HTTP_GET,[]{sendState();});
  server.on("/move",HTTP_POST,[]{
    String cell = server.arg("cell");
    if (cell.length()!=1 || cell[0]<'0' || cell[0]>'8') {sendState(400);return;}
    int i=cell[0]-'0';
    if (training || !currentRequest() || result!='-' || espTurn || board[i]!='-') {sendState(409);return;}
    board[i]='X'; evaluate(); ++revision;
    espTurn=result=='-'; espAt=millis(); draw(); sendState();
  });
  server.on("/reset",HTTP_POST,[]{
    if (training || !currentRequest()) {sendState(409);return;}
    startGame(); sendState();
  });
  server.on("/train",HTTP_POST,[]{
    if(training || !currentRequest()){sendState(409);return;}
    training=true;trained=false;modelSaved=false;
    learner.reset(esp_random());++revision;
    draw();sendState();
  });
  server.onNotFound([]{server.send(404,"text/plain","Nao encontrado");});
  startGame();
  server.begin();
}

void loop() {
  server.handleClient();
  if(training){
    uint32_t batchStart=micros();
    do {learner.episode();}while(learner.episodes<Learner::EPISODES && micros()-batchStart<4000);
    if(learner.episodes>=Learner::EPISODES){
      modelSaved=saveModel();trained=true;training=false;++revision;
      // Resume the existing game; training does not change starting order.
      espAt=millis();draw();
    }else if(millis()-lastTrainingDraw>=100){lastTrainingDraw=millis();draw();}
    delay(1);return;
  }
  if (espTurn && millis()-espAt>=450) {
    uint8_t freeCells[9], count=0;
    for (uint8_t i=0;i<9;++i) if (board[i]=='-') freeCells[count++]=i;
    if (count) {
      // Rejection sampling avoids modulo bias; no tactical logic is applied.
      uint32_t value, threshold = (uint32_t(0)-uint32_t(count)) % count;
      do {value=esp_random();} while (value<threshold);
      int action=trained?learner.choose(board,'O'):freeCells[value%count];
      if(action>=0)board[action]='O';
    }
    espTurn=false; evaluate(); ++revision; draw();
  }
  delay(2);
}
