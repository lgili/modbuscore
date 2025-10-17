#ifndef MODBUSCORE_COMMON_STATUS_H
#define MODBUSCORE_COMMON_STATUS_H

/**
 * @file status.h
 * @brief Canonical status codes shared across the ModbusCore3 runtime.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mbc_status {
    MBC_STATUS_OK = 0,
    MBC_STATUS_INVALID_ARGUMENT = -1,
    MBC_STATUS_ALREADY_INITIALISED = -2,
    MBC_STATUS_NOT_INITIALISED = -3,
    MBC_STATUS_UNSUPPORTED = -4,
    MBC_STATUS_IO_ERROR = -5,
    MBC_STATUS_BUSY = -6,
    MBC_STATUS_NO_RESOURCES = -7,
    MBC_STATUS_DECODING_ERROR = -8,
    MBC_STATUS_TIMEOUT = -9
} mbc_status_t;

static inline int mbc_status_is_ok(mbc_status_t status)
{
    return status == MBC_STATUS_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_COMMON_STATUS_H */
