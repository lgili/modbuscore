#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "modbus_amalgamated.h"

#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Runtime configuration for the ESP-IDF Modbus quickstart helper.
 */
typedef struct {
    uart_port_t port;          /**< UART controller used for RTU traffic. */
    int tx_pin;                /**< TXD GPIO. */
    int rx_pin;                /**< RXD GPIO. */
    int rts_pin;               /**< RTS/DE GPIO (set to -1 to disable). */
    int cts_pin;               /**< CTS/RE GPIO (set to -1 to disable). */
    int baudrate;              /**< Serial baudrate. */
    uint32_t read_timeout_ms;  /**< Poll timeout passed to uart_read_bytes(). */
    bool install_driver;       /**< When true the helper installs the UART driver. */
} modbus_esp_quickstart_config_t;

/**
 * @brief Returns a configuration seeded from menuconfig defaults.
 */
modbus_esp_quickstart_config_t modbus_esp_quickstart_default_config(void);

/**
 * @brief Initialises the Modbus client and UART transport.
 *
 * Call once from `app_main()` before polling the client. The helper provisions
 * the UART peripheral (unless @p cfg->install_driver is false), sets up the
 * non-blocking transport bridge and configures the Modbus client with the
 * pre-allocated transaction pool.
 */
mb_err_t modbus_esp_quickstart_init(const modbus_esp_quickstart_config_t *cfg);

/**
 * @brief Releases UART resources and clears the client state.
 */
void modbus_esp_quickstart_shutdown(void);

/**
 * @brief Returns the quickstart Modbus client instance.
 */
mb_client_t *modbus_esp_quickstart_client(void);

/**
 * @brief Submits a read holding registers request using the default client.
 */
mb_err_t modbus_esp_quickstart_submit_read_holding(uint8_t unit_id,
                                                   uint16_t start_address,
                                                   uint16_t quantity,
                                                   mb_client_callback_t callback,
                                                   void *user_ctx);

/**
 * @brief Submits a write single register request using the default client.
 */
mb_err_t modbus_esp_quickstart_submit_write_single(uint8_t unit_id,
                                                   uint16_t address,
                                                   uint16_t value,
                                                   mb_client_callback_t callback,
                                                   void *user_ctx);

#ifdef __cplusplus
}
#endif
