param([switch]$Demo, [switch]$Windowed)
$ErrorActionPreference = 'Stop'
$taskData = Join-Path $env:LOCALAPPDATA 'CtrlFeira'
New-Item -ItemType Directory -Force $taskData | Out-Null
$taskPython = $null
try { $taskPython = (& py -3.12 -c 'import sys; assert sys.maxsize>2**32; print(sys.executable)' 2>$null) } catch {}
if (-not $taskPython) {
    $taskInstalled = $false
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        & winget install --id Python.Python.3.12 --exact --architecture x64 --silent --accept-package-agreements --accept-source-agreements
        $taskInstalled = $LASTEXITCODE -eq 0
    }
    if (-not $taskInstalled) {
        $taskInstaller = Join-Path $taskData 'python-3.12.10-amd64.exe'
        Invoke-WebRequest 'https://www.python.org/ftp/python/3.12.10/python-3.12.10-amd64.exe' -OutFile $taskInstaller
        $taskSignature = Get-AuthenticodeSignature -LiteralPath $taskInstaller
        if ($taskSignature.Status -ne 'Valid' -or $taskSignature.SignerCertificate.Subject -notmatch 'Python Software Foundation') {
            throw 'Assinatura do instalador Python invalida; arquivo nao executado.'
        }
        $taskInstallProcess = Start-Process -FilePath $taskInstaller -ArgumentList '/quiet InstallAllUsers=0 PrependPath=0 Include_launcher=1 InstallLauncherAllUsers=0 Include_test=0' -WindowStyle Hidden -Wait -PassThru
        if ($taskInstallProcess.ExitCode -notin @(0,3010)) { throw 'Falha ao instalar Python 3.12.' }
    }
    $taskPython = Join-Path $env:LOCALAPPDATA 'Programs\Python\Python312\python.exe'
    if (-not (Test-Path $taskPython)) { $taskPython = (& py -3.12 -c 'import sys; print(sys.executable)') }
}
$taskVenv = Join-Path $taskData 'venv'
if (-not (Test-Path "$taskVenv\Scripts\python.exe")) {
    & $taskPython -m venv $taskVenv
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao criar ambiente Python.' }
}
$taskRunner = Join-Path $taskVenv 'Scripts\python.exe'
$taskRequirements = Join-Path $PSScriptRoot 'fair\requirements.txt'
$taskHash = (Get-FileHash $taskRequirements).Hash
$taskStamp = Join-Path $taskData 'dependencies.sha256'
if (-not (Test-Path $taskStamp) -or (Get-Content $taskStamp -Raw).Trim() -ne $taskHash) {
    & $taskRunner -m pip install -r $taskRequirements
    if ($LASTEXITCODE -ne 0) { throw 'Falha nas dependências. Confira internet e execute novamente.' }
    Set-Content $taskStamp $taskHash
}
$taskModel = Join-Path $taskData 'gesture_recognizer.task'
if (-not (Test-Path $taskModel)) {
    Invoke-WebRequest 'https://storage.googleapis.com/mediapipe-models/gesture_recognizer/gesture_recognizer/float16/1/gesture_recognizer.task' -OutFile "$taskModel.part"
    Move-Item -LiteralPath "$taskModel.part" -Destination $taskModel -Force
}
# PnP status is useful even when a missing driver prevents creation of a COM port.
Get-CimInstance Win32_PnPEntity -Filter 'ConfigManagerErrorCode = 28' -ErrorAction SilentlyContinue |
    Where-Object { $_.PNPDeviceID -like 'USB*' } |
    ForEach-Object {
        $taskDriver = switch -Regex ($_.PNPDeviceID) {
            'VID_10C4' { 'Silicon Labs CP210x: https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers'; break }
            'VID_1A86' { 'WCH CH340/CH341: https://www.wch-ic.com/downloads/CH341SER_EXE.html'; break }
            'VID_0403' { 'FTDI VCP: https://ftdichip.com/drivers/vcp-drivers/'; break }
            default { 'Identifique o chip USB no Gerenciador de Dispositivos; consulte GUIA-FEIRA.md.' }
        }
        Write-Warning "USB sem driver: $($_.Name) $($_.PNPDeviceID). $taskDriver"
    }
$taskArguments = @((Join-Path $PSScriptRoot 'fair\panel.py'))
if ($Demo) { $taskArguments += '--demo' }
if ($Windowed) { $taskArguments += '--windowed' }
& $taskRunner @taskArguments
if ($LASTEXITCODE -ne 0) { throw "Painel terminou com erro. Consulte $taskData\panel.log" }
