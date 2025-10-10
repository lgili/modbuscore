/**
 * @file mapping.h
 * @brief Convenience helpers for wiring register storage into the Modbus server.
 *
 * Gate 15 introduces libmodbus-style mapping helpers so applications can
 * bootstrap the server runtime with a single descriptor instead of manually
 * chaining @ref mb_server_init and @ref mb_server_add_storage calls.
 */

#ifndef MODBUS_MAPPING_H
#define MODBUS_MAPPING_H

#include <modbus/conf.h>

#if MB_CONF_BUILD_SERVER

#include <stdbool.h>

#include <modbus/mb_err.h>
#include <modbus/server.h>
#include <modbus/transport_if.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Describes a contiguous bank of holding registers served by the
 *        built-in storage helpers.
 */
typedef struct {
    mb_u16 start;      /**< First register address served by the bank. */
    mb_u16 count;      /**< Number of registers exposed by the bank. */
    mb_u16 *storage;   /**< Backing storage (``count`` elements). */
    bool read_only;    /**< Reject write requests when set. */
} mb_server_mapping_bank_t;

/**
 * @brief Binds server storage, region descriptors and request pool in one step.
 */
typedef struct {
    const mb_transport_if_t *iface;      /**< Transport adopted by the server. */
    mb_u8 unit_id;                       /**< Modbus unit identifier served. */
    mb_server_region_t *regions;         /**< Region descriptor array. */
    mb_size_t region_capacity;           /**< Number of entries in ``regions``. */
    mb_server_request_t *request_pool;   /**< Request descriptor pool. */
    mb_size_t request_capacity;          /**< Number of entries in ``request_pool``. */
    const mb_server_mapping_bank_t *banks; /**< Register banks to register (may be ``NULL``). */
    mb_size_t bank_count;                /**< Number of elements in ``banks``. */
} mb_server_mapping_config_t;

/**
 * @brief Registers the provided banks on an already initialised server.
 *
 * Storage-backed regions are inserted in the order they appear in ``banks``.
 * Entries with ``count == 0`` are ignored. When ``storage`` is ``NULL`` the
 * function returns ::MB_ERR_INVALID_ARGUMENT.
 */
mb_err_t mb_server_mapping_apply(mb_server_t *server,
                                 const mb_server_mapping_bank_t *banks,
                                 mb_size_t bank_count);

/**
 * @brief Convenience wrapper that bundles server initialisation and storage registration.
 */
mb_err_t mb_server_mapping_init(mb_server_t *server,
                                const mb_server_mapping_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* MB_CONF_BUILD_SERVER */

#endif /* MODBUS_MAPPING_H */
