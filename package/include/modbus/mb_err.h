/**
 * @file mb_err.h
 * @brief Central repository for Modbus error codes and helpers.
 *
 * This header consolidates the error enumeration and related utilities that
 * were previously scattered across legacy headers, providing a single source of
 * truth for status codes shared by the client, server and transport layers.
 */

#ifndef MODBUS_MB_ERR_H
#define MODBUS_MB_ERR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Unified Modbus error / status codes.
 *
 * The values preserve the legacy names used throughout the codebase so the
 * existing implementation keeps compiling while the headers get reorganized.
 */
typedef enum modbus_error {
    MODBUS_ERROR_NONE = 0,                /**< No error. */
    MODBUS_ERROR_INVALID_ARGUMENT = -1,   /**< Invalid argument provided. */
    MODBUS_ERROR_TIMEOUT = -2,            /**< Read/write timeout occurred. */
    MODBUS_ERROR_TRANSPORT = -3,          /**< Transport layer error. */
    MODBUS_ERROR_CRC = -4,                /**< CRC check failed. */
    MODBUS_ERROR_INVALID_REQUEST = -5,    /**< Received invalid request frame. */
    MODBUS_ERROR_OTHER_REQUESTS = -6,     /**< Received other types of requests. */
    MODBUS_OTHERS_REQUESTS = -7,          /**< Placeholder for additional request types. */
    MODBUS_ERROR_OTHER = -8,              /**< Other unspecified error. */
    MODBUS_ERROR_CANCELLED = -9,          /**< Operation was cancelled. */
    MODBUS_ERROR_NO_RESOURCES = -10,      /**< Requested resource could not be reserved. */
    MODBUS_ERROR_BUSY = -11,              /**< Resource busy (queue full, TX in progress, etc.). */

    /* Modbus exceptions (positive values) */
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 1,            /**< Exception 1: Illegal function. */
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,        /**< Exception 2: Illegal data address. */
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE = 3,          /**< Exception 3: Illegal data value. */
    MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE = 4,       /**< Exception 4: Server device failure. */
    MODBUS_EXCEPTION_ACKNOWLEDGE = 5,                 /**< Exception 5: Acknowledge (processing). */
    MODBUS_EXCEPTION_SERVER_DEVICE_BUSY = 6,          /**< Exception 6: Server device busy. */
    MODBUS_EXCEPTION_NEGATIVE_ACKNOWLEDGE = 7,        /**< Exception 7: Negative acknowledge. */
    MODBUS_EXCEPTION_MEMORY_PARITY_ERROR = 8,         /**< Exception 8: Memory parity error. */
    MODBUS_EXCEPTION_GATEWAY_PATH_UNAVAILABLE = 10,   /**< Exception 10: Gateway path unavailable. */
    MODBUS_EXCEPTION_GATEWAY_TARGET_DEVICE_FAILED = 11/**< Exception 11: Target device failed to respond. */
} modbus_error_t;

/**
 * @brief Convenience alias mirroring the historical ``modbus_error_t`` name.
 */
typedef modbus_error_t mb_err_t;

/*
 * Windows SDK (winuser.h) defines MB_OK for MessageBox return values.
 * We need to undefine it and redefine for Modbus use.
 * 
 * Note: This is safe because:
 * 1. We only use MB_OK in Modbus context (error codes)
 * 2. Windows MessageBox APIs use the full Windows types (int, not mb_err_t)
 * 3. No code should be mixing MessageBox returns with Modbus error codes
 */
#ifdef MB_OK
#undef MB_OK
#endif

/* Define MB_OK for Modbus and keep MSVC quiet when Windows headers redefine it. */
#if defined(_MSC_VER)
#pragma warning(disable: 4005) /* macro redefinition */
#endif

#define MB_OK                       MODBUS_ERROR_NONE
#define MB_ERR_INVALID_ARGUMENT     MODBUS_ERROR_INVALID_ARGUMENT
#define MB_ERR_TIMEOUT              MODBUS_ERROR_TIMEOUT
#define MB_ERR_TRANSPORT            MODBUS_ERROR_TRANSPORT
#define MB_ERR_CRC                  MODBUS_ERROR_CRC
#define MB_ERR_INVALID_REQUEST      MODBUS_ERROR_INVALID_REQUEST
#define MB_ERR_OTHER_REQUESTS       MODBUS_ERROR_OTHER_REQUESTS
#define MB_ERR_OTHER                MODBUS_ERROR_OTHER
#define MB_ERR_CANCELLED            MODBUS_ERROR_CANCELLED
#define MB_ERR_NO_RESOURCES         MODBUS_ERROR_NO_RESOURCES
#define MB_ERR_BUSY                 MODBUS_ERROR_BUSY

#define MB_EX_ILLEGAL_FUNCTION      MODBUS_EXCEPTION_ILLEGAL_FUNCTION
#define MB_EX_ILLEGAL_DATA_ADDRESS  MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS
#define MB_EX_ILLEGAL_DATA_VALUE    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE
#define MB_EX_SERVER_DEVICE_FAILURE MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE
#define MB_EX_ACKNOWLEDGE           MODBUS_EXCEPTION_ACKNOWLEDGE
#define MB_EX_SERVER_DEVICE_BUSY    MODBUS_EXCEPTION_SERVER_DEVICE_BUSY
#define MB_EX_NEGATIVE_ACKNOWLEDGE  MODBUS_EXCEPTION_NEGATIVE_ACKNOWLEDGE
#define MB_EX_MEMORY_PARITY_ERROR   MODBUS_EXCEPTION_MEMORY_PARITY_ERROR
#define MB_EX_GATEWAY_PATH_UNAVAILABLE MODBUS_EXCEPTION_GATEWAY_PATH_UNAVAILABLE
#define MB_EX_GATEWAY_TARGET_FAILED MODBUS_EXCEPTION_GATEWAY_TARGET_DEVICE_FAILED

static inline bool mb_err_is_ok(mb_err_t err)
{
    return err == MB_OK;
}

const char *mb_err_str(mb_err_t err);

/**
 * @brief Determines if the given error code represents a Modbus exception.
 *
 * @param err Error code to inspect.
 * @return `true` when the code is a protocol exception (1–4), `false` otherwise.
 */
static inline bool mb_err_is_exception(modbus_error_t err)
{
    return (err == MODBUS_EXCEPTION_ILLEGAL_FUNCTION) ||
           (err == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS) ||
           (err == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE) ||
           (err == MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE) ||
           (err == MODBUS_EXCEPTION_ACKNOWLEDGE) ||
           (err == MODBUS_EXCEPTION_SERVER_DEVICE_BUSY) ||
           (err == MODBUS_EXCEPTION_NEGATIVE_ACKNOWLEDGE) ||
           (err == MODBUS_EXCEPTION_MEMORY_PARITY_ERROR) ||
           (err == MODBUS_EXCEPTION_GATEWAY_PATH_UNAVAILABLE) ||
           (err == MODBUS_EXCEPTION_GATEWAY_TARGET_DEVICE_FAILED);
}

/* Backward-compatible helpers ------------------------------------------------ */

static inline bool modbus_error_is_exception(modbus_error_t err)
{
    return mb_err_is_exception(err);
}

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_ERR_H */
