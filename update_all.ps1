<#
.SYNOPSIS
    Atualiza dispositivos via OTA — usa scan.py para descobrir, checa versão, atualiza se necessário
.PARAMETER Force
    Ignora a versão atual do dispositivo e força OTA em todos
.EXAMPLE
    .\update_all.ps1
.EXAMPLE
    .\update_all.ps1 -f
#>

param(
    [Alias('f')]
    [switch]$Force
)

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
        (Join-Path $ProjectDir "../shared/src/shared_config.h"),   # hub/
        (Join-Path $ProjectDir "../../shared/src/shared_config.h") # nodes/*
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
    param([string]$ProjectDir, [string]$EnvName, [array]$Devices, [string]$Label, [switch]$Force)
    $localVer = Get-FwVersion -ProjectDir $ProjectDir
    if (-not $localVer) { Write-Host "  [aviso] FW_VERSION nao encontrado em $ProjectDir"; return }

    Write-Host "`n=== $Label (local: $localVer)$(if ($Force) {' [FORCE]'}) ===" -ForegroundColor Cyan
    $pendentes = if ($Force) { $Devices } else {
        $Devices | Where-Object {
            $need = Compare-Version -LocalVer $localVer -DeviceVer $_.fw_version
            if (-not $need) { Write-Host "  [SKIP] $($_.ip): ja atualizado ($($_.fw_version))" -ForegroundColor Green }
            $need
        }
    }
    if ($pendentes.Count -eq 0) { Write-Host "  Todos atualizados."; return }

    Write-Host "  Build..."
    & $pio run -d $ProjectDir -e $EnvName
    if ($LASTEXITCODE -ne 0) { Write-Host "  Build falhou" -ForegroundColor Red; return }

    foreach ($dev in $pendentes) {
        Write-Host "  Enviando OTA para $($dev.ip)..."
        & $pio run -d $ProjectDir -e $EnvName --target upload --upload-port $dev.ip
        if ($LASTEXITCODE -ne 0) { Write-Host "  OTA falhou: $($dev.ip)" -ForegroundColor Red; continue }
        Write-Host "  [OK] $($dev.ip) -> $localVer" -ForegroundColor Green
    }
}

# ---- main ----

$pio = Get-Pio
if (-not $pio) { Write-Error "PlatformIO CLI not found"; exit 1 }

$devices = Discover-Devices
if ($devices.Count -eq 0) { Write-Host "Nenhum dispositivo encontrado."; exit 0 }

Write-Host "`nDispositivos encontrados:" -ForegroundColor Cyan
$devices | ForEach-Object { Write-Host "  [$($_.type)] $($_.ip)  FW=$($_.fw_version)  platform=$($_.platform)" }

$root = $PSScriptRoot
$nodes = Join-Path $root "nodes"

# Hub: split by platform (ESP32 vs ESP8266)
$gwEsp32   = $devices | Where-Object { $_.type -eq "gateway" -and $_.platform -ne "esp8266" }
$gwEsp8266 = $devices | Where-Object { $_.type -eq "gateway" -and $_.platform -eq "esp8266" }
if ($gwEsp32) {
    Update-DeviceType -ProjectDir (Join-Path $root "hub") -EnvName "hub_32_ota" -Devices $gwEsp32 -Label "hub (esp32)" -Force:$Force
}
if ($gwEsp8266) {
    Update-DeviceType -ProjectDir (Join-Path $root "hub") -EnvName "hub_8266_ota" -Devices $gwEsp8266 -Label "hub (esp8266)" -Force:$Force
}

# Nodes: dir base por tipo
$nodeDir = @{
    lampada       = "lamp"
    dht_gas       = "climate-gas"
    pir           = "presence"
    repeater      = "extender"
    onoff         = "switch"
}

# Mapa composto "tipo_plataforma" → env
# Per-node platformio.ini usa env "esp8266_ota"
$nodeEnv = @{}
foreach ($t in $nodeDir.Keys) {
    $nodeEnv["${t}_esp8266"] = "esp8266_ota"
}

# Agrupa devices por tipo conhecido
$groups = @{}
foreach ($t in $nodeDir.Keys) {
    $match = $devices | Where-Object { $_.type -eq $t }
    if (-not $match) { continue }
    $byPfx = $match | Group-Object { if ($_.platform) { $_.platform } else { "esp8266" } }
    foreach ($g in $byPfx) {
        $key = "${t}_$($g.Name)"
        $groups[$key] = $g.Group
    }
}

foreach ($key in $groups.Keys) {
    $type = $key -replace '_[^_]+$', ''
    $dir  = Join-Path $nodes $nodeDir[$type]
    $env  = $nodeEnv[$key]
    Update-DeviceType -ProjectDir $dir -EnvName $env -Devices $groups[$key] -Label $key -Force:$Force
}

$unknown = $devices | Where-Object {
    $_.type -eq "unknown" -or
    ($_.type -ne "gateway" -and -not ($nodeDir.Keys -contains $_.type))
}
if ($unknown) {
    Write-Host "`n  [ignored] tipo desconhecido:" -ForegroundColor DarkGray
    $unknown | ForEach-Object { Write-Host "           $($_.ip) ($($_.type))" -ForegroundColor DarkGray }
}

Write-Host "`n=== Concluido ===" -ForegroundColor Green
