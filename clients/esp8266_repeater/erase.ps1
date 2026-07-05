param (
    [string]$Port = "COM3"
)

$ErrorActionPreference = "Stop"

function Test-Command {
    param($Command)
    return (Get-Command $Command -ErrorAction SilentlyContinue) -ne $null
}

if (Test-Command pio) {
    Write-Host "Erasing via PlatformIO on $Port..." -ForegroundColor Cyan
    pio run -t erase --upload-port "$Port"
    exit
}

if (Test-Command platformio) {
    Write-Host "Erasing via PlatformIO on $Port..." -ForegroundColor Cyan
    platformio run -t erase --upload-port "$Port"
    exit
}

$esptool = "esptool.py"
if (-not (Get-Command $esptool -ErrorAction SilentlyContinue)) {
    $esptool = "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py"
}

if (Test-Path $esptool) {
    Write-Host "Erasing via esptool.py on $Port..." -ForegroundColor Cyan
    python "$esptool" --port "$Port" erase_flash
    exit
}

Write-Error "PlatformIO CLI or esptool.py not found"
Write-Host "Install: pip install platformio" -ForegroundColor Yellow
exit 1
