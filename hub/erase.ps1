param (
    [string]$p = "COM3",
    [string]$e
)

<#
.SYNOPSIS
    Erase ESP8266 flash via serial
.EXAMPLE
    .\erase.ps1                     # serial, COM3, env esp8266
    .\erase.ps1 -p COM4             # serial, COM4
    .\erase.ps1 -p COM4 -e esp8266_ota   # override env
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

$envName = if ($e) { $e } else { "hub_8266" }
Write-Host "Erasing via PlatformIO on $p (env: $envName)..." -ForegroundColor Cyan
& $pio run -e $envName -t erase --upload-port "$p"
if ($LASTEXITCODE -ne 0) { Write-Error "Erase failed"; exit 1 }

Write-Host "Done." -ForegroundColor Green
