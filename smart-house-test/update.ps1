param(
  [Parameter(Mandatory = $true)]
  [ValidatePattern('^[a-zA-Z0-9.-]+(:[0-9]+)?$')]
  [string]$Address,
  [string]$Firmware
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildPath = Join-Path $projectRoot '.build/smart-house-test'
if (-not $Firmware) {
  & arduino-cli compile --fqbn esp32:esp32:esp32 --build-path $buildPath $PSScriptRoot
  if ($LASTEXITCODE -ne 0) { throw 'A compilacao falhou. Nada foi enviado.' }
  $Firmware = Join-Path $buildPath 'smart-house-test.ino.bin'
}
$firmwarePath = (Resolve-Path -LiteralPath $Firmware).Path
$localConfig = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'wifi_config.h')
$userMatch = [regex]::Match($localConfig, 'OTA_USER\s*=\s*"([^"\\]+)"')
$passwordMatch = [regex]::Match($localConfig, 'OTA_PASSWORD\s*=\s*"([^"\\]+)"')
if (-not $userMatch.Success -or -not $passwordMatch.Success) {
  throw 'Configure OTA_USER e OTA_PASSWORD em wifi_config.h (texto sem aspas ou barras).'
}
# Envia a credencial pela entrada padrao do curl, sem inclui-la na linha de comando.
$curlConfig = 'user = "' + $userMatch.Groups[1].Value + ':' + $passwordMatch.Groups[1].Value + '"'
$curlConfig | & curl.exe --config - --noproxy '*' --http1.1 --header 'Expect:' --fail --show-error --silent --max-time 180 --form "firmware=@$firmwarePath" "http://$Address/update"
if ($LASTEXITCODE -ne 0) { throw 'O envio OTA falhou. Confira o endereco, a rede e a senha.' }
