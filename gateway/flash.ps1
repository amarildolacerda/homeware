param (
    [string]$Port = "COM3",
    [switch]$Ota,
    [string]$Ip
)

# Flash ESP8266 Gateway
# .\flash.ps1                     (serial, COM3)
# .\flash.ps1 -Port COM4          (serial, COM4)
# .\flash.ps1 -Ota -Ip 192.168.1.100  (OTA)

$ErrorActionPreference = "Stop"

function Test-Command {
    param($Command)
    return (Get-Command $Command -ErrorAction SilentlyContinue) -ne $null
}

$pio = $null
if (Test-Command pio) { $pio = "pio" }
elseif (Test-Command platformio) { $pio = "platformio" }

if (-not $pio) {
    Write-Error "PlatformIO CLI not found. Install: pip install platformio"
    exit 1
}

if ($Ota -or $Ip) {
    $target = $Ip
    if (-not $target) {
        $target = Read-Host "IP do Gateway para OTA"
    }
    Write-Host "Building and uploading OTA to $target..." -ForegroundColor Cyan
    & $pio run -e esp8266_gateway_ota --target upload --upload-port "$target"
} else {
    Write-Host "Building and flashing on $Port..." -ForegroundColor Cyan
    & $pio run -e esp8266_gateway --target upload --upload-port "$Port"
}

Write-Host "Done." -ForegroundColor Green
