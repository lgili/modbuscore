bora arquitetar essa lib de Modbus em C como se fosse produto de mercado: modular, portátil, sem travar, com testes e docs. segue um plano incremental com “gates” objetivos pra só avançar quando estiver sólido.

Visão e princípios
	•	Portável: C11 “puro” (fallbacks para C99), sem malloc por padrão (pool opcional), zero dependências duras.
	•	Modular: separa PDU (codec), Transporte (RTU/TCP/ASCII), Core FSM (cliente/servidor), HAL/Port (UART/TCP, timers), Buffers/Queue e Log.
	•	À prova de travas: tudo não-bloqueante, com timeouts por estado, watchdog interno, fila de transações com retries e cancelamento.
	•	Segura: validação estrita de tamanhos e códigos, limites de PDU (≤ 253 bytes), CRC/MBAP, checagem de over/underflow, UB-safe.
	•	Observável: métricas, eventos, níveis de log, hooks de tracing.
	•	Testável: TDD, fuzzer nos decoders, sanitizers, cobertura e CI.

⸻

Arquitetura (camadas)

app (exemplos) ─────────────────────────────────────────────────────┐
                                                                    │
             ┌───────────── Core FSM ───────────────┐               │
             │  Cliente (master)  |  Servidor (slave)               │
             │  Transações, timeouts, retries, watchdog             │
             └──────────────────────────────────────┘               │
             ┌────────────  PDU Codec  ────────────┐                │
             │  encode/decode FCs, exceções, limites               │
             └──────────────────────────────────────┘               │
             ┌──────────  Abstração Transporte  ─────┐              │
             │  RTU | ASCII | TCP  (plug-ins)        │              │
             └───────────────────────────────────────┘              │
             ┌───────────  Port/HAL  ────────────────┐              │
             │  UART/TCP, timers, timestamp, mutex opcional         │
             └───────────────────────────────────────┘              │
             ┌────────── Util: Buffers/Queue/CRC/Log ─┐             │
             └─────────────────────────────────────────┘             │

	•	Core FSM: máquina de estados finita (poll-based) para Rx/Tx, per-transação; estados com deadline.
	•	PDU Codec: builders/parsers (FC03/04/06/16/…); exceções; tabelas de limites.
	•	Transporte: RTU (CRC/tempo de silêncio), TCP (MBAP/TID), ASCII (opcional).
	•	Port/HAL: ponteiros de função para read, write, now_ms, sleep(0), yield(), “ticks” e locks opcionais.
	•	Utilitários: ring buffer lock-free (SPSC), pool fixo, CRC16, MBAP helpers, logger.

⸻

Estrutura de repositório

/include/modbus/          # headers públicos
/src/core/                # fsm, transaction mgr
/src/pdu/                 # encoders/decoders
/src/transport/rtu/       # rtu
/src/transport/tcp/       # tcp
/src/port/                # port de referência (posix, freertos, bare)
/src/util/                # crc, ringbuf, pool, log
/examples/                # cliente/servidor
/tests/unit/              # ceedling ou cmocka
/tests/fuzz/              # libFuzzer/AFL (decoders)
/docs/                    # doxygen + sphinx (breathe)
/cmake/                   # toolchains, options


⸻

Roadmap incremental com Gates

Cada etapa tem Objetivo, Entregáveis e Gate (aceitação). Só segue se o gate passar.

0) Foundation & Build
	•	Objetivo: esqueleto do projeto, opções de build e estilo.
	•	Entregáveis: CMake + presets (gcc/clang/msvc), -Wall -Wextra -Werror, opções MODBUS_*, clang-format, Doxygen básico, CI (GitHub Actions) com matrix.
	•	Gate: CI verde (compila 3 toolchains), verificação de estilo, docs geradas.

1) Tipos, erros, log e utilitários
	•	Entregáveis: mb_types.h (tipos fixos), mb_err.h, logger MB_LOG(level, ...), ringbuf SPSC, CRC16.
	•	Gate: unit tests 100% em utils; ASan/UBSan sem findings.

2) PDU Codec (núcleo)
	•	Entregáveis: encode/decode FC 0x03, 0x06, 0x10 + exceções; limites (PDU ≤ 253).
	•	Gate: testes de tabela (vetores bons/ruins), cobertura ≥ 90%, fuzz 5 min sem crashes no decoder.

3) Abstração de Transporte
	•	Entregáveis: mb_transport_if.h (send/recv não-bloqueante), mb_frame.h.
	•	Gate: mock de transporte passando testes de integração com PDU.

4) RTU mínimo
	•	Entregáveis: framing RTU (addr, PDU, CRC), stateful parser com timeout de silêncio (3.5 chars), TX com inter-char.
	•	Gate: laço eco (loopback) em porta mock e UART simulada; teste de perda/duplicação de bytes.

