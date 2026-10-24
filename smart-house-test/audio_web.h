#pragma once
const char AUDIO_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Casa / Áudios</title><style>body{font:16px system-ui;background:#101113;color:#eeeae4;max-width:720px;margin:32px auto;padding:16px}a{color:#dbbf8b}button,input{font:inherit;max-width:100%;margin:8px 0}button{padding:8px 14px;cursor:pointer}li{padding:6px 0}#result{white-space:pre-line}button:disabled{opacity:.5}</style>
<h1>Voz da casa</h1><a href="/">Voltar às luzes</a><p>Envie um WAV por frase: PCM mono, 16 bits, 16 kHz. Até 128.000 bytes por arquivo e 1.200.000 bytes no conjunto. Use os nomes abaixo.</p>
<p id="storage" role="status"></p><button id="prepare" hidden>Preparar memória de áudio</button>
<form id="upload"><label>Gravações <input id="files" type="file" accept=".wav" multiple required></label><br><button id="send">Enviar arquivos</button></form>
<p id="result" role="status"></p><ul id="list"></ul><script>
const $=s=>document.querySelector(s);let busy=false;
async function refresh(){const r=await fetch('/status',{cache:'no-store'});if(!r.ok)throw Error('Não foi possível consultar a casa.');const a=(await r.json()).audio;
$('#storage').textContent=!a.storage_ready?'Memória ainda não preparada.':!a.ready?'Áudio indisponível. Confira o dispositivo.':'Memória pronta. Ajuste o volume na tela de luzes.';
$('#prepare').hidden=a.storage_ready;$('#send').disabled=busy||!a.storage_ready;
$('#list').replaceChildren(...a.files.map(f=>{const li=document.createElement('li');li.textContent=f.name+' — '+(f.available?'disponível':'ausente ou inválido');return li}));}
$('#prepare').onclick=async()=>{if(!confirm('Preparar a partição de dados apaga todos os arquivos nela. Continuar?'))return;$('#prepare').disabled=true;try{const r=await fetch('/audio/initialize',{method:'POST',body:new URLSearchParams({confirm:'ERASE_AUDIO_DATA'})});if(!r.ok)throw Error(await r.text());await refresh()}catch(e){$('#result').textContent=e.message}finally{$('#prepare').disabled=false}};
$('#upload').onsubmit=async e=>{e.preventDefault();if(busy)return;busy=true;$('#send').disabled=true;$('#files').disabled=true;$('#result').textContent='';try{for(const file of $('#files').files){const data=new FormData();data.append('audio',file);const r=await fetch('/audio/upload',{method:'POST',body:data});if(!r.ok)throw Error(file.name+': '+await r.text());$('#result').textContent+=file.name+': enviado.\n'}}catch(e){$('#result').textContent+=e.message}finally{busy=false;$('#files').disabled=false;await refresh().catch(e=>$('#result').textContent=e.message)}};
refresh().catch(e=>$('#result').textContent=e.message);
</script></html>)HTML";

