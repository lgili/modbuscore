// SPDX-License-Identifier: Apache-2.0
//
// Public interface for the STM32 LL Modbus RTU transport that couples
// circular DMA reception with the UART IDLE-line interrupt.

#ifndef MODBUS_STM32_IDLE_H
#define MODBUS_STM32_IDLE_H

#include "modbus_amalgamated.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MODBUS_STM32_IDLE_RX_SIZE
#define MODBUS_STM32_IDLE_RX_SIZE 256U
#endif

#ifndef MODBUS_STM32_IDLE_DEFAULT_DATA_BITS
#define MODBUS_STM32_IDLE_DEFAULT_DATA_BITS 8U
#endif

#ifndef MODBUS_STM32_IDLE_DEFAULT_STOP_BITS
#define MODBUS_STM32_IDLE_DEFAULT_STOP_BITS 1U
#endif

typedef uint32_t (*modbus_stm32_idle_now_fn_t)(void *user_ctx);
typedef void (*modbus_stm32_idle_delay_fn_t)(uint32_t usec, void *user_ctx);
typedef void (*modbus_stm32_idle_direction_fn_t)(bool is_tx, void *user_ctx);

typedef struct {
    USART_TypeDef *uart;
    DMA_TypeDef *dma;
    uint32_t dma_channel;
    uint32_t silence_timeout_ms;
    uint32_t baudrate;              /**< UART baud rate used to derive guard times (set to zero to disable auto calculation). */
    uint8_t data_bits;              /**< Data bits per frame (defaults to 8 when zero). */
    bool parity_enabled;            /**< Set true to account for the parity bit in guard calculations. */
    uint8_t stop_bits;              /**< Stop bit count (1 or 2, defaults to 1 when zero). */
    uint32_t t15_guard_us;          /**< Optional override for T1.5 in microseconds (0 = derive from framing). */
    uint32_t t35_guard_us;          /**< Optional override for T3.5 in microseconds (0 = derive from framing). */
    modbus_stm32_idle_now_fn_t now_us;
    modbus_stm32_idle_delay_fn_t delay_us;
    modbus_stm32_idle_direction_fn_t set_direction;
    void *user_ctx;
} modbus_stm32_idle_config_t;

mb_err_t modbus_stm32_idle_init(const modbus_stm32_idle_config_t *cfg,
                                mb_client_txn_t *txn_pool,
                                mb_size_t txn_pool_len);

mb_err_t modbus_stm32_idle_poll(void);

mb_client_t *modbus_stm32_idle_client(void);

mb_err_t modbus_stm32_idle_submit_read_inputs(uint8_t unit_id,
                                              uint16_t addr,
                                              uint16_t count,
                                              const mb_embed_request_opts_t *opts,
                                              mb_client_txn_t **out_txn);

mb_err_t modbus_stm32_idle_submit_write_single(uint8_t unit_id,
                                               uint16_t addr,
                                               uint16_t value,
                                               const mb_embed_request_opts_t *opts,
                                               mb_client_txn_t **out_txn);

void modbus_stm32_idle_usart_isr(void);

void modbus_stm32_idle_dma_isr(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_STM32_IDLE_H */
