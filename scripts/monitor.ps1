# Soros monitor (115200): powershell -ExecutionPolicy Bypass -File scripts\monitor.ps1 [-Port COM10]
# Kilépés: Ctrl+C. Gépeld be: help
param([string]$Port)
. "$PSScriptRoot\env.ps1"
if (-not $Port) { $Port = Find-PanelPort }
if (-not $Port) { throw 'Nem található a panel soros portja.' }
& $Cli --config-file $CliConfig monitor --port $Port --config baudrate=115200
