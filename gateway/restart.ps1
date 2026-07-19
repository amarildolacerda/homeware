param(
    [string]$Host = "192.168.1.185"
)

try {
    $r = Invoke-WebRequest -Uri "http://$Host/api/restart" -Method POST -TimeoutSec 10
    Write-Host "Gateway reiniciado em $Host" -ForegroundColor Green
} catch {
    Write-Host "Falha ao reiniciar gateway em $Host : $_" -ForegroundColor Red
}