File audioUploadFile;
int audioUploadId = -1, audioUploadCode = 400;
bool audioUploadActive = false, audioUploadDone = false;
size_t audioUploadBytes = 0, audioUploadBudget = 0;
String audioUploadMessage;
bool audioOriginAllowed() {
  const String origin = otaServer.header("Origin");
  return !origin.length() || origin == String("http://") + otaServer.header("Host");
}
void audioUploadCleanup() {
  audioUploadFile.close();
  if (audioUploadActive) { LittleFS.remove("/upload.tmp"); audioResume(); }
  audioUploadActive = false;
}
void audioWebSetup() {
  otaServer.on("/audio", HTTP_GET, []() {
    if (!otaAuthenticate()) return;
    otaServer.sendHeader("Cache-Control", "no-store");
    otaServer.send_P(200, "text/html; charset=utf-8", AUDIO_PAGE);
  });
  otaServer.on("/audio/initialize", HTTP_POST, []() {
    if (!otaAuthenticate()) return;
    if (!audioOriginAllowed()) { otaServer.send(403,"text/plain","Origem invalida"); return; }
    if (audioMounted || otaServer.arg("confirm") != "ERASE_AUDIO_DATA") { otaServer.send(400,"text/plain","Preparacao nao permitida"); return; }
    audioPause();
    audioMounted = LittleFS.format() && LittleFS.begin(false);
    audioRefresh(); audioResume();
    otaServer.send(audioMounted ? 200 : 500,"text/plain", audioMounted ? "Memoria preparada" : "Falha ao preparar memoria");
  });
  otaServer.on("/audio/upload", HTTP_POST, []() {
    if (!otaAuthenticate()) { audioUploadCleanup(); return; }
    if (!audioOriginAllowed()) { audioUploadCleanup(); otaServer.send(403,"text/plain","Origem invalida"); return; }
    // Reset response state even for a malformed request with no multipart file.
    bool done = audioUploadDone;
    int code = audioUploadCode;
    String message = audioUploadMessage;
    audioUploadCleanup(); audioUploadDone = false; audioUploadCode = 400; audioUploadMessage = "Arquivo WAV ausente";
    otaServer.send(done ? 200 : code, "text/plain; charset=utf-8", done ? "Audio salvo" : message);
  }, []() {
    HTTPUpload& upload = otaServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      audioUploadCleanup(); audioUploadDone = false; audioUploadCode = 400;
      audioUploadMessage = "Use um nome de frase permitido e WAV PCM mono, 16 bits, 16 kHz";
      if (!otaServer.authenticate(OTA_USER, OTA_PASSWORD) || !audioOriginAllowed()) return;
      audioUploadId = audioId(upload.filename);
      if (!audioMounted) { audioUploadCode = 409; audioUploadMessage = "Prepare a memoria primeiro"; return; }
      if (audioUploadId < 0 || upload.name != "audio") return;
      audioPause(); audioUploadActive = true; audioUploadBytes = 0;
      size_t other = 0;
      for (size_t i = 0; i < AUDIO_COUNT; ++i) if (int(i) != audioUploadId) other += audioSizes[i];
      audioUploadBudget = other < AUDIO_LIMIT ? std::min(AUDIO_FILE_LIMIT, AUDIO_LIMIT-other) : 0;
      audioUploadFile = LittleFS.open("/upload.tmp", "w");
      if (!audioUploadFile) { audioUploadCode = 507; audioUploadMessage = "Sem espaco para envio"; }
    } else if (upload.status == UPLOAD_FILE_WRITE && audioUploadActive && audioUploadFile) {
      if (upload.currentSize > audioUploadBudget - audioUploadBytes) {
        audioUploadFile.close(); audioUploadCode = 413; audioUploadMessage = "Limite de tamanho excedido";
      } else if (audioUploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
        audioUploadFile.close(); audioUploadCode = 507; audioUploadMessage = "Falha ao gravar arquivo";
      } else audioUploadBytes += upload.currentSize;
    } else if (upload.status == UPLOAD_FILE_END && audioUploadActive) {
      bool complete = bool(audioUploadFile) && audioUploadBytes == upload.totalSize;
      audioUploadFile.close();
      File candidate = LittleFS.open("/upload.tmp", "r"); WavInfo info;
      bool valid = complete && candidate && audioWav(candidate, info);
      candidate.close();
      if (valid) {
        // LittleFS rename atomically replaces the destination; keep the old clip on error.
        audioUploadDone = LittleFS.rename("/upload.tmp", audioPath(audioUploadId));
        if (!audioUploadDone) { audioUploadCode = 500; audioUploadMessage = "Falha ao substituir audio"; }
      }
      audioRefresh(); audioUploadCleanup();
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      audioUploadDone = false; audioUploadCode = 400; audioUploadMessage = "Envio interrompido"; audioUploadCleanup();
    }
  });
}
