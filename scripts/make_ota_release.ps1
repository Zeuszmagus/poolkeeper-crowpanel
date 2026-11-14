# Távoli frissítés (OTA) kiadás készítése.
#   powershell -ExecutionPolicy Bypass -File scripts\make_ota_release.ps1 -BaseUrl https://<tárhely>/poolkeeper [-Devices PK-71D040,PK-XXXXXX]
# Eredmény az ota\ mappában: poolkeeper_ui-<build>.bin és manifest.json — ezt a kettőt kell feltölteni a tárhelyre.
# Előtte az src\app_config.h-ban a PK_FW_BUILD értékét növelni kell (a panel csak nagyobb buildet telepít).
param([Parameter(Mandatory = $true)][string]$BaseUrl, [string[]]$Devices = @('*'), [switch]$NoBuild)
. "$PSScriptRoot\env.ps1"
if (-not $NoBuild) { & "$PSScriptRoot\build.ps1" }
$cfg = Get-Content (Join-Path $Sketch 'src\app_config.h') -Raw -Encoding UTF8
$build = [int]([regex]::Match($cfg, '#define PK_FW_BUILD (\d+)').Groups[1].Value)
$version = [regex]::Match($cfg, '#define PK_FW_VERSION "([^"]+)"').Groups[1].Value
$outDir = Join-Path $Root 'ota'
New-Item -ItemType Directory -Force $outDir | Out-Null
$binName = "poolkeeper_ui-$build.bin"
$bin = Join-Path $outDir $binName
Copy-Item (Join-Path $BuildDir "$SketchName.ino.bin") $bin -Force
$sha = (Get-FileHash $bin -Algorithm SHA256).Hash.ToLower()
$manifest = [ordered]@{
  build   = $build
  version = $version
  url     = ($BaseUrl.TrimEnd('/') + '/' + $binName)
  sha256  = $sha
  devices = $Devices
}
$json = $manifest | ConvertTo-Json -Compress
[System.IO.File]::WriteAllText((Join-Path $outDir 'manifest.json'), $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Kész: $bin"
Write-Host "Manifest: $json"
