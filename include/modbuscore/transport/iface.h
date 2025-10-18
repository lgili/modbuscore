#ifndef MODBUSCORE_TRANSPORT_IFACE_H
#define MODBUSCORE_TRANSPORT_IFACE_H

/**
 * @file iface.h
 * @brief Non-blocking transport contract used by the runtime.
 *
 * The goal is to allow clients/servers to operate on different media
 * (TCP, RTU, mocks) simply by injecting callbacks. All operations return
 * `mbc_status_t` and respect step budgets for integration with cooperative
 * event loops.
 */

#include <stddef.h>
#include <stdint.h>

#include <modbuscore/runtime/dependencies.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send data through transport interface.
 *
 * @param iface Transport interface
 * @param buffer Data to send
 * @param length Number of bytes to send
 * @param out I/O result (can be NULL)
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_transport_send(const mbc_transport_iface_t *iface,
                                const uint8_t *buffer,
                                size_t length,
                                mbc_transport_io_t *out);

/**
 * @brief Receive data from transport interface.
 *
 * @param iface Transport interface
 * @param buffer Buffer to receive data into
 * @param capacity Buffer capacity
 * @param out I/O result (can be NULL)
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_transport_receive(const mbc_transport_iface_t *iface,
                                   uint8_t *buffer,
                                   size_t capacity,
                                   mbc_transport_io_t *out);

/**
 * @brief Get current timestamp from transport.
 *
 * @param iface Transport interface
 * @return Current time in milliseconds
 */
uint64_t mbc_transport_now(const mbc_transport_iface_t *iface);

/**
 * @brief Cooperative yield (optional operation).
 *
 * @param iface Transport interface
 */
void mbc_transport_yield(const mbc_transport_iface_t *iface);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_IFACE_H */
