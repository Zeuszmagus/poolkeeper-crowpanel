# Gyári állapot visszaállítása.
#   -Backup : a saját panelről 2026-11-12-én készült teljes 16 MB-os mentés (alapértelmezett)
#   -Vendor : az Elecrow 2026-09-16-os gyári merged firmware
# Használat: powershell -ExecutionPolicy Bypass -File scripts\restore_factory.ps1 [-Vendor] [-Port COM10]
param([switch]$Vendor, [string]$Port)
. "$PSScriptRoot\env.ps1"
if (-not $Port) { $Port = Find-PanelPort }
if (-not $Port) { throw 'Nem található a panel soros portja.' }
if ($Vendor) {
    $img = Join-Path $Root 'vendor\elecrow-1.46\factory_firmware\firmware\RotaryScreen_1_46_ESP32S3_16MB_merged.bin'
    $expected = 'a19d0791915d7bf4e098c4388c402b202f067aa18c66ec787de13995972835cd'
} else {
    $img = Join-Path $Root 'backup\panel_44bd8d71d040_flash_16MB_20261112.bin'
    $expected = 'bced47d6e58106997aec90a282701dae5bc82887f206f3a5163d111556c23968'
}
$hash = (Get-FileHash $img -Algorithm SHA256).Hash.ToLower()
if ($hash -ne $expected) { throw "SHA-256 eltérés: $img" }
Write-Host "Visszaállítás: $img -> $Port"
& $Python -m esptool --chip esp32s3 --port $Port --baud 921600 write-flash -z --flash-mode dio --flash-freq 80m --flash-size 16MB 0x0 $img
if ($LASTEXITCODE -ne 0) { throw 'Írási hiba — próbáld BOOT+RESET letöltési módban, vagy --baud 115200-zal.' }
