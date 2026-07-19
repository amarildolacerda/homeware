param (
    [string]$p = "COM3",
    [string]$o,
    [string]$e
)

<#
.SYNOPSIS
    Flash ESP32/ESP8266 Gateway
.EXAMPLE
    .\flash.ps1                     # serial, COM3, env padrao
    .\flash.ps1 -p COM4             # serial, COM4
    .\flash.ps1 -o 192.168.1.14     # OTA
    .\flash.ps1 -o 192.168.1.14 -e esp32_gateway_ota   # OTA ESP32
    .\flash.ps1 -p COM3 -e esp32_gateway                # serial ESP32
#>

$ErrorActionPreference = "Stop"

function Get-Pio {
    $paths = @(
        "pio",
        "platformio",
        "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python3*\Scripts\pio.exe"
    )
    foreach ($pp in $paths) {
        $cmd = Get-Command $pp -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    return $null
}

$pio = Get-Pio
if (-not $pio) {
    Write-Error "PlatformIO CLI not found. Tente: python -m pip install platformio"
    exit 1
}

if ($o) {
    $envName = if ($e) { $e } else { "esp32_gateway_ota" }
    Write-Host "Building + OTA upload to $o (env: $envName)..." -ForegroundColor Cyan
    & $pio run -e $envName --target upload --upload-port "$o"
    if ($LASTEXITCODE -ne 0) { Write-Error "OTA upload failed"; exit 1 }
} else {
    $envName = if ($e) { $e } else { "esp8266_gateway" }
    Write-Host "Building + serial flash on $p (env: $envName)..." -ForegroundColor Cyan
    & $pio run -e $envName --target upload --upload-port "$p"
    if ($LASTEXITCODE -ne 0) { Write-Error "Flash failed"; exit 1 }
}

Write-Host "Done." -ForegroundColor Green