5) Core FSM (cliente)
	•	Entregáveis: transaction manager (fila), estados Idle→Sending→Waiting→Done/Timeout/Retry/Abort, retries exponenciais, cancel.
	•	Gate: testes de stress (1k reqs), perda de pacotes simulada, sem deadlocks; watchdog interno nunca dispara em caso nominal.

6) Servidor (slave)
	•	Entregáveis: tabela de registradores com callbacks de leitura/escrita, checagem de faixa, exceções.
	•	Gate: conformidade básica (cliente ↔ servidor local) com FC 03/06/16; injeção de frames inválidos gera exceções corretas.

7) TCP (MBAP)
	•	Entregáveis: MBAP header (TID, length, unit), mapeamento TID↔transação, multi-conexão opcional.
	•	Gate: testes contra mbtget/modpoll (docker) no CI; throughput estável sob 1k rps (mock).

8) Robustez avançada
	•	Entregáveis: backpressure (limite de fila), anti-head-of-line (prioridades), timeouts por FC, poison pill para desbloqueio, métricas (contadores/latência).
	•	Gate: testes de caos (latência/burst/perda 20–40%), sem starvation, sem leak (valgrind).

9) Port/HALs
	•	Entregáveis: port_posix, port_freertos, port_bare (tick + UART/TCP), locks opcionais (C11 atomics fallback).
	•	Status: ✅ POSIX sockets, bare-metal tick adapter, FreeRTOS stream/queue shim e mutex portátil entregues.
	•	Gate: exemplos rodando em POSIX e FreeRTOS (simulado).

10) ASCII (opcional) + FCs extras
	•	Entregáveis: ASCII framing; FC 01/02/04/05/0F/17 se necessário.
	•	Status: 🔄 Helpers das FCs adicionados (req/resp validados); ASCII ainda pendente.
	•	Gate: mesma bateria de testes do RTU/TCP.

11) Observabilidade & Debug
	•	Entregáveis: eventos (state enter/leave), trace hex (on/off), callback de evento, contador por erro/FC, MB_DIAG.
	•	Gate: exemplo mostra métricas e diagnóstico útil em falhas induzidas.

12) Documentação & API Stability
	•	Entregáveis: guia de portas, guia de migração, cookbook (cliente/servidor), Doxygen completo, versão SemVer 1.0.0.
	•	Gate: build da doc sem warnings; exemplos compilam e rodam; revisão de breaking changes = zero.

13) Hardening & Release
	•	Entregáveis: clang-tidy perfil segurança, MISRA-C checklist (não estrito), -fno-common -fstack-protector, FORTIFY_SOURCE quando disponível.
	•	Gate: CI full (sanitizers + fuzz + cobertura + linters) verde; tag v1.0.0.
	•	Status: ✅ clang-tidy endurecido, hardening flags e jobs de cobertura/fuzz/lint na CI.

⸻

Estratégias de “à prova de travas” e perda de pacotes
	•	Poll não-bloqueante: mb_client_poll()/mb_server_poll() avança a FSM em passos pequenos.
	•	Timeouts por estado com deadlines (timestamp capturado na entrada do estado).
	•	Watchdog interno: se um estado excede N×timeout → aborta transação, incrementa métrica, volta a Idle.
	•	Retries exponenciais com jitter; limite por transação e por janela.
	•	Fila de transações com backpressure (tamanho fixo), cancelamento e prioridades.
	•	Ring buffer SPSC e pool de buffers (sem malloc), evitando fragmentação.
	•	Validações: limites de ADU/PDU, CRC/MBAP, endereços, contagens, alinhamento.
	•	Fail-fast: retorno de erro imediato e eventos de diagnóstico; nunca “engolir” erro silenciosamente.

⸻

Qualidade, testes e CI
	•	Unit: CMocka/Ceedling para utils, codec e FSM.
	•	Integração: cliente↔servidor in-process com transportes mock e RTU/TCP reais (docker modbus-tk, mbserver).
	•	Fuzzing: libFuzzer/AFL em decoders (PDU/RTU/TCP).
	•	Sanitizers: ASan/UBSan/TSan (quando aplicável).
	•	Cobertura: gcov/lcov (alvo ≥ 85%).
	•	Estática: clang-tidy, cppcheck (regras segurança), inclui MISRA subset.
	•	CI: matrix gcc/clang/msvc; builds release/debug/sanitize; artifacts das docs/exemplos.

⸻

Sketch de API (rascunho)

Comentários em inglês (como você costuma preferir nos códigos); texto em PT-BR.

/* include/modbus/modbus.h */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_PDU_MAX 253

typedef enum {
    MB_OK = 0,
    MB_EINVAL,
    MB_EBUSY,
    MB_EAGAIN,
    MB_ETIME,
    MB_ECRC,
    MB_EIO,
    MB_EPROTO,
    MB_ENOSPC,
    MB_ESTATE,
} mb_err_t;

