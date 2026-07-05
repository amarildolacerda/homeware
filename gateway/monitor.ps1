param (
    [string]$Port = "COM3"
)

# Monitor serial do Gateway
# .\monitor.ps1              (COM3)
# .\monitor.ps1 -Port COM4   (COM4)

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

Write-Host "Monitoring on $Port (Ctrl+C to exit)..." -ForegroundColor Cyan
& $pio device monitor --port "$Port" --baud 115200
