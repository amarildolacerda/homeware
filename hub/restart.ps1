param(
    [string]$Gateway = "192.168.1.19"
)

try {
    $r = Invoke-WebRequest -Uri "http://$Gateway/api/restart" -Method POST -TimeoutSec 10
    Write-Host "Gateway reiniciado em $Gateway" -ForegroundColor Green
} catch {
    Write-Host "Falha ao reiniciar gateway em $Gateway : $_" -ForegroundColor Red
}