/* Forward decls */
struct mb_port;
struct mb_client;
struct mb_server;

/* Timestamps in milliseconds */
typedef uint64_t mb_time_ms_t;

/* Transport-neutral send/recv (non-blocking) */
typedef struct {
    mb_err_t (*send)(void *ctx, const uint8_t *buf, size_t len);
    mb_err_t (*recv)(void *ctx, uint8_t *buf, size_t cap, size_t *out_len);
} mb_transport_if_t;

/* Port/HAL */
typedef struct mb_port {
    void *user;                          /* UART/TCP handle, etc. */
    mb_transport_if_t io;                /* send/recv */
    mb_time_ms_t (*now_ms)(void);        /* monotonic time */
    void (*yield)(void);                 /* optional: cpu hint */
} mb_port_t;

/* Client transaction handle */
typedef struct {
    uint16_t tid;        /* for TCP; 0 for RTU */
    uint8_t  unit_id;
    uint8_t  fc;         /* function code */
    const uint8_t *pdu;  /* request PDU payload (after FC) */
    size_t   pdu_len;
    uint32_t timeout_ms;
    uint8_t *resp_buf;   /* provided by caller or pool */
    size_t   resp_cap;
    size_t   resp_len;   /* filled on completion */
    mb_err_t status;
} mb_txn_t;

/* Creation / lifecycle */
mb_err_t mb_client_create(mb_client **out, const mb_port_t *port, size_t queue_depth);
mb_err_t mb_client_destroy(mb_client *c);

/* Non-blocking submit */
mb_err_t mb_client_submit(mb_client *c, mb_txn_t *txn);
/* Poll advances FSM; call from main loop or timer */
mb_err_t mb_client_poll(mb_client *c);

/* Helpers: common builders (FC 03/06/16) */
mb_err_t mb_build_read_holding(uint16_t addr, uint16_t qty, uint8_t *out_pdu, size_t *inout_len);
mb_err_t mb_parse_read_holding_resp(const uint8_t *pdu, size_t len, const uint8_t **out_bytes, size_t *out_len);

/* Server */
typedef mb_err_t (*mb_srv_read_cb)(uint16_t addr, uint16_t qty, uint8_t *out_bytes, size_t out_cap);
typedef mb_err_t (*mb_srv_write_cb)(uint16_t addr, const uint8_t *bytes, size_t len);

mb_err_t mb_server_create(mb_server **out, const mb_port_t *port);
mb_err_t mb_server_set_callbacks(mb_server *s, mb_srv_read_cb rd, mb_srv_write_cb wr);
mb_err_t mb_server_poll(mb_server *s);

#ifdef __cplusplus
}
#endif

Exemplo mínimo (cliente RTU) — não-bloqueante:

/* loop de aplicação */
mb_client *cli; mb_port_t port = my_rtu_port(); 
mb_client_create(&cli, &port, /*queue*/8);

uint8_t pdu[5]; size_t pdu_len = sizeof pdu;
mb_build_read_holding(0x0000, 2, pdu, &pdu_len);

uint8_t resp[64];
mb_txn_t t = {
  .unit_id = 1, .fc = 0x03, .pdu = pdu, .pdu_len = pdu_len,
  .timeout_ms = 200, .resp_buf = resp, .resp_cap = sizeof resp
};
mb_client_submit(cli, &t);

for (;;) {
  mb_client_poll(cli);
  if (t.status == MB_OK && t.resp_len) {
    const uint8_t *bytes; size_t n;
    mb_parse_read_holding_resp(t.resp_buf+1, t.resp_len-1, &bytes, &n);
    /* processa bytes... */
  }
  app_do_other_work();
}


⸻

Documentação
	•	Doxygen para API + Sphinx (Breathe) para guias (Port/HAL, cookbook, troubleshooting).
	•	Exemplos: cliente RTU/TCP; servidor RTU/TCP; POSIX e FreeRTOS.
	•	CHANGELOG (SemVer), CONTRIBUTING, SECURITY.md, Design.md (FSM + diagramas).

⸻

Próximas 2 semanas (sugestão de sprints)
	1.	Dia 1–3: Etapas 0–1 (foundation, utils, log) + CI.
	2.	Dia 4–7: Etapa 2–3 (PDU codec + transporte mock) com TDD e fuzz básico.
	3.	Dia 8–10: Etapa 4–5 (RTU + FSM cliente) + testes de caos.
	4.	Dia 11–14: Etapa 6 (servidor) + docs iniciais e exemplos.

⸻

Extras que valem ouro
	•	Config Kconfig/cmake-options para habilitar/desabilitar FCs e transportes.
	•	Modo amalgamado (um .c + .h) para quem quiser integrar rápido.
	•	Hooks de auditoria (ex.: antes de enviar/depois de receber) para trace/pcap.
	•	Gerador de stubs (script) para FCs customizados.

⸻