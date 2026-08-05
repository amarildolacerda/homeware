# TCP Push Command + Hub RSSI no Sidebar — Design

Data: 2026-08-05 · Branch: dev

## Objetivo

1. **Push de comando do hub para nodes TCP**: quando o hub recebe um comando de mudança de estado para um node TCP (lamp) com IP conhecido, enviar direto via `POST /api/relay` no IP do node, mantendo o polling (`GET /node/command`) como fallback.
2. **RSSI do hub no sidebar**: exibir a intensidade do sinal WiFi do próprio hub (`WiFi.RSSI()`) no header do sidebar (direita) do dashboard do hub.

## Arquitetura

### Task 1 — Push com fallback para polling

**Fluxo atual** (não alterado como base):
- `TcpRadioHandler::send_command(mac, state)` enfileira `PendingCommand` em `m_pending_commands[device_id]`.
- O node TCP polla `GET /node/command/{device_id}` e aplica o comando (`set_relay`).

**Novo fluxo:**
- `send_command(mac, state)`:
  - Enfileira o `PendingCommand` (polling) como hoje — fallback garantido.
  - Se `sensor->ip` for conhecido (algum byte != 0), enfileira também um `PushCommand { device_id, ip[4], state, created_at }` em `m_push_queue`.
- `loop()` passa a chamar `process_push_queue()` (não-bloqueante — regra 15):
  - **Máximo 1 tentativa HTTP por ciclo de loop** (`break` após o primeiro push tentado) para limitar bloqueio do loop.
  - **Retry rate-limited**: cada push só é tentado de novo após `TCP_PUSH_RETRY_INTERVAL_MS` (1000ms, define local no `.cpp` — não tocar shared).
  - Para cada push tentado dentro de `TCP_COMMAND_TTL_MS`:
    - Monta URL `http://{ip}:{TCP_HTTP_PORT}/api/relay` e faz `POST` com corpo `{"state":true|false}` via `HTTPClient` (timeout curto, ex. 800ms).
    - **HTTP 200** → remove o `PendingCommand` de fallback mais antigo (front) da fila daquele `device_id` (FIFO alinhado ao push) + remove o push da fila. Log de sucesso.
    - **Falha/timeout/erro** → mantém o push (retry após intervalo); o polling segue como fallback.
  - Pushes expirados (TTL) são removidos.
- Idempotência: `set_relay(bool)` no lamp torna dupla entrega inofensiva (regra TCP: `on_command` seta, não alterna).

**Estruturas:**

```cpp
struct PushCommand {
    String device_id;
    uint8_t ip[4];
    uint8_t state;
    unsigned long created_at;
    unsigned long last_attempt_ms;  // 0 = nunca tentado
};
std::vector<PushCommand> m_push_queue;
```

**Métodos novos:**
- `void process_push_queue();`
- `void remove_pending_commands(const std::string& device_id);` (helper p/ remover fallback ao push OK)

**Includes novos em `tcp_radio_handler.cpp`:** `<WiFi.h>`, `<HTTPClient.h>`.

### Task 2 — RSSI do hub no sidebar

**Backend** (`hub/src/web_server.cpp`, handler `/api/info`):
- Adicionar `doc["wifi_rssi"] = WiFi.RSSI();`

**Frontend** (`hub/include/pages.h`, página principal — `PAGE_HUB`/dashboard):
- `.logo` vira flex-row: título à esquerda, indicador de sinal à direita.
- HTML do indicador:
  ```html
  <div class="signal" id="hubSignal" title="RSSI: --">
    <span class="bar"></span><span class="bar"></span><span class="bar"></span><span class="bar"></span>
  </div>
  ```
- CSS:
  ```css
  .logo-row{display:flex;align-items:center;justify-content:space-between;gap:8px}
  .signal{display:flex;align-items:flex-end;gap:2px;height:14px}
  .signal .bar{width:3px;border-radius:1px;background:var(--border-strong)}
  .signal .bar:nth-child(1){height:4px}
  .signal .bar:nth-child(2){height:7px}
  .signal .bar:nth-child(3){height:10px}
  .signal .bar:nth-child(4){height:13px}
  .signal .bar.on{background:var(--success)}
  ```
- JS `updateHubSignal(rssi)`:
  - Nível de barras: `>= -40` → 4, `>= -60` → 3, `>= -70` → 2, `>= -80` → 1, senão 0.
  - Seta `title` para `RSSI: <rssi> dBm`.
- Chamada no `loadData()` (overview, poll 5s) após fetch de `/api/info`, e no load da página settings (segundo fetch de `/api/info`).
- No media query `max-width:700px` (modo colapsado 60px), esconder o `.signal` junto com `h1`/`span`.

## Arquivos alterados

| Arquivo | Mudança |
|---|---|
| `hub/src/tcp_radio_handler.h` | struct `PushCommand`, `m_push_queue`, `process_push_queue()`, `remove_pending_commands()` |
| `hub/src/tcp_radio_handler.cpp` | push no `send_command`, `process_push_queue()` no `loop()`, includes HTTP |
| `hub/src/web_server.cpp` | `wifi_rssi` no `/api/info` |
| `hub/include/pages.h` | logo flex-row + indicador de sinal + CSS + JS |

Sem alterações no lamp (usa `POST /api/relay` existente) nem em outros nodes.

## Tratamento de erro

- **Push falha** (node offline, timeout, HTTP != 200): comando permanece na fila de polling — o node recebe quando voltar a pollar. Push retry até TTL.
- **Sem IP conhecido**: apenas polling (comportamento atual).
- **HTTPClient em loop**: instância local por push, `end()` sempre chamado; timeout 800ms; `yield()`/`delay(1)` conforme padrão ESP32 para não travar WDT.

## Verificação

- Build do hub (`pio run -e hub` ou script `build.sh`) — compila com e sem `TCP_ENABLED`.
- Teste manual (bancada): lamp TCP registrado com IP → comando ON/OFF pelo dashboard/hub console → observar POST `/api/relay` no log do lamp (atraso < polling interval) e estado atualizado. Desligar o lamp → comando → verificar fallback de polling quando o lamp voltar.
- Dashboard: conferir barras de sinal no sidebar refletindo `wifi_rssi` do `/api/info`, incluindo tooltip.

## Fora de escopo (YAGNI)

- Push para `restart` (mantém polling).
- Indicador de sinal por node no sidebar (só RSSI global do hub).
- Remoção total do polling.
