# Alexa SSDP Test - Windows PowerShell
# Run this from Windows (not WSL2) to test multicast SSDP

$SSDP_MULTICAST = "239.255.255.250"
$SSDP_PORT = 1900
$NODE_IP = "192.168.1.20"

Write-Host "=== Alexa SSDP Test ===" -ForegroundColor Cyan
Write-Host "Node IP: $NODE_IP"
Write-Host ""

# Test 1: Unicast SSDP to node
Write-Host "1. Testing unicast SSDP to node..." -ForegroundColor Yellow
try {
    $udp = New-Object System.Net.Sockets.UdpClient
    $udp.Client.Bind(New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0))
    
    $msg = "M-SEARCH * HTTP/1.1`r`nHOST: ${NODE_IP}:${SSDP_PORT}`r`nMAN: `"ssdp:discover`"`r`nMX: 3`r`nST: urn:schemas-upnp-org:device:Basic:1`r`n`r`n"
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($msg)
    
    $udp.Send($bytes, $bytes.Length, $NODE_IP, $SSDP_PORT) | Out-Null
    
    $endpoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
    $udp.Client.ReceiveTimeout = 3000
    
    $data = $udp.Receive([ref]$endpoint)
    $response = [System.Text.Encoding]::ASCII.GetString($data)
    
    Write-Host "[OK] Unicast SSDP response from $($endpoint.Address):" -ForegroundColor Green
    Write-Host $response.Substring(0, [Math]::Min(300, $response.Length))
    $udp.Close()
} catch {
    Write-Host "[FAIL] Unicast SSDP: $_" -ForegroundColor Red
}

Write-Host ""

# Test 2: Multicast SSDP
Write-Host "2. Testing multicast SSDP..." -ForegroundColor Yellow
try {
    $udp = New-Object System.Net.Sockets.UdpClient
    $udp.Client.Bind(New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0))
    
    # Join multicast group
    $multicastAddress = [System.Net.IPAddress]::Parse($SSDP_MULTICAST)
    $udp.JoinMulticastGroup($multicastAddress)
    
    $msg = "M-SEARCH * HTTP/1.1`r`nHOST: ${SSDP_MULTICAST}:${SSDP_PORT}`r`nMAN: `"ssdp:discover`"`r`nMX: 3`r`nST: urn:schemas-upnp-org:device:Basic:1`r`n`r`n"
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($msg)
    
    $udp.Send($bytes, $bytes.Length, $SSDP_MULTICAST, $SSDP_PORT) | Out-Null
    
    Write-Host "Waiting for responses (5s)..."
    
    $endpoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
    $udp.Client.ReceiveTimeout = 5000
    
    $responses = @()
    $start = Get-Date
    while (((Get-Date) - $start).TotalSeconds -lt 5) {
        try {
            $data = $udp.Receive([ref]$endpoint)
            $response = [System.Text.Encoding]::ASCII.GetString($data)
            $responses += [PSCustomObject]@{
                IP = $endpoint.Address.ToString()
                Response = $response.Substring(0, [Math]::Min(200, $response.Length))
            }
            Write-Host "[OK] Response from $($endpoint.Address)" -ForegroundColor Green
        } catch {
            break
        }
    }
    
    $udp.Close()
    
    if ($responses.Count -gt 0) {
        Write-Host "`nGot $($responses.Count) response(s)" -ForegroundColor Green
        $responses | Format-Table -AutoSize
    } else {
        Write-Host "`n[FAIL] No multicast responses - multicast may be blocked" -ForegroundColor Red
    }
} catch {
    Write-Host "[FAIL] Multicast SSDP: $_" -ForegroundColor Red
}

Write-Host ""

# Test 3: HTTP API
Write-Host "3. Testing HTTP API..." -ForegroundColor Yellow
try {
    $response = Invoke-RestMethod -Uri "http://$NODE_IP/api/lights" -TimeoutSec 5
    Write-Host "[OK] /api/lights:" -ForegroundColor Green
    $response | ConvertTo-Json -Depth 3
} catch {
    Write-Host "[FAIL] HTTP API: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Test Complete ===" -ForegroundColor Cyan
