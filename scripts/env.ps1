# Közös beállítások a build/upload szkriptekhez. Minden verzió itt van rögzítve.
$ErrorActionPreference = 'Stop'
$Root      = Split-Path -Parent $PSScriptRoot
$Cli       = Join-Path $Root 'tools\arduino-cli\arduino-cli.exe'
$CliConfig = Join-Path $Root 'tools\arduino-cli.yaml'
$Python    = Join-Path $Root 'tools\venv\Scripts\python.exe'
$SketchName = if ($env:PK_SKETCH) { $env:PK_SKETCH } else { 'poolkeeper_ui' }   # régi M1: poolkeeper_panel
$Sketch    = Join-Path $Root "firmware\$SketchName"
$Libs      = Join-Path $Root 'firmware\libraries'
$BuildDir  = Join-Path $Root "build\$SketchName"

$CoreVersion = '3.3.8'   # esp32:esp32 — a gyári RotaryScreen firmware (2026-09-16) is ezzel készült
# CrowPanel 1.46": ESP32-S3R8, 16 MB flash (DIO, 80 MHz a gyári képpel egyezően), OPI PSRAM,
# USB-Serial/JTAG (USB CDC On Boot), egyedi partíciós tábla (partitions.csv = elecrow_s3, 10 MB APP)
$Fqbn = 'esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=dio,FlashSize=16M,PSRAM=opi,PartitionScheme=custom,CPUFreq=240,LoopCore=1,EventsCore=1,DebugLevel=none'

function Find-PanelPort {
    # Espressif natív USB (VID 303A) soros port keresése
    $dev = Get-PnpDevice -PresentOnly -Class Ports -ErrorAction SilentlyContinue |
        Where-Object { $_.InstanceId -match 'VID_303A' } | Select-Object -First 1
    if ($dev -and $dev.FriendlyName -match '\((COM\d+)\)') { return $Matches[1] }
    return $null
}
