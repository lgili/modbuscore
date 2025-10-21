# ModbusCore 1.0 – Roadmap para Reimplementação do Zero

Este documento descreve o plano completo para reconstruir a biblioteca ModbusCore do zero, garantindo API unificada, portabilidade total e arquitetura baseada em injeção de dependências. Cada fase possui objetivos, entregáveis obrigatórios e critérios de saída antes de avançar.

---

## Visão & Princípios Orientadores

1. **API única e consistente** para todos os cenários (desktop, embedded, servidor).
2. **Arquitetura modular** com injeção de dependências (DI) explícita em todas as camadas.
3. **Experiência de desenvolvimento (DX)** fluida: perfis declarativos, exemplos executáveis, documentação integrada.
4. **Testabilidade total**: tudo deve ser mockável e testável de cima a baixo.
5. **Robustez e extensibilidade**: suportar novos transports/protocolos sem tocar o core.

---

## Fase 0 – Pesquisa & Requisitos

- **Objetivo**: Capturar requisitos, restrições e expectativas.
- **Entregáveis**:
  - Lista de casos de uso (cliente, servidor, gateway, diagnostics).
  - Matriz de plataformas alvo (bare metal, FreeRTOS, POSIX, Windows).
  - Mapa de integración com projetos existentes (compat libmodbus, perfis current).
  - Inventário de lições aprendidas da base atual (dívidas técnicas/resgates).
- **Critério de saída**:
  - Reunião com stakeholders/usuários aprova o escopo.
  - Documento “Requirements.md” versionado.

---

## Fase 1 – Arquitetura e DSL de Configuração

- **Objetivo**: Definir a espinha dorsal da nova biblioteca.
- **Entregáveis**:
  - Arquitetura de camadas (Core, Runtime, Adapters, Drivers, Tooling).
  - Definição de interfaces centrais:
    - `mb_transport_iface`
    - `mb_clock_iface`
    - `mb_allocator_iface`
    - `mb_logger_iface`
    - `mb_diag_sink`
  - Especificação do DSL de configuração (TOML/YAML) e conversor para C/CMake.
- **Critério de saída**:
  - Diagramas revistos e aprovados.
  - Prototipo mínimo criando e injetando dependências (sem lógica Modbus ainda).

---

## Fase 2 – Runtime de Injeção de Dependências

- **Objetivo**: Implementar o “container” DI e builders de runtime.
- **Entregáveis**:
  - `mb_runtime_builder`: orquestra instâncias (transporte, mempool, watchdog).
  - Suporte a ownership explícito (stack vs heap vs static).
  - Sistema de perfis declarativos montando runtime default.
- **Critério de saída**:
  - Testes com mocks demonstrando substituição de qualquer componente.
  - Ferramenta CLI simples (`modbus new`) gerando esqueleto baseado em perfis.

---

## Fase 3 – Núcleo Protocolar (Stateless)

- **Objetivo**: Implementar núcleo Modbus puramente orientado a dados/estado externo.
- **Entregáveis**:
  - State Machines para cliente e servidor independentes de transporte.
  - Manejo completo de FCs com tabelas de operação (hooks por FC).
  - Tratamento de tempo via `mb_clock_iface`.
  - Hooks de métricas/telemetria.
- **Progresso atual**:
  - Skeleton `mbc_engine` com DI (cliente/servidor) e estados `IDLE/RECEIVING/SENDING/WAIT_RESPONSE`.
  - Eventos básicos (`STEP_BEGIN`, `RX_READY`, `TX_SENT`, `STEP_END`, `STATE_CHANGE`, `TIMEOUT`) emitidos a cada passo.
  - Loop com orçamento inicial e timeouts de cliente respeitando `response_timeout_ms`.
  - API `mbc_engine_submit_request` permite TX simples com verificação de busy.
  - Utilitários PDU (`pdu.h`) com encode/decode e builders de FC03/FC06/FC16, além de parser de exceções.
  - Teste `protocol_engine_smoke` cobre init/step/TX/erros com mocks determinísticos.
- **Critério de saída**:
  - Testes unitários 100% cobrindo modos happy-path / error / timeout.
  - Benchmarks básicos comparando com versão antiga (mínimo regressão).

---

## Fase 4 – Drivers Oficiais de Transporte & Port

- **Objetivo**: Fornecer implementações padrão com DI.
- **Entregáveis**:
  - Drivers certificados:
    - ✅ POSIX TCP (completado)
    - ⏳ POSIX UDP (opcional)
    - ✅ Windows Winsock (cliente)
    - ⏳ FreeRTOS stream buffers (futuro)
    - ✅ Bare-metal UART (RTU) com guard-times configuráveis
    - ⏳ POSIX RTU (termios)
    - ⏳ Windows RTU (Win32 serial API)
  - ✅ Suite de mocks profissionais (modo deterministic, latência configurável).
