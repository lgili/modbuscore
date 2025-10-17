# ModbusCore 3.0 (WIP)

Nova implementação da biblioteca Modbus com arquitetura pensada desde o início para:

- **API única** em qualquer cenário (desktop, embedded, servidor).
- **Injeção de dependências** em todas as camadas (transporte, clock, alocador, logger).
- **Testabilidade total** – tudo mockável sem hacks.
- **Perfis declarativos** e DX moderna.

## Estado Atual

- Estrutura inicial criada do zero.
- Runtime básico com DI e teste smoke (`ctest`).
- Código legado arquivado em `bak/` para referência histórica.

## Como Compilar

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Exemplo rápido (bootstrap com Builder)

```c
#include <modbuscore/runtime/builder.h>

static mbc_status_t send_stub(void *ctx, const uint8_t *buf, size_t len) {
    (void)ctx; (void)buf; (void)len; return MBC_STATUS_OK;
}

static mbc_status_t recv_stub(void *ctx, uint8_t *buf, size_t cap, size_t *out_len) {
    (void)ctx; (void)buf; (void)cap; if (out_len) *out_len = 0; return MBC_STATUS_OK;
}

int main(void) {
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);

    mbc_transport_iface_t transport = {
        .ctx = NULL,
        .send = send_stub,
        .receive = recv_stub,
    };

    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime = {0};
    mbc_status_t status = mbc_runtime_builder_build(&builder, &runtime);
    /* status == MBC_STATUS_OK e runtime possui clock/alocador/logger padrão */
    mbc_runtime_shutdown(&runtime);
    return status;
}
```

Consulte `docs/transport_custom.md` para ver como plugar um transporte próprio.
Veja também `docs/protocol/fsm_overview.md` para entender o desenho atual da FSM/PDU.

## Próximos Passos

1. Evoluir o runtime para validar dependências opcionais e builders.
2. Definir interfaces completas de transporte, logger, métricas.
3. Começar a mover regras de FSM/Protocolo para o novo core.

Consulte `roadmap.md` para o plano completo fase a fase.
