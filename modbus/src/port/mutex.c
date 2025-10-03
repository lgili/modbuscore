#include <modbus/port/mutex.h>

#if !MB_PORT_HAS_C11_THREADS && defined(__cplusplus)
#include <atomic>
using std::memory_order_acquire;
using std::memory_order_release;
#endif

mb_err_t mb_port_mutex_init(mb_port_mutex_t *mutex)
{
    if (mutex == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

#if MB_PORT_HAS_C11_THREADS
    if (mtx_init(&mutex->handle, mtx_plain) != thrd_success) {
        mutex->initialised = false;
    return MB_ERR_OTHER;
    }
    mutex->initialised = true;
    return MB_OK;
#else
#if defined(__cplusplus)
    mutex->flag.clear(memory_order_release);
#else
    atomic_flag_clear(&mutex->flag);
#endif
    return MB_OK;
#endif
}

void mb_port_mutex_deinit(mb_port_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }
#if MB_PORT_HAS_C11_THREADS
    if (mutex->initialised) {
        mtx_destroy(&mutex->handle);
        mutex->initialised = false;
    }
#else
#if defined(__cplusplus)
    mutex->flag.clear(memory_order_release);
#else
    atomic_flag_clear(&mutex->flag);
#endif
#endif
}

mb_err_t mb_port_mutex_lock(mb_port_mutex_t *mutex)
{
    if (mutex == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }
#if MB_PORT_HAS_C11_THREADS
    if (!mutex->initialised) {
    return MB_ERR_OTHER;
    }
    return (mtx_lock(&mutex->handle) == thrd_success) ? MB_OK : MB_ERR_OTHER;
#else
#if defined(__cplusplus)
    while (mutex->flag.test_and_set(memory_order_acquire)) {
#else
    while (atomic_flag_test_and_set_explicit(&mutex->flag, memory_order_acquire)) {
#endif
        /* busy spin */
    }
    return MB_OK;
#endif
}

void mb_port_mutex_unlock(mb_port_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }
#if MB_PORT_HAS_C11_THREADS
    if (!mutex->initialised) {
        return;
    }
    (void)mtx_unlock(&mutex->handle);
#else
#if defined(__cplusplus)
    mutex->flag.clear(memory_order_release);
#else
    atomic_flag_clear_explicit(&mutex->flag, memory_order_release);
#endif
#endif
}
