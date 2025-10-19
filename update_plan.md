# ModbusCore 1.0 – Plano de Iterações (Atualizado)

Este arquivo acompanha o progresso incremental da nova implementação.  
O roadmap completo está em `roadmap.md`; aqui focamos no curto prazo.

---

## ✅ Iteração 0 – Bootstrap (concluído)

- Arquivar código antigo em `bak/`.
- Criar skeleton CMake + biblioteca `modbuscore` (C17).
- Definir status codes e runtime básico com injeção de dependências mínimas.
- Adicionar teste `runtime_smoke` e garantir build/ctest verdes.

---

## 🚧 Iteração 1 – Consolidar Runtime/DI

**Objetivos**
- Permitir dependências opcionais (logger, allocator etc.) com defaults seguros.
 - Builder fluente (`mbc_runtime_builder`) para evitar `memset`.
- Introduzir contratos para diag/metrics (stubs).
- Documentar fluxo de bootstrap e ownership.

**Progresso**
- ✅ `mbc_runtime_builder` criado com defaults para clock/allocator/logger.
- ✅ Teste `runtime_smoke` cobre builder com mocks.

**Critérios de saída**
- Testes cobrindo builder, defaults e erros de configuração.
- README atualizado com exemplo de bootstrap completo.

---

## 🔜 Iteração 2 – Desenho dos Contratos de Transporte

**Objetivos**
- Definir interface de transporte moderna (send/recv async-friendly, budget).
- Preparar mocks oficiais (transporte determinístico para testes).
- Especificar traits de clock/timeout/guard times.

**Progresso**
- ✅ `mbc_transport_iface_t` definido com helpers (`mbc_transport_send/receive/now/yield`).
- ✅ Teste `transport_iface_smoke` cobre cenários de sucesso e falhas.

**Critérios**
- Header `transport_iface.h` com doc clara + teste unitário mockando send/recv.
- Guia rápido explicando como implementar transporte custom.

---

## 🔮 Iteração 3 – Núcleo Protocolar (Skeleton)

**Objetivos**
- Criar módulo `protocol/engine` com FSM placeholders (cliente/servidor).
- Garantir que FSM consuma somente interfaces injetadas (sem `#ifdef`).
- Estabelecer eventos de telemetria (hook de logger/diag).

**Progresso**
- ✅ `mbc_engine` com DI (cliente/servidor) e estados `IDLE/RECEIVING/SENDING/WAIT_RESPONSE`.
- ✅ Eventos básicos (`STEP_BEGIN`, `RX_READY`, `TX_SENT`, `STEP_END`, `STATE_CHANGE`, `TIMEOUT`) publicados + loop com orçamento.
- ✅ API `mbc_engine_submit_request` para TX simples com verificação de busy e timeout configurável.
- ✅ Utilitários de PDU (`pdu.h`) com encode/decode e request/response de FC03/06/16 + tratamento de exceções.
- ✅ Testes `protocol_engine_smoke` (incluindo timeout) e `pdu_smoke` cobrem as novas funcionalidades.

**Critérios**
- State machine base compila com testes simulando ciclos simples (mock).
- Documentação de estados e transições principais (draft).

---

## ✍️ Notas Gerais

- Sempre manter cobertura de testes em cada commit relevante.
- Próximas fases (drivers oficiais, perfis, DX) seguirão após estabilizar core.
- Qualquer extração do `bak/` deve vir acompanhada de refatoração com nomenclatura clara.

---

_Última atualização: 2025-03-28_
