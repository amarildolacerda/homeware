param (
    [string]$p = "COM3",
    [string]$o
)

# Flash ESP8266 Gateway
# .\flash.ps1                     (serial, COM3)
# .\flash.ps1 -p COM4             (serial, COM4)
# .\flash.ps1 -o 192.168.1.100    (OTA)

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

if ($o) {
    Write-Host "Building and uploading OTA to $o..." -ForegroundColor Cyan
    & $pio run -e esp8266_gateway_ota --target upload --upload-port "$o"
} else {
    Write-Host "Building and flashing on $p..." -ForegroundColor Cyan
    & $pio run -e esp8266_gateway --target upload --upload-port "$p"
}

Write-Host "Done." -ForegroundColor Green