- **Progresso atual (Fase 4A)**:
  - ✅ Driver POSIX TCP implementado (324 linhas)
  - ✅ Socket não-bloqueante com polling cooperativo
  - ✅ Resolução de hostname (IP + DNS)
  - ✅ TCP_NODELAY e keepalive configurados
  - ✅ Testes unitários (test_posix_tcp.c)
  - ✅ Exemplo end-to-end funcional (example_tcp_client_fc03.c)
  - ✅ Driver Winsock TCP (cliente não-bloqueante com WSAStartup)
  - ✅ Servidor mock Python para testes de integração
  - ✅ Script de teste automatizado (run_integration_test.sh)
  - ✅ Documentação completa (PHASE4_COMPLETE.md)
  - ✅ Mock de transporte determinístico (`mock.c`) com falhas, latência e helpers de teste
  - ✅ Casos de resiliência para o engine (`tests/test_engine_resilience.c`)
- **Critério de saída**:
  - ✅ Driver TCP com testes integrados e documentação passo a passo
  - ⏳ Perfis `POSIX_CLIENT`, `FREERTOS_RTU`, `BARE_GATEWAY` (futuro)

---

## Fase 5 – Experiência do Desenvolvedor (DX) & Tooling

- **Objetivo**: Facilitar onboarding, scaffolding e documentação.
- **Entregáveis**:
  - CLI “modbus” com comandos:
    - `modbus new profile`
    - `modbus add transport`
    - `modbus doctor` (diagnóstico de config).
  - Exemplos executáveis (desktop, RTU, gateway, CI pipeline).
  - Documentação viva (Doxygen + Sphinx + snippets compilados).
- **Critério de saída**:
  - Tutorial “Hello Modbus em 5 minutos” validado por usuário externo.
  - Documentação automática versionada (gh-pages ou docs site).

---

## Fase 6 – Migração e Limpeza

- **Objetivo**: Declarar oficialmente a versão 1.0 (DI) como API suportada e
  remover artefatos legados.
- **Entregáveis**:
  - Documentação “Breaking Changes” explicando a transição para 1.0.
  - Limpeza completa da pasta `bak/` e de referências à API antiga.
  - Checklist para quem está adotando a 1.0 do zero (sem compat layer).
- **Critério de saída**:
  - Repositório sem código 2.x.
  - README/Quick Start destacam apenas a API moderna.
  - Roadmap atualizado refletindo o abandono da compat layer.

---

## Fase 7 – Robustez, Diagnósticos e Segurança

- **Objetivo**: Completar features críticos de produção.
- **Entregáveis**:
  - Módulos opcionais:
    - TLS / DTLS (quando aplicável).
    - Modo “managed auto-heal” (retries, debouncing, circuit-breaker).
      - ✅ Supervisor `mbc_autoheal_supervisor_t` com backoff exponencial e circuit breaker.
    - Diagnósticos avançados (Eventos, logs estruturados, tracing).
      - ⚙️ `mbc_diag_sink` integrado ao runtime; engine já emite eventos estruturados.
  - Testes de fuzzing e fault-injection integrados.
    - ✅ LibFuzzer harnesses implementados: `fuzz_mbap_decoder`, `fuzz_rtu_crc`, `fuzz_pdu_parser`.
    - ✅ Seed corpus com 15 frames Modbus válidos (MBAP, RTU, PDU).
    - ✅ CI/CD com fuzzing automático (PR: 60s, Semanal: 6h).
    - ⏳ Campanha de 1B execuções em andamento (ver `FUZZING.md`).
- **Critério de saída**:
  - Cobertura de fuzz nos parsers > 1B execs sem falha.
  - Certificação interna de resiliência (latência, ruído, drop > 30%).

---

## Fase 8 – Distribuição, Suporte e Roadmap Futuro

- **Objetivo**: Preparar lançamento e governança contínua.
- **Entregáveis**:
  - Releases binários/pacotes (conan/vcpkg, pkg-config, CMake package).
  - Política de versionamento semântico e suporte LTS.
  - Backlog público de features e roadmap pós-lançamento.
- **Critério de saída**:
  - Lançamento da versão 1.0 (release notes, changelog, anúncio).
  - Acordo de suporte com comunidade / equipes internas.

---

## Checklist de Governança por Fase

- Documentar decisões (ADR) e associá-las ao roadmap.
- Cada fase só avança após aprovação em design review.
- Manter branch dedicado por fase com testes verdes antes do merge.
- Atualizar métricas (performance, footprint, DX) a cada marco.

---

## Próximos Passos Imediatos

1. Realizar Fase 0 (pesquisa) com entrevistas, levantamento de requisitos.
2. Montar time/líderes responsáveis por cada fase.
3. Agendar checkpoint quinzenal de alinhamento estratégico.

---

**Contato & Manutenção do Roadmap**
- Responsável inicial: `@lgili`
- Documentos relacionados: `Requirements.md`, `Architecture.md`, `Design_ADRs/`

Este roadmap deve ser atualizado continuamente conforme feedback e descobertas de implementação surgirem.
