# Guia Rápido – Implementando um Transporte Custom

O runtime ModbusCore injeta todo o I/O por meio de `mbc_transport_iface_t`. Para prover um transporte próprio (serial proprietário, BLE, mocks), basta preencher as callbacks abaixo:

```c
#include <modbuscore/transport/iface.h>

typedef struct {
    my_uart_handle_t uart;
} my_uart_transport_t;

static mbc_status_t uart_send(void *ctx,
                              const uint8_t *buffer,
                              size_t length,
                              mbc_transport_io_t *out)
{
    my_uart_transport_t *t = ctx;
    ssize_t written = my_uart_write(&t->uart, buffer, length);
    if (written < 0) {
        return MBC_STATUS_IO_ERROR;
    }
    if (out) {
        out->processed = (size_t)written;
    }
    return MBC_STATUS_OK;
}

static mbc_status_t uart_recv(void *ctx,
                              uint8_t *buffer,
                              size_t capacity,
                              mbc_transport_io_t *out)
{
    my_uart_transport_t *t = ctx;
    ssize_t read = my_uart_read(&t->uart, buffer, capacity);
    if (read < 0) {
        return MBC_STATUS_IO_ERROR;
    }
    if (out) {
        out->processed = (size_t)read;
    }
    return MBC_STATUS_OK;
}

static uint64_t uart_now(void *ctx)
{
    (void)ctx;
    return platform_millis();
}

static void uart_yield(void *ctx)
{
    (void)ctx;
    platform_yield();
}
\n```

Depois, injete no builder:

```c
my_uart_transport_t hw = {.uart = my_uart_open(...)};\nmbc_transport_iface_t iface = {\n    .ctx = &hw,\n    .send = uart_send,\n    .receive = uart_recv,\n    .now = uart_now,        /* opcional, mas recomendado */\n    .yield = uart_yield,    /* opcional */\n};\n\nmbc_runtime_builder_t builder;\nmbc_runtime_builder_init(&builder);\nmbc_runtime_builder_with_transport(&builder, &iface);\nmbc_runtime_t runtime;\nmbc_runtime_builder_build(&builder, &runtime);\n```\n\n> **Dicas**\n> - Preencha `out->processed` para permitir que o core acompanhe bytes efetivos.\n> - No modo cooperativo, implemente `yield` para liberar CPU enquanto aguarda I/O.\n> - Utilize `MBC_STATUS_IO_ERROR` em falhas permanentes e `MBC_STATUS_OK` com `processed = 0` para timeouts ou falta de dados.\n*** End Patch
