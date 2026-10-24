param([string]$Clang='clang++', [string]$SysrootInclude='')
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskBuild=Join-Path $taskRoot '.build'
New-Item -ItemType Directory -Force -Path $taskBuild | Out-Null
$taskArgs=@('--target=wasm32','-O2','-std=c++11','-nostdlib','-fno-exceptions','-fno-rtti','-Wl,--no-entry','-Wl,--export-all')
if($SysrootInclude){$taskArgs+=@('-isystem',$SysrootInclude)}
$taskOutput=Join-Path $taskBuild 'wav-test.wasm'
& $Clang @taskArgs (Join-Path $PSScriptRoot 'preview/wav_test.cpp') -o $taskOutput
if($LASTEXITCODE -ne 0){throw 'Falha ao compilar o parser WAV para testes. Use clang++ com wasm-ld e headers de wasm32.'}
& node (Join-Path $PSScriptRoot 'check-audio.cjs') $taskOutput
if($LASTEXITCODE -ne 0){throw 'Falha nos testes de audio. Execute a previa local antes deste script.'}
