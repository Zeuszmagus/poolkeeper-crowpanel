# Firmware fordítása: powershell -ExecutionPolicy Bypass -File scripts\build.ps1
. "$PSScriptRoot\env.ps1"
& $Cli --config-file $CliConfig compile `
    --fqbn $Fqbn `
    --libraries $Libs `
    --build-path $BuildDir `
    --warnings default `
    $Sketch
if ($LASTEXITCODE -ne 0) { throw "Fordítási hiba ($LASTEXITCODE)" }
Get-ChildItem $BuildDir -Filter "$SketchName.ino*.bin" | Select-Object Name, Length | Format-Table -AutoSize
