#ifndef MODBUSCORE_TRANSPORT_RTU_UART_H
#define MODBUSCORE_TRANSPORT_RTU_UART_H

/**
 * @file rtu_uart.h
 * @brief RTU UART transport with hardware abstraction layer.
 *
 * This driver provides a portable RTU transport that delegates low-level UART
 * operations to a user-provided backend. It handles RTU framing, timing guards,
 * and buffering while remaining independent of specific hardware or OS APIs.
 */

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hardware abstraction layer for UART operations.
 */
typedef struct mbc_rtu_uart_backend {
    void* ctx;                                                      /**< User context */
    size_t (*write)(void* ctx, const uint8_t* data, size_t length); /**< Write data to UART */
    size_t (*read)(void* ctx, uint8_t* data, size_t capacity);      /**< Read data from UART */
    void (*flush)(void* ctx);                                       /**< Flush TX buffer */
    uint64_t (*now_us)(void* ctx);                /**< Get current time in microseconds */
    void (*delay_us)(void* ctx, uint32_t micros); /**< Blocking delay in microseconds */
} mbc_rtu_uart_backend_t;

/**
 * @brief RTU UART configuration.
 */
typedef struct mbc_rtu_uart_config {
    mbc_rtu_uart_backend_t backend; /**< Hardware backend callbacks */
    uint32_t baud_rate;             /**< Baud rate (e.g., 9600, 19200) */
    uint8_t data_bits;              /**< Data bits (7 or 8) */
    uint8_t parity_bits;            /**< Parity bits (0=none, 1=odd, 2=even) */
    uint8_t stop_bits;              /**< Stop bits (1 or 2) */
    uint32_t guard_time_us;         /**< Inter-frame guard time in microseconds */
    size_t rx_buffer_capacity;      /**< RX buffer capacity */
} mbc_rtu_uart_config_t;

/**
 * @brief Opaque RTU UART context.
 */
typedef struct mbc_rtu_uart_ctx mbc_rtu_uart_ctx_t;

/**
 * @brief Create RTU UART transport instance.
 *
 * @param config Configuration (must not be NULL)
 * @param out_iface Transport interface filled on output
 * @param out_ctx Allocated context (use destroy when done)
 * @return MBC_STATUS_OK on success or error code
 */
mbc_status_t mbc_rtu_uart_create(const mbc_rtu_uart_config_t* config,
                                 mbc_transport_iface_t* out_iface, mbc_rtu_uart_ctx_t** out_ctx);

/**
 * @brief Destroy RTU UART transport, releasing resources.
 */
void mbc_rtu_uart_destroy(mbc_rtu_uart_ctx_t* ctx);

/**
 * @brief Reset RTU UART transport to initial state.
 */
void mbc_rtu_uart_reset(mbc_rtu_uart_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_RTU_UART_H */
