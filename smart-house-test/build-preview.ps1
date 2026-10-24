param([string]$Clang='clang++')
$ErrorActionPreference='Stop'
$taskSource=Join-Path $PSScriptRoot 'preview\engine.cpp'
$taskOutput=Join-Path $PSScriptRoot 'effects.wasm'
& $Clang --target=wasm32 -O2 -std=c++11 -nostdlib -fno-exceptions -fno-rtti '-Wl,--no-entry' '-Wl,--export-all' $taskSource -o $taskOutput
if($LASTEXITCODE -ne 0){throw 'Falha ao gerar effects.wasm. Use clang++ com wasm-ld instalado.'}
& node (Join-Path $PSScriptRoot 'check-effects.cjs')
if($LASTEXITCODE -ne 0){throw 'Falha ao validar efeitos.'}
