/**
 * @file mutex.h
 * @brief Optional synchronisation primitives for Modbus ports.
 */

#ifndef MODBUS_PORT_MUTEX_H
#define MODBUS_PORT_MUTEX_H

#include <stdbool.h>

#if !defined(__STDC_NO_THREADS__)
#define MB_PORT_HAS_C11_THREADS 1
#include <threads.h>
#else
#define MB_PORT_HAS_C11_THREADS 0
#endif

#if MB_PORT_HAS_C11_THREADS == 0
#if defined(__cplusplus)
#include <atomic>
typedef std::atomic_flag mb_port_atomic_flag;
#else
#include <stdatomic.h>
typedef atomic_flag mb_port_atomic_flag;
#endif
#endif

#include <modbus/mb_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque mutex structure with C11 threads fallback.
 */
typedef struct mb_port_mutex {
#if MB_PORT_HAS_C11_THREADS
    mtx_t handle;
    bool initialised;
#else
    mb_port_atomic_flag flag;
#endif
} mb_port_mutex_t;

/**
 * @brief Initialises a mutex object.
 *
 * @retval MB_OK                 Mutex initialised successfully.
 * @retval MB_ERR_INVALID_ARGUMENT @p mutex was NULL.
 * @retval MB_ERR_OTHER          Underlying platform call failed.
 */
mb_err_t mb_port_mutex_init(mb_port_mutex_t *mutex);

/**
 * @brief Releases resources held by the mutex.
 *
 * No-op when the mutex has not been initialised.
 */
void mb_port_mutex_deinit(mb_port_mutex_t *mutex);

/**
 * @brief Attempts to acquire the mutex, blocking/spinning until success.
 *
 * @retval MB_OK                 Mutex acquired successfully.
 * @retval MB_ERR_INVALID_ARGUMENT @p mutex was NULL.
 * @retval MB_ERR_OTHER          Mutex not initialised or platform error.
 */
mb_err_t mb_port_mutex_lock(mb_port_mutex_t *mutex);

/**
 * @brief Releases the mutex.
 */
void mb_port_mutex_unlock(mb_port_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PORT_MUTEX_H */
