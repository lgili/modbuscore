#ifndef MODBUSCORE_TRANSPORT_WIN32_RTU_H
#define MODBUSCORE_TRANSPORT_WIN32_RTU_H

/**
 * @file win32_rtu.h
 * @brief Win32 RTU serial transport driver.
 *
 * This driver provides RTU transport over Windows serial devices using Win32 API.
 * It handles device configuration, overlapped I/O, and timing guards for proper
 * RTU frame spacing.
 */

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Win32 RTU configuration.
 */
typedef struct mbc_win32_rtu_config {
    const char* port_name;     /**< Port name (e.g., "COM3"). */
    uint32_t baud_rate;        /**< Baud rate (e.g., 9600). */
    uint8_t data_bits;         /**< Data bits (5-8, default 8). */
    char parity;               /**< 'N', 'E', 'O' (default 'N'). */
    uint8_t stop_bits;         /**< Stop bits (1 or 2, default 1). */
    uint32_t guard_time_us;    /**< Optional guard time override (0 = auto). */
    size_t rx_buffer_capacity; /**< Internal buffer capacity (default 256). */
} mbc_win32_rtu_config_t;

/**
 * @brief Opaque Win32 RTU context.
 */
typedef struct mbc_win32_rtu_ctx mbc_win32_rtu_ctx_t;

/**
 * @brief Create Win32 RTU transport instance.
 *
 * @param config Configuration (must not be NULL)
 * @param out_iface Transport interface filled on output
 * @param out_ctx Allocated context (use destroy when done)
 * @return MBC_STATUS_OK on success or error code
 */
mbc_status_t mbc_win32_rtu_create(const mbc_win32_rtu_config_t* config,
                                  mbc_transport_iface_t* out_iface, mbc_win32_rtu_ctx_t** out_ctx);

/**
 * @brief Destroy Win32 RTU transport, releasing resources.
 */
void mbc_win32_rtu_destroy(mbc_win32_rtu_ctx_t* ctx);

/**
 * @brief Reset Win32 RTU transport to initial state.
 */
void mbc_win32_rtu_reset(mbc_win32_rtu_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_WIN32_RTU_H */
