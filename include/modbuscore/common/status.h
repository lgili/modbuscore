#ifndef MODBUSCORE_COMMON_STATUS_H
#define MODBUSCORE_COMMON_STATUS_H

/**
 * @file status.h
 * @brief Canonical status codes shared across the ModbusCore v3.0 runtime.
 *
 * All library functions that can fail return a mbc_status_t code.
 * Use mbc_status_is_ok() to check for success.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status codes for ModbusCore operations.
 */
typedef enum mbc_status {
    MBC_STATUS_OK = 0,                      /**< Operation completed successfully */
    MBC_STATUS_INVALID_ARGUMENT = -1,       /**< Invalid function argument */
    MBC_STATUS_ALREADY_INITIALISED = -2,    /**< Component already initialized */
    MBC_STATUS_NOT_INITIALISED = -3,        /**< Component not initialized */
    MBC_STATUS_UNSUPPORTED = -4,            /**< Unsupported operation or function code */
    MBC_STATUS_IO_ERROR = -5,               /**< I/O error (network, serial, etc.) */
    MBC_STATUS_BUSY = -6,                   /**< Resource is busy */
    MBC_STATUS_NO_RESOURCES = -7,           /**< Insufficient resources (memory, buffer space) */
    MBC_STATUS_DECODING_ERROR = -8,         /**< Frame/PDU decoding error */
    MBC_STATUS_TIMEOUT = -9                 /**< Operation timeout */
} mbc_status_t;

/**
 * @brief Check if status indicates success.
 *
 * @param status Status code to check
 * @return 1 if status is MBC_STATUS_OK, 0 otherwise
 */
static inline int mbc_status_is_ok(mbc_status_t status)
{
    return status == MBC_STATUS_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_COMMON_STATUS_H */
