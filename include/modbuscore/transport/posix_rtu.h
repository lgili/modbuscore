#ifndef MODBUSCORE_TRANSPORT_POSIX_RTU_H
#define MODBUSCORE_TRANSPORT_POSIX_RTU_H

/**
 * @file posix_rtu.h
 * @brief POSIX RTU serial transport driver.
 *
 * This driver provides RTU transport over POSIX serial devices (termios).
 * It handles device configuration, non-blocking I/O, and timing guards
 * for proper RTU frame spacing.
 */

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief POSIX RTU configuration.
 */
typedef struct mbc_posix_rtu_config {
    const char* device_path;   /**< Device path (e.g., "/dev/ttyUSB0"). */
    uint32_t baud_rate;        /**< Baud rate (e.g., 9600). */
    uint8_t data_bits;         /**< Data bits (5-8, default 8). */
    char parity;               /**< 'N', 'E', or 'O' (default 'N'). */
    uint8_t stop_bits;         /**< Stop bits (1 or 2, default 1). */
    uint32_t guard_time_us;    /**< Guard time override (0 = auto). */
    size_t rx_buffer_capacity; /**< Internal buffer capacity (default 256). */
} mbc_posix_rtu_config_t;

/**
 * @brief Opaque POSIX RTU context.
 */
typedef struct mbc_posix_rtu_ctx mbc_posix_rtu_ctx_t;

/**
 * @brief Create POSIX RTU transport instance.
 *
 * @param config Configuration (must not be NULL)
 * @param out_iface Transport interface filled on output
 * @param out_ctx Allocated context (use destroy when done)
 * @return MBC_STATUS_OK on success or error code
 */
mbc_status_t mbc_posix_rtu_create(const mbc_posix_rtu_config_t* config,
                                  mbc_transport_iface_t* out_iface, mbc_posix_rtu_ctx_t** out_ctx);

/**
 * @brief Destroy POSIX RTU transport, releasing resources.
 */
void mbc_posix_rtu_destroy(mbc_posix_rtu_ctx_t* ctx);

/**
 * @brief Reset POSIX RTU transport to initial state.
 */
void mbc_posix_rtu_reset(mbc_posix_rtu_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_POSIX_RTU_H */
