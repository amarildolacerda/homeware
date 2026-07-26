param()

$ErrorActionPreference = "Stop"

function Get-Pio {
    $paths = @(
        "pio",
        "platformio",
        "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python3*\Scripts\pio.exe"
    )
    foreach ($p in $paths) {
        $cmd = Get-Command $p -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    return $null
}

$pio = Get-Pio
if (-not $pio) {
    Write-Error "PlatformIO CLI not found. Tente: python -m pip install platformio"
    exit 1
}

Write-Host "Building Hub (ESP8266)..." -ForegroundColor Cyan
& $pio run -e hub_8266

Write-Host "Build complete. Firmware in .pio/build/hub_8266/firmware.bin" -ForegroundColor Green
