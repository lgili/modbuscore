#ifndef MODBUSCORE_TRANSPORT_WIN32_RTU_H
#define MODBUSCORE_TRANSPORT_WIN32_RTU_H

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mbc_win32_rtu_config {
    const char* port_name;     /**< Nome da porta (ex: "COM3"). */
    uint32_t baud_rate;        /**< Baud rate (ex: 9600). */
    uint8_t data_bits;         /**< Bits de dados (5-8, default 8). */
    char parity;               /**< 'N', 'E', 'O' (default 'N'). */
    uint8_t stop_bits;         /**< Bits de stop (1 ou 2, default 1). */
    uint32_t guard_time_us;    /**< Override opcional do guard-time (0 = auto). */
    size_t rx_buffer_capacity; /**< Capacidade do buffer interno (default 256). */
} mbc_win32_rtu_config_t;

typedef struct mbc_win32_rtu_ctx mbc_win32_rtu_ctx_t;

mbc_status_t mbc_win32_rtu_create(const mbc_win32_rtu_config_t* config,
                                  mbc_transport_iface_t* out_iface, mbc_win32_rtu_ctx_t** out_ctx);

void mbc_win32_rtu_destroy(mbc_win32_rtu_ctx_t* ctx);

void mbc_win32_rtu_reset(mbc_win32_rtu_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_WIN32_RTU_H */
