#ifndef MODBUSCORE_TRANSPORT_RTU_UART_H
#define MODBUSCORE_TRANSPORT_RTU_UART_H

#include <stddef.h>
#include <stdint.h>

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mbc_rtu_uart_backend {
    void *ctx;
    size_t (*write)(void *ctx, const uint8_t *data, size_t length);
    size_t (*read)(void *ctx, uint8_t *data, size_t capacity);
    void (*flush)(void *ctx);
    uint64_t (*now_us)(void *ctx);
    void (*delay_us)(void *ctx, uint32_t micros);
} mbc_rtu_uart_backend_t;

typedef struct mbc_rtu_uart_config {
    mbc_rtu_uart_backend_t backend;
    uint32_t baud_rate;
    uint8_t data_bits;
    uint8_t parity_bits;
    uint8_t stop_bits;
    uint32_t guard_time_us;
    size_t rx_buffer_capacity;
} mbc_rtu_uart_config_t;

typedef struct mbc_rtu_uart_ctx mbc_rtu_uart_ctx_t;

mbc_status_t mbc_rtu_uart_create(const mbc_rtu_uart_config_t *config,
                                 mbc_transport_iface_t *out_iface,
                                 mbc_rtu_uart_ctx_t **out_ctx);

void mbc_rtu_uart_destroy(mbc_rtu_uart_ctx_t *ctx);

void mbc_rtu_uart_reset(mbc_rtu_uart_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_RTU_UART_H */
