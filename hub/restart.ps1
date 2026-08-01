param(
    [string]$Gateway,
    [switch]$All
)

function Restart-Gateway($ip) {
    try {
        $r = Invoke-WebRequest -Uri "http://$ip/api/restart" -Method POST -TimeoutSec 10
        switch ($r.StatusCode) {
            200 { Write-Host "  $ip ok (HTTP 200)" -ForegroundColor Green }
            400 { Write-Host "  $ip erro: max sensors reached or already pairing" -ForegroundColor Yellow }
            default { Write-Host "  $ip falha: HTTP $($r.StatusCode)" -ForegroundColor Red }
        }
    } catch {
        Write-Host "  $ip falha: $($_.Exception.Message)" -ForegroundColor Red
    }
}

if ($All) {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $scanPy = Join-Path (Split-Path -Parent $scriptDir) "scan.py"
    if (-not (Test-Path $scanPy)) {
        Write-Host "scan.py nao encontrado em $scanPy" -ForegroundColor Red
        exit 1
    }
    Write-Host "Scanning rede..." -ForegroundColor Cyan
    $json = python $scanPy --json 2>$null
    $devices = $json | ConvertFrom-Json
    $gateways = $devices | Where-Object { $_.type -eq "gateway" }
    if (-not $gateways -or $gateways.Count -eq 0) {
        Write-Host "Nenhum gateway encontrado na rede" -ForegroundColor Yellow
        exit 0
    }
    Write-Host "Encontrados $($gateways.Count) gateway(s):" -ForegroundColor Cyan
    foreach ($gw in $gateways) {
        Restart-Gateway $gw.ip
    }
} elseif ($Gateway) {
    Restart-Gateway $Gateway
} else {
    Write-Host "Uso:" -ForegroundColor Yellow
    Write-Host "  .\restart.ps1 -Gateway <ip>        Reinicia um gateway"
    Write-Host "  .\restart.ps1 -All                 Reinicia todos os gateways da rede (via scan.py)"
}
