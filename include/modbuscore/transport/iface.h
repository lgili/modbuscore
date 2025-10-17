#ifndef MODBUSCORE_TRANSPORT_IFACE_H
#define MODBUSCORE_TRANSPORT_IFACE_H

/**
 * @file iface.h
 * @brief Contrato de transporte não-bloqueante utilizado pelo runtime.
 *
 * O objetivo é permitir que clientes/servidores operem em diferentes meios
 * (TCP, RTU, mocks) apenas injetando callbacks. Todas as operações retornam
 * `mbc_status_t` e respeitam orçamentos de passos para integração com loop
 * cooperativo.
 */

#include <stddef.h>
#include <stdint.h>

#include <modbuscore/runtime/dependencies.h>

#ifdef __cplusplus
extern "C" {
#endif

mbc_status_t mbc_transport_send(const mbc_transport_iface_t *iface,
                                const uint8_t *buffer,
                                size_t length,
                                mbc_transport_io_t *out);

mbc_status_t mbc_transport_receive(const mbc_transport_iface_t *iface,
                                   uint8_t *buffer,
                                   size_t capacity,
                                   mbc_transport_io_t *out);

uint64_t mbc_transport_now(const mbc_transport_iface_t *iface);

void mbc_transport_yield(const mbc_transport_iface_t *iface);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_IFACE_H */
