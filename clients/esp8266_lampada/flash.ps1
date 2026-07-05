param (
    [string]$IP = "192.168.1.100"
)

# Flash ESP8266 OTA from Windows PowerShell
# Requires: Python + espota.py (from ESP8266 Arduino core) or platformio

$ErrorActionPreference = "Stop"

function Test-Command {
    param($Command)
    return (Get-Command $Command -ErrorAction SilentlyContinue) -ne $null
}

if (Test-Command pio) {
    Write-Host "Using PlatformIO..." -ForegroundColor Cyan
    pio run -t upload --upload-port "$IP"
    exit
}

if (Test-Command platformio) {
    Write-Host "Using PlatformIO..." -ForegroundColor Cyan
    platformio run -t upload --upload-port "$IP"
    exit
}

$firmware = ".pio\build\esp8266\firmware.bin"
if (-not (Test-Path $firmware)) {
    if (Test-Command pio) {
        pio run
    } elseif (Test-Command platformio) {
        platformio run
    } else {
        Write-Error "Firmware not found and PlatformIO not available"
        exit 1
    }
}

Write-Host "Uploading OTA to $IP..." -ForegroundColor Cyan
python -m espota -i $IP -p 8266 -f $firmware
