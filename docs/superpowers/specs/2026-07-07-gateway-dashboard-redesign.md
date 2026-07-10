# Gateway Dashboard Redesign

## Architecture

SPA leve com shell + 3 páginas carregadas via `fetch()`:

- **Shell** (`PAGE_SHELL`): sidebar fixa + container dinâmico + JS de roteamento. Servido em `/`.
- **Overview** (`PAGE_OVERVIEW`): servido em `/overview`, carregado via fetch no container.
- **Settings** (`PAGE_SETTINGS`): servido em `/settings`, carregado via fetch no container.
- **Logs** (`PAGE_LOGS`): servido em `/logs`, carregado via fetch no container.

### Tamanhos estimados (PROGMEM)

| Página | Tamanho | Descrição |
|--------|---------|-----------|
| `PAGE_SHELL` | ~2KB | Sidebar + CSS base + roteador JS |
| `PAGE_OVERVIEW` | ~5KB | Stats + grid sensores com ícones/indicadores |
| `PAGE_SETTINGS` | ~2KB | Config MQTT + info bridge |
| `PAGE_LOGS` | ~2KB | Tabela de eventos |

### Navegação

Sidebar esquerda fixa com 3 links:
- **Visão Geral** (ícone: grid) → `/overview`
- **Configurações** (ícone: engrenagem) → `/settings`
- **Logs** (ícone: lista) → `/logs`

Link ativo ganha bg `var(--primary-focus)`.

Roteador JS:
```js
function navigate(page) {
    fetch('/' + page).then(r => r.text()).then(html => {
        document.getElementById('page').innerHTML = html;
    });
    // highlight active link
}
```

Mobile: sidebar vira bottom nav em <600px.

## Paleta de Cores (atualizada)

Tema claro do mockup. Ver style guide em `.opencode/skills/style-guide/SKILL.md`.

- `--bg: #f4f4f4` — fundo cinza claro
- `--surface: #ffffff` — cartões brancos
- `--text: #1f2937` — texto principal
- `--primary: #5e6ad2` — destaque lavanda
- `--border: #e5e7eb` — bordas suaves
- `--success: #16a34a` — verde ativo
- `--danger: #dc2626` — vermelho erro/alerta
- `--warn: #f59e0b` — amarelo alerta
- `--info: #2563eb` — azul informação

## Overview

### Stats Bar
4 métricas no topo: Pareados, Online, RX Total, Uptime. Igual ao atual.

### Grid de Sensores

**Ícones por tipo** (SVG inline):

| Tipo | Ícone |
|------|-------|
| 1 (Temp+Hum) | Termômetro |
| 2 (Contato) | Sino |
| 3 (Movimento) | Figura correndo |
| 4 (Gás) | Nuvem |
| 5 (Chuva) | Gota |
| 6 (Tanque) | Nível |
| 7 (DHT+Gas) | Combinado |
| 8 (Interruptor) | Tomada |

**Indicadores coloridos:**
- Bateria > 50% → barra verde (`var(--success)`)
- Bateria 20-50% → barra amarela (`var(--warn)`)
- Bateria < 20% → barra vermelha (`var(--danger)`) + badge "BATERIA FRACA"
- RSSI > -70 dBm → verde; -70 a -85 → amarelo; < -85 → vermelho

**Alerta offline:** Card com borda vermelha, badge "Offline" pulsando.

**Filtro por tipo:** Botões "Todos / Temp / Contato / Interruptor / Gás / Chuva" no topo do grid. Ao clicar, filtra os sensores exibidos (JS, sem fetch).

**Touch-friendly:** Todos os botões com `min-height: 44px`.

**Botões do sensor:** Menu ⋮ com "Renomear", "Remover" (igual ao dropdown já implementado).

### Sensor type 8 (Interruptor)
Botão ON/OFF grande (ocupando largura do card), cor verde quando ligado, cinza quando desligado.

## Settings

Cards:
- **MQTT:** formulário com Host, Porta, Usuário, Senha + Salvar
- **Bridge:** exibe `device_id`, `fw_version` (read-only)
- **Ações:** botão "Reiniciar" (com `confirm()`) e "Re-registrar" (broadcast)

## Logs

- Busca `GET /api/logs` → `{ "logs": [ { "t": <epoch_ms>, "msg": "...", "level": "info"|"warn"|"error" } ] }`
- Exibe tabela: timestamp formatado + mensagem
- Máximo 50 eventos (buffer circular no firmware)
- Auto-refresh a cada 10s
- Cores por severidade: info (normal), warn (amarelo), error (vermelho)
- Vazio: "Nenhum evento registrado"

### Endpoint novo: `GET /api/logs`
- Retorna JSON com array de logs
- Sem autenticação (uso local)

### Buffer circular (firmware)
- Array de 50 structs `{ unsigned long time; char msg[64]; char level[8]; }`
- Função `log_add(const char* level, const char* fmt, ...)` chamada em eventos:
  - Sensor pareado/removido
  - Sensor online/offline
  - Erro MQTT (conexão, publish)
  - Comando recebido (ON/OFF)
  - Pareamento iniciado/finalizado
  - Boot / restart

## Responsivo

- **< 900px:** sidebar vira bottom nav fixa na base (3 ícones + label)
- **< 600px:** stats grid 2 colunas; padding reduzido
- **Touch:** todos botões com `min-height: 44px`

## Endpoints existentes (inalterados)

- `GET /api/info` — stats + MQTT status
- `GET /api/sensors` — lista sensores
- `POST /api/restart` — restart
- `POST /api/broadcast` — re-registro
- `POST /api/pair/start`, `/api/pair/stop`
- `POST /api/sensor/{slot}/name`, `/remove`, `/command`
- `POST /api/config/mqtt`
- `GET /docs` — API docs

## Backend (web_server.cpp)

- Adicionar rota `GET /overview` → serve `PAGE_OVERVIEW`
- Adicionar rota `GET /settings` → serve `PAGE_SETTINGS`
- Adicionar rota `GET /logs` → serve `PAGE_LOGS`
- Adicionar rota `GET /api/logs` → retorna JSON dos logs
- Rota `/` → serve `PAGE_SHELL`
- Manter `/docs` inalterado
