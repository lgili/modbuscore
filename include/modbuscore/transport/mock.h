#ifndef MODBUSCORE_TRANSPORT_MOCK_H
#define MODBUSCORE_TRANSPORT_MOCK_H

/**
 * @file mock.h
 * @brief Transporte mock determinístico para testes da ModbusCore.
 *
 * O objetivo deste driver é permitir simulações de rede 100% controladas,
 * incluindo latências configuráveis para envio/recebimento e avanço manual
 * do relógio monotônico. Nenhuma chamada bloqueia; todos os dados são
 * armazenados em filas internas e podem ser inspecionados ou entregues sob
 * demanda pelos testes.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuração opcional para o mock de transporte.
 *
 * Todos os campos são opcionais e default para 0 caso @p config seja NULL
 * em mbc_mock_transport_create().
 */
typedef struct mbc_mock_transport_config {
    uint32_t initial_now_ms;    /**< Timestamp inicial do relógio interno (default 0). */
    uint32_t send_latency_ms;   /**< Latência aplicada a cada envio antes de ficar disponível aos testes. */
    uint32_t recv_latency_ms;   /**< Latência base aplicada a frames enfileirados via schedule (default 0). */
    uint32_t yield_advance_ms;  /**< Incremento de tempo aplicado ao chamar yield (default 0 = não avança). */
} mbc_mock_transport_config_t;

/**
 * @brief Contexto opaco do mock (definido internamente).
 */
typedef struct mbc_mock_transport mbc_mock_transport_t;

/**
 * @brief Instancia o transporte mock e preenche uma interface ModbusCore.
 *
 * @param config Configuração opcional (NULL usa defaults).
 * @param out_iface Interface de transporte preenchida na saída.
 * @param out_ctx Contexto interno alocado (use destroy ao final).
 *
 * @return MBC_STATUS_OK em sucesso ou código de erro correspondente.
 */
mbc_status_t mbc_mock_transport_create(const mbc_mock_transport_config_t *config,
                                       mbc_transport_iface_t *out_iface,
                                       mbc_mock_transport_t **out_ctx);

/**
 * @brief Destrói o mock liberando todos os recursos alocados.
 */
void mbc_mock_transport_destroy(mbc_mock_transport_t *ctx);

/**
 * @brief Reinicia o mock para o estado inicial, descartando filas e
 *        voltando o relógio para @ref mbc_mock_transport_config::initial_now_ms.
 */
void mbc_mock_transport_reset(mbc_mock_transport_t *ctx);

/**
 * @brief Avança o relógio interno em @p delta_ms milissegundos.
 */
void mbc_mock_transport_advance(mbc_mock_transport_t *ctx, uint32_t delta_ms);

/**
 * @brief Agenda dados para serem recebidos pelo próximo mbc_transport_receive().
 *
 * Os bytes serão disponibilizados após `config->recv_latency_ms + delay_ms`.
 *
 * @param ctx Mock previamente criado.
 * @param data Buffer de dados a ser copiado para a fila.
 * @param length Quantidade de bytes (deve ser > 0).
 * @param delay_ms Latência adicional antes dos dados ficarem prontos.
 *
 * @return MBC_STATUS_OK em sucesso, MBC_STATUS_NO_RESOURCES se memória esgotar,
 *         ou MBC_STATUS_INVALID_ARGUMENT se parâmetros forem inválidos.
 */
mbc_status_t mbc_mock_transport_schedule_rx(mbc_mock_transport_t *ctx,
                                            const uint8_t *data,
                                            size_t length,
                                            uint32_t delay_ms);

/**
 * @brief Recupera o próximo frame transmitido, se já estiver disponível.
 *
 * @param ctx Mock previamente criado.
 * @param buffer Buffer destino para cópia (não pode ser NULL).
 * @param capacity Capacidade do buffer destino.
 * @param out_length Número de bytes copiados. Definido como 0 se nada pronto.
 *
 * @return MBC_STATUS_OK em sucesso,
 *         MBC_STATUS_NO_RESOURCES se @p capacity for insuficiente,
 *         MBC_STATUS_INVALID_ARGUMENT para parâmetros inválidos.
 */
mbc_status_t mbc_mock_transport_fetch_tx(mbc_mock_transport_t *ctx,
                                         uint8_t *buffer,
                                         size_t capacity,
                                         size_t *out_length);

/**
 * @brief Informa quantos frames aguardam entrega no RX.
 */
size_t mbc_mock_transport_pending_rx(const mbc_mock_transport_t *ctx);

/**
 * @brief Informa quantos frames transmitidos aguardam coleta pelos testes.
 */
size_t mbc_mock_transport_pending_tx(const mbc_mock_transport_t *ctx);

/**
 * @brief Ajusta o estado de conexão do mock (true = conectado).
 *
 * Quando desconectado, operações de send/receive retornam MBC_STATUS_IO_ERROR.
 */
void mbc_mock_transport_set_connected(mbc_mock_transport_t *ctx, bool connected);

/**
 * @brief Faz com que a próxima chamada a send retorne @p status sem enfileirar dados.
 */
void mbc_mock_transport_fail_next_send(mbc_mock_transport_t *ctx, mbc_status_t status);

/**
 * @brief Faz com que a próxima chamada a receive retorne @p status.
 */
void mbc_mock_transport_fail_next_receive(mbc_mock_transport_t *ctx, mbc_status_t status);

/**
 * @brief Descarta o próximo frame pendente de RX (independente de estar pronto).
 */
mbc_status_t mbc_mock_transport_drop_next_rx(mbc_mock_transport_t *ctx);

/**
 * @brief Descarta o próximo frame pendente de TX (antes de fetch).
 */
mbc_status_t mbc_mock_transport_drop_next_tx(mbc_mock_transport_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_MOCK_H */
