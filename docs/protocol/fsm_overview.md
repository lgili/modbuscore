# Visão Geral – FSM do Motor Modbus

O motor ModbusCore opera com injeção de dependências e avança em micro-passos cooperativos. Os estados atuais são:

```
    ┌────────┐    dados RX
    │ IDLE   │ ─────────────▶ │ RECEIVING │
    └───┬────┘                │    │
        │ rearm timeouts      │    │ buffer pronto
        ▼                     ▼    │
    │ WAIT_RESPONSE │ ◀───┐ SENDING │
        ▲                │    │
        │ timeout        └────┘
```

- **IDLE** — aguardando trabalho; primeira chamada a `mbc_engine_step` consulta o transporte e decide passar para `RECEIVING` (se bytes disponíveis) ou `SENDING` (se há requisições pendentes).
- **RECEIVING** — consome ADU de entrada; quando buffer completo, dispara evento `RX_READY` e transita para `WAIT_RESPONSE` (cliente) ou permanece em `SENDING` (servidor) para responder.
- **SENDING** — emite ADU (resposta ou requisição); ao completar, gera evento `TX_SENT` e volta para `WAIT_RESPONSE` (cliente) ou `IDLE` (servidor).
- **WAIT_RESPONSE** — cliente aguarda resposta; timeouts disparam evento `TIMEOUT` e retornam a `IDLE`.

Eventos emitidos em cada passo: `STEP_BEGIN` → (`RX_READY` quando bytes chegam) → `PDU_READY` (quando um PDU é decodificado) → `STATE_CHANGE` → `STEP_END`. Em caso de expiração de tempo, `TIMEOUT` é enviado antes do retorno ao estado `IDLE`.

Próximos passos:
- Preencher funções de transição com codificação/decodificação de ADU.
- Integrar tabelas de FC e hooks de telemetria detalhada.
