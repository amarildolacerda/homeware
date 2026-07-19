param (
    [string]$p = "COM3",
    [string]$o,
    [string]$e
)

<#
.SYNOPSIS
    Flash ESP8266 Lampada
.EXAMPLE
    .\flash.ps1                     # serial, COM3, env esp8266
    .\flash.ps1 -p COM4             # serial, COM4
    .\flash.ps1 -o 192.168.1.100    # OTA, env esp8266_ota
    .\flash.ps1 -o 192.168.1.100 -e esp8266   # override env
#>

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
    Write-Error "PlatformIO CLI not found. Use: python -m pip install platformio"
    exit 1
}

if ($o) {
    $envName = if ($e) { $e } else { "esp8266_ota" }
    Write-Host "Building + OTA upload to $o (env: $envName)..." -ForegroundColor Cyan
    & $pio run -e $envName --target upload --upload-port "$o"
    if ($LASTEXITCODE -ne 0) { Write-Error "OTA upload failed"; exit 1 }
} else {
    $envName = if ($e) { $e } else { "esp8266" }
    Write-Host "Building + serial flash on $p (env: $envName)..." -ForegroundColor Cyan
    & $pio run -e $envName --target upload --upload-port "$p"
    if ($LASTEXITCODE -ne 0) { Write-Error "Flash failed"; exit 1 }
}

Write-Host "Done." -ForegroundColor Green
