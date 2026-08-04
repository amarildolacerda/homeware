# AgriSense IoT — Roadmap

## Futuro

### Hub IP fixo no node TCP
- **Problema**: `TcpNodeProtocol` descobre o hub via UDP broadcast (primeiro que responder ganha). Se 2 hubs estiverem na mesma rede, o node registra em um apenas.
- **Solução**: campo "Hub IP" no WiFiManager (salvo na EEPROM). Se configurado, o node pula o UDP discover e registra direto no hub configurado. Se não, usa o comportamento atual.
- **Impacto**: `tcp_node_protocol.cpp`, WiFiManager dos nodes TCP (lamp, etc.)

### Cobertura por tecnologia
- **LoRa**: localidades distantes, alcance longo, baixa taxa de dados
- **ESP-NOW**: pontos sem alcance do router, comunicação direta node↔hub sem WiFi
- **TCP/WiFi**: nodes próximos ao router, maior throughput, dashboards via HTTP
