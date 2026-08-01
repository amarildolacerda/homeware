param(
    [Parameter(Mandatory=$true)][string]$Gateway
)

try {
    $r = Invoke-WebRequest -Uri "http://$Gateway/api/restart" -Method POST -TimeoutSec 10
    switch ($r.StatusCode) {
        200 { Write-Host "ok (HTTP 200)" -ForegroundColor Green }
        400 { Write-Host "erro: max sensors reached or already pairing" -ForegroundColor Yellow }
        default { Write-Host "falha: HTTP $($r.StatusCode)" -ForegroundColor Red }
    }
} catch {
    Write-Host "falha: $($_.Exception.Message)" -ForegroundColor Red
}
