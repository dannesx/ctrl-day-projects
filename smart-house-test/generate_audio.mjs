import { ElevenLabsClient } from '@elevenlabs/elevenlabs-js';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// Run from the repository root with node --env-file=.env.local.
const directory = path.dirname(fileURLToPath(import.meta.url));
const destination = path.join(directory, 'audio-pt-br');
const voiceId = 'EXAVITQu4vr4xnSDxMaL';
const modelId = 'eleven_v4';
const phrases = {
  inicio: 'Casa inteligente pronta.',
  tudo_on: 'Todas as luzes acesas.',
  tudo_off: 'Todas as luzes apagadas.',
  superior_on: 'Luzes do andar superior acesas.',
  superior_off: 'Luzes do andar superior apagadas.',
  central_on: 'Luzes do andar central acesas.',
  central_off: 'Luzes do andar central apagadas.',
  terreo_on: 'Luzes do térreo acesas.',
  terreo_off: 'Luzes do térreo apagadas.',
  manual: 'Modo cores fixas ativado.',
  ciclo: 'Ciclo de cores ativado.',
  neve: 'Modo neve ativado.',
  halloween: 'Modo Halloween ativado.',
  natal: 'Modo Natal ativado.',
  matrix: 'Modo Matrix ativado.',
};

function wav(pcm) {
  if (!pcm.length || pcm.length % 2) throw Error('Invalid PCM response');
  const header = Buffer.alloc(44);
  header.write('RIFF'); header.writeUInt32LE(36 + pcm.length, 4);
  header.write('WAVEfmt ', 8); header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20); header.writeUInt16LE(1, 22);
  header.writeUInt32LE(16000, 24); header.writeUInt32LE(32000, 28);
  header.writeUInt16LE(2, 32); header.writeUInt16LE(16, 34);
  header.write('data', 36); header.writeUInt32LE(pcm.length, 40);
  return Buffer.concat([header, pcm]);
}

async function main() {
  if (!process.env.ELEVENLABS_API_KEY) throw Error('Load .env.local using --env-file');
  const client = new ElevenLabsClient();
  await mkdir(destination, { recursive: true });
  const manifest = { voiceId, voice: 'Sarah - Mature, Reassuring, Confident', modelId, language: 'pt-BR', files: [] };
  let total = 0;
  for (const [name, text] of Object.entries(phrases)) {
    const filename = name + '.wav';
    const target = path.join(destination, filename);
    let data;
    try { data = await readFile(target); }
    catch (error) { if (error.code !== 'ENOENT') throw error; }
    if (!data) {
      const stream = await client.textToSpeech.convert(voiceId, {
        text, modelId, languageCode: 'pt', outputFormat: 'pcm_16000',
        voiceSettings: { stability: 0.7, similarityBoost: 0.75 },
      });
      const chunks = [];
      for await (const chunk of stream) chunks.push(Buffer.from(chunk));
      data = wav(Buffer.concat(chunks));
      // Keep long originals for editing rather than cutting speech or regenerating it.
      if (data.length > 128000) {
        await writeFile(path.join(destination, name + '.too-long.wav'), data, { flag: 'wx' });
        throw Error(filename + ' exceeds the ESP32 limit; original saved as .too-long.wav');
      }
      await writeFile(target, data, { flag: 'wx' });
    }
    if (data.length < 46 || data.length > 128000 || data.toString('ascii', 0, 4) !== 'RIFF' ||
        data.toString('ascii', 8, 16) !== 'WAVEfmt ' || data.readUInt16LE(20) !== 1 ||
        data.readUInt16LE(22) !== 1 || data.readUInt32LE(24) !== 16000 || data.readUInt16LE(34) !== 16 ||
        data.readUInt32LE(40) !== data.length - 44) throw Error('Invalid WAV: ' + filename);
    total += data.length;
    if (total > 1200000) throw Error('Combined files exceed ESP32 storage limit');
    const seconds = (data.length - 44) / 32000;
    manifest.files.push({ filename, text, bytes: data.length, seconds });
    console.log(filename + ': ' + seconds.toFixed(2) + 's');
  }
  manifest.totalBytes = total;
  await writeFile(path.join(destination, 'manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
  console.log('Ready: ' + manifest.files.length + ' WAV files, ' + total + ' bytes.');
}

main().catch(error => {
  // SDK error objects may contain request headers; print only a sanitized summary.
  console.error(error.statusCode ? 'ElevenLabs HTTP ' + error.statusCode + ': ' + (error.body?.detail?.message || 'request failed') : error.message);
  process.exitCode = 1;
});
