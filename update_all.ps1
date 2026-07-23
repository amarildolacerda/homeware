<#
.SYNOPSIS
    Atualiza dispositivos via OTA — usa scan.py para descobrir, checa versão, atualiza se necessário
.EXAMPLE
    .\update_all.ps1
#>

$ErrorActionPreference = "Stop"

# ---- helpers ----

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

function Get-FwVersion {
    param([string]$ProjectDir)
    $paths = @(
        (Join-Path $ProjectDir "include/config.h"),
        (Join-Path $ProjectDir "src/config.h"),
        (Join-Path $ProjectDir "include/pages.h"),
        (Join-Path $ProjectDir "../shared/src/shared_config.h")
    )
    # Tambem busca na raiz do projeto (shared esta em submodulo pode nao estar clonado)
    $paths += (Join-Path $PSScriptRoot "shared/src/shared_config.h")
    foreach ($f in $paths) {
        $f = [System.IO.Path]::GetFullPath($f)
        if (-not (Test-Path $f)) { continue }
        $content = Get-Content $f -Raw -ErrorAction SilentlyContinue
        if ($content -match '#define\s+FW_VERSION\s+"([^"]+)"') {
            return $matches[1]
        }
    }
    return $null
}

function Compare-Version {
    param([string]$LocalVer, [string]$DeviceVer)
    if (-not $DeviceVer) { return $true }
    $re = '^v?(\d+)\.(\d+)\.(\d+)'
    $m1 = [regex]::Match($LocalVer, $re)
    $m2 = [regex]::Match($DeviceVer, $re)
    if (-not $m1.Success) { return $true }
    if (-not $m2.Success) { return $true }
    for ($i = 1; $i -le 3; $i++) {
        $a = [int]$m1.Groups[$i].Value
        $b = [int]$m2.Groups[$i].Value
        if ($a -gt $b) { return $true }
        if ($a -lt $b) { return $false }
    }
    return $false
}

# ---- descoberta via scan.py --json ----

function Discover-Devices {
    $scanPy = Join-Path $PSScriptRoot "scan.py"
    if (-not (Test-Path $scanPy)) {
        Write-Error "scan.py nao encontrado em $scanPy"
        exit 1
    }
    Write-Host "Descobrindo dispositivos via scan.py..." -ForegroundColor Cyan
    $raw = & python3 $scanPy --json 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Error "scan.py falhou: $raw"
        exit 1
    }
    try {
        $devices = $raw | ConvertFrom-Json
    } catch {
        Write-Error "Erro ao parsear JSON do scan.py: $_"
        exit 1
    }
    return $devices
}

# ---- deploy ----

function Update-DeviceType {
    param([string]$ProjectDir, [string]$EnvName, [array]$Devices, [string]$Label)
    $localVer = Get-FwVersion -ProjectDir $ProjectDir
    if (-not $localVer) { Write-Host "  [aviso] FW_VERSION nao encontrado em $ProjectDir"; return }

    Write-Host "`n=== $Label (local: $localVer) ===" -ForegroundColor Cyan
    $pendentes = $Devices | Where-Object {
        $need = Compare-Version -LocalVer $localVer -DeviceVer $_.fw_version
        if (-not $need) { Write-Host "  [SKIP] $($_.ip): ja atualizado ($($_.fw_version))" -ForegroundColor Green }
        $need
    }
    if ($pendentes.Count -eq 0) { Write-Host "  Todos atualizados."; return }

    Push-Location $ProjectDir
    Write-Host "  Build..."
    & $pio run -e $EnvName
    if ($LASTEXITCODE -ne 0) { Write-Error "  Build falhou"; Pop-Location; return }

    foreach ($dev in $pendentes) {
        Write-Host "  Enviando OTA para $($dev.ip)..."
        & $pio run -e $EnvName --target upload --upload-port $dev.ip
        if ($LASTEXITCODE -ne 0) { Write-Error "  OTA falhou: $($dev.ip)"; continue }
        Write-Host "  [OK] $($dev.ip) -> $localVer" -ForegroundColor Green
    }
    Pop-Location
}

# ---- main ----

$pio = Get-Pio
if (-not $pio) { Write-Error "PlatformIO CLI not found"; exit 1 }

$devices = Discover-Devices
if ($devices.Count -eq 0) { Write-Host "Nenhum dispositivo encontrado."; exit 0 }

Write-Host "`nDispositivos encontrados:" -ForegroundColor Cyan
$devices | ForEach-Object { Write-Host "  [$($_.type)] $($_.ip)  FW=$($_.fw_version)" }

$root = $PSScriptRoot
$clients = Join-Path $root "clients"

# Mapeamento tipo → (project dir, env name)
# ESP32: gateway    ESP8266: lampada, dht_gas, pir, repeater, onoff
$map = @{
    gateway = @{ Dir = Join-Path $root "gateway"; Env = "esp32_gateway_ota" }
    lampada = @{ Dir = Join-Path $clients "esp8266_lampada"; Env = "esp8266_ota" }
    dht_gas = @{ Dir = Join-Path $clients "esp8266_dht_gas"; Env = "esp8266_ota" }
    pir     = @{ Dir = Join-Path $clients "esp8266_pir"; Env = "esp8266_ota" }
    repeater= @{ Dir = Join-Path $clients "esp8266_repeater"; Env = "esp8266_ota" }
    onoff   = @{ Dir = Join-Path $clients "esp8266_onoff";  Env = "esp8266_ota" }
}

foreach ($type in $map.Keys) {
    $match = $devices | Where-Object { $_.type -eq $type }
    if ($match) {
        $cfg = $map[$type]
        Update-DeviceType -ProjectDir $cfg.Dir -EnvName $cfg.Env -Devices $match -Label $type
    }
}
$unknown = $devices | Where-Object { $_.type -eq "unknown" -or -not $map.ContainsKey($_.type) }
if ($unknown) {
    Write-Host "`n  [ignored] tipo desconhecido:" -ForegroundColor DarkGray
    $unknown | ForEach-Object { Write-Host "           $($_.ip) ($($_.type))" -ForegroundColor DarkGray }
}

Write-Host "`n=== Concluido ===" -ForegroundColor Green
