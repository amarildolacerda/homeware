<#
.SYNOPSIS
    Testa a descoberta Alexa (Espalexa) de um node lamp da AgriSense IoT.

.DESCRICAO
    O node lamp emula uma Philips Hue Bridge via biblioteca Espalexa. A Alexa
    descobre o device atraves de SSDP (M-SEARCH multicast 239.255.255.250:1900)
    e depois consome os endpoints HTTP:
      - /description.xml         -> descricao do device (so existe se Espalexa ativo)
      - /api/<user>/lights       -> lista de luminarias (user arbitrario, ex: agri)
    Este script valida os 3 pontos acima e indica se o Espalexa esta de fato
    respondendo no node alvo.

.COMO USAR
    1. Edite a variavel $NODE_IP abaixo com o IP do lamp que deseja testar.
    2. Execute no PowerShell do Windows (nao WSL2, pois o multicast UDP e o
       firewall podem se comportar diferente):
         powershell -ExecutionPolicy Bypass -File .\alexa_ssdp.ps1
    3. Interprete o resultado:
       - Test 1 (unicast): o node deve responder diretamente a um M-SEARCH.
       - Test 2 (multicast): lista os devices que responderam na rede; a coluna
         "Espalexa" = True confirma que e um lamp AgriSense.
       - Test 3 (HTTP): /description.xml deve retornar 200. Se der 404, o
         Espalexa nao inicializou no node (build sem -DALEXA_ENABLED ou
         falha no multicast UDP em runtime).

.PREREQUISITOS
    - Node lamp ligado e na mesma rede/WiFi do PC.
    - Firewall do Windows liberando entrada UDP 1900 (para receber as respostas).

.OBS
    O node deve ter sido flashado com uma env que define -DALEXA_ENABLED
    (ex: esp8266, esp8266_tcp, lamp_alexa em platformio.ini).
#>

$SSDP_MULTICAST = "239.255.255.250"
$SSDP_PORT = 1900
$NODE_IP = "192.168.1.14"

Write-Host "=== Alexa SSDP Test ===" -ForegroundColor Cyan
Write-Host "Node IP: $NODE_IP"
Write-Host ""

# Test 1: Unicast SSDP to node
Write-Host "1. Testing unicast SSDP to node..." -ForegroundColor Yellow
try {
    $udp = New-Object System.Net.Sockets.UdpClient
    $udp.Client.Bind([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0))
    
    $msg = "M-SEARCH * HTTP/1.1`r`nHOST: ${NODE_IP}:${SSDP_PORT}`r`nMAN: `"ssdp:discover`"`r`nMX: 3`r`nST: urn:schemas-upnp-org:device:basic:1`r`n`r`n"
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($msg)
    
    $udp.Send($bytes, $bytes.Length, $NODE_IP, $SSDP_PORT) | Out-Null
    
    $endpoint = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0)
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
    $udp.Client.Bind([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0))
    
    # Join multicast group
    $multicastAddress = [System.Net.IPAddress]::Parse($SSDP_MULTICAST)
    $udp.JoinMulticastGroup($multicastAddress)
    
    $msg = "M-SEARCH * HTTP/1.1`r`nHOST: ${SSDP_MULTICAST}:${SSDP_PORT}`r`nMAN: `"ssdp:discover`"`r`nMX: 3`r`nST: urn:schemas-upnp-org:device:basic:1`r`n`r`n"
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($msg)
    
    $udp.Send($bytes, $bytes.Length, $SSDP_MULTICAST, $SSDP_PORT) | Out-Null
    
    Write-Host "Waiting for responses (5s)..."
    
    $endpoint = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0)
    $udp.Client.ReceiveTimeout = 5000
    
    $responses = @()
    $start = Get-Date
    while (((Get-Date) - $start).TotalSeconds -lt 5) {
        try {
            $data = $udp.Receive([ref]$endpoint)
            $response = [System.Text.Encoding]::ASCII.GetString($data)
            $isEspalexa = ($response -match "description.xml") -or ($response -match "asic:1")
            $responses += [PSCustomObject]@{
                IP = $endpoint.Address.ToString()
                Espalexa = $isEspalexa
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

# Test 3: HTTP API (Espalexa emula Philips Hue: /description.xml e /api/<user>/lights)
Write-Host "3. Testing HTTP API (Espalexa/Hue)..." -ForegroundColor Yellow
try {
    $desc = Invoke-RestMethod -Uri "http://$NODE_IP/description.xml" -TimeoutSec 5
    Write-Host "[OK] /description.xml (Espalexa ativo):" -ForegroundColor Green
    $desc | Select-Object -Property * | Format-List
} catch {
    Write-Host "[FAIL] /description.xml: $_" -ForegroundColor Red
    Write-Host "  -> Espalexa provavelmente nao inicializou no node (ALEXA_ENABLED? begin() falhou?)" -ForegroundColor DarkYellow
}

try {
    $lights = Invoke-RestMethod -Uri "http://$NODE_IP/api/agri/lights" -TimeoutSec 5
    Write-Host "[OK] /api/agri/lights:" -ForegroundColor Green
    $lights | ConvertTo-Json -Depth 3
} catch {
    Write-Host "[INFO] /api/agri/lights: $_" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "=== Test Complete ===" -ForegroundColor Cyan
