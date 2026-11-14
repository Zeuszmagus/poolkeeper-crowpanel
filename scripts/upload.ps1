# Feltöltés USB-n: powershell -ExecutionPolicy Bypass -File scripts\upload.ps1 [-Port COM10] [-NoBuild]
# Ha "Connecting..." után elakad: tartsd lenyomva a BOOT-ot, nyomd meg röviden a RESET-et, engedd el a BOOT-ot.
param([string]$Port, [switch]$NoBuild)
. "$PSScriptRoot\env.ps1"
if (-not $NoBuild) { & "$PSScriptRoot\build.ps1" }
if (-not $Port) { $Port = Find-PanelPort }
if (-not $Port) { throw 'Nem található a panel soros portja (VID 303A). Ellenőrizd az USB-adatkábelt.' }
Write-Host "Feltöltés: $Port"
& $Cli --config-file $CliConfig upload --fqbn $Fqbn --port $Port --input-dir $BuildDir $Sketch
if ($LASTEXITCODE -ne 0) { throw "Feltöltési hiba ($LASTEXITCODE)" }
