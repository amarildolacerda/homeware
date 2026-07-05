param ([string]$IP = "192.168.1.101")
$ErrorActionPreference = "Stop"
function Test-Command { param($Command); return (Get-Command $Command -ErrorAction SilentlyContinue) -ne $null }
if (Test-Command pio) { pio run -t upload --upload-port "$IP"; exit }
if (Test-Command platformio) { platformio run -t upload --upload-port "$IP"; exit }
$firmware = ".pio\build\esp8266\firmware.bin"
if (-not (Test-Path $firmware)) { if (Test-Command pio) { pio run } elseif (Test-Command platformio) { platformio run } else { Write-Error "PlatformIO not found"; exit 1 } }
python -m espota -i $IP -p 8266 -f $firmware
