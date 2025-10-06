#include "modbus_esp_quickstart.h"

#include <string.h>

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef CONFIG_MODBUS_QUICKSTART_YIELD_MS
#define CONFIG_MODBUS_QUICKSTART_YIELD_MS 1
#endif

#ifndef CONFIG_MODBUS_QUICKSTART_CLIENT_POOL_SIZE
#define CONFIG_MODBUS_QUICKSTART_CLIENT_POOL_SIZE 4
#endif

#ifndef CONFIG_MODBUS_QUICKSTART_CLIENT_QUEUE_CAPACITY
#define CONFIG_MODBUS_QUICKSTART_CLIENT_QUEUE_CAPACITY 0
#endif

#ifndef CONFIG_MODBUS_QUICKSTART_UART_RX_BUFFER
#define CONFIG_MODBUS_QUICKSTART_UART_RX_BUFFER 256
#endif

#ifndef CONFIG_MODBUS_QUICKSTART_UART_TX_BUFFER
#define CONFIG_MODBUS_QUICKSTART_UART_TX_BUFFER 256
#endif

#ifndef CONFIG_MODBUS_QUICKSTART_UART_PORT
#define CONFIG_MODBUS_QUICKSTART_UART_PORT 1
#endif

#ifndef CONFIG_MODBUS_QUICKSTART_UART_BAUDRATE
#define CONFIG_MODBUS_QUICKSTART_UART_BAUDRATE 115200
#endif

static const char *TAG = "modbus_qs";

typedef struct {
    uart_port_t port;
    TickType_t read_timeout_ticks;
} modbus_esp_uart_transport_t;

static mb_client_t s_client;
static mb_client_txn_t s_tx_pool[CONFIG_MODBUS_QUICKSTART_CLIENT_POOL_SIZE];
static bool s_initialised;
static bool s_driver_installed;
static modbus_esp_uart_transport_t s_transport_ctx;
static mb_transport_if_t s_iface;

static mb_err_t modbus_esp_uart_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    modbus_esp_uart_transport_t *transport = (modbus_esp_uart_transport_t *)ctx;
    if (!transport || !buf || len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    int written = uart_write_bytes(transport->port, (const char *)buf, (size_t)len);
    if (written < 0) {
        ESP_LOGE(TAG, "uart_write_bytes failed (%d)", written);
        return MB_ERR_TRANSPORT;
    }

    if (out != NULL) {
        out->processed = (mb_size_t)written;
    }

    if ((size_t)written < len) {
        ESP_LOGW(TAG, "UART truncated write (%d/%u)", written, (unsigned)len);
        return MB_ERR_TRANSPORT;
    }

    return MB_OK;
}

static mb_err_t modbus_esp_uart_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    modbus_esp_uart_transport_t *transport = (modbus_esp_uart_transport_t *)ctx;
    if (!transport || !buf || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    int read = uart_read_bytes(transport->port, buf, (uint32_t)cap, transport->read_timeout_ticks);
    if (read < 0) {
        ESP_LOGE(TAG, "uart_read_bytes failed (%d)", read);
        return MB_ERR_TRANSPORT;
    }

    if (out != NULL) {
        out->processed = (mb_size_t)read;
    }

    return MB_OK;
}

static mb_time_ms_t modbus_esp_now(void *ctx)
{
    (void)ctx;
    return (mb_time_ms_t)(esp_timer_get_time() / 1000ULL);
}

static void modbus_esp_yield(void *ctx)
{
    (void)ctx;
#if CONFIG_MODBUS_QUICKSTART_YIELD_MS > 0
    vTaskDelay(pdMS_TO_TICKS(CONFIG_MODBUS_QUICKSTART_YIELD_MS));
#else
    taskYIELD();
#endif
}

modbus_esp_quickstart_config_t modbus_esp_quickstart_default_config(void)
{
    modbus_esp_quickstart_config_t cfg = {
        .port = (uart_port_t)CONFIG_MODBUS_QUICKSTART_UART_PORT,
        .tx_pin = UART_PIN_NO_CHANGE,
        .rx_pin = UART_PIN_NO_CHANGE,
        .rts_pin = UART_PIN_NO_CHANGE,
        .cts_pin = UART_PIN_NO_CHANGE,
        .baudrate = CONFIG_MODBUS_QUICKSTART_UART_BAUDRATE,
        .read_timeout_ms = 10U,
        .install_driver = true,
    };
    return cfg;
}

static void modbus_esp_configure_uart(const modbus_esp_quickstart_config_t *cfg)
{
    uart_config_t uart_cfg = {
        .baud_rate = cfg->baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_param_config(cfg->port, &uart_cfg));
    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_set_pin(cfg->port,
                                               cfg->tx_pin,
                                               cfg->rx_pin,
                                               cfg->rts_pin,
                                               cfg->cts_pin));
}

mb_err_t modbus_esp_quickstart_init(const modbus_esp_quickstart_config_t *cfg)
{
    if (cfg == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (s_initialised) {
        modbus_esp_quickstart_shutdown();
    }

    if (cfg->install_driver) {
        esp_err_t err = uart_driver_install(cfg->port,
                                            CONFIG_MODBUS_QUICKSTART_UART_RX_BUFFER,
                                            CONFIG_MODBUS_QUICKSTART_UART_TX_BUFFER,
                                            0,
                                            NULL,
                                            0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "uart_driver_install failed (%s)", esp_err_to_name(err));
            return MB_ERR_TRANSPORT;
        }
        s_driver_installed = (err == ESP_OK);
    }

    modbus_esp_configure_uart(cfg);

    s_transport_ctx.port = cfg->port;
    s_transport_ctx.read_timeout_ticks = pdMS_TO_TICKS(cfg->read_timeout_ms);

    s_iface.ctx = &s_transport_ctx;
    s_iface.send = modbus_esp_uart_send;
    s_iface.recv = modbus_esp_uart_recv;
    s_iface.now = modbus_esp_now;
    s_iface.yield = modbus_esp_yield;

    memset(&s_client, 0, sizeof(s_client));
    memset(s_tx_pool, 0, sizeof(s_tx_pool));

    mb_err_t err = mb_client_init(&s_client, &s_iface, s_tx_pool, MB_COUNTOF(s_tx_pool));
    if (err != MB_OK) {
        ESP_LOGE(TAG, "mb_client_init failed (%d)", (int)err);
        modbus_esp_quickstart_shutdown();
        return err;
    }

#if CONFIG_MODBUS_QUICKSTART_CLIENT_QUEUE_CAPACITY > 0
    mb_client_set_queue_capacity(&s_client, CONFIG_MODBUS_QUICKSTART_CLIENT_QUEUE_CAPACITY);
#endif

    s_initialised = true;
    ESP_LOGI(TAG, "Modbus client ready on UART%d @ %d baud", cfg->port, cfg->baudrate);
    return MB_OK;
}

void modbus_esp_quickstart_shutdown(void)
{
    if (!s_initialised) {
        return;
    }

    if (s_driver_installed) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(uart_driver_delete(s_transport_ctx.port));
        s_driver_installed = false;
    }

    memset(&s_client, 0, sizeof(s_client));
    memset(s_tx_pool, 0, sizeof(s_tx_pool));
    memset(&s_transport_ctx, 0, sizeof(s_transport_ctx));
    memset(&s_iface, 0, sizeof(s_iface));
    s_initialised = false;
}

mb_client_t *modbus_esp_quickstart_client(void)
{
    return s_initialised ? &s_client : NULL;
}

static mb_err_t modbus_esp_submit_request(uint8_t unit_id,
                                          const mb_u8 *pdu,
                                          mb_size_t pdu_len,
                                          mb_client_callback_t callback,
                                          void *user_ctx)
{
    if (!s_initialised || pdu == NULL || pdu_len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (pdu_len > MB_PDU_MAX) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_client_request_t request = {
        .flags = 0U,
        .request = {
            .unit_id = unit_id,
            .function = pdu[0],
            .payload = &pdu[1],
            .payload_len = pdu_len - 1U,
        },
        .timeout_ms = 0U,
        .max_retries = 0U,
        .retry_backoff_ms = 0U,
        .callback = callback,
        .user_ctx = user_ctx,
    };

    return mb_client_submit(&s_client, &request, NULL);
}

mb_err_t modbus_esp_quickstart_submit_read_holding(uint8_t unit_id,
                                                   uint16_t start_address,
                                                   uint16_t quantity,
                                                   mb_client_callback_t callback,
                                                   void *user_ctx)
{
    mb_u8 pdu[5] = {0};
    mb_err_t err = mb_pdu_build_read_holding_request(pdu, sizeof(pdu), start_address, quantity);
    if (err != MB_OK) {
        return err;
    }
    return modbus_esp_submit_request(unit_id, pdu, sizeof(pdu), callback, user_ctx);
}

mb_err_t modbus_esp_quickstart_submit_write_single(uint8_t unit_id,
                                                   uint16_t address,
                                                   uint16_t value,
                                                   mb_client_callback_t callback,
                                                   void *user_ctx)
{
    mb_u8 pdu[5] = {0};
    mb_err_t err = mb_pdu_build_write_single_request(pdu, sizeof(pdu), address, value);
    if (err != MB_OK) {
        return err;
    }
    return modbus_esp_submit_request(unit_id, pdu, sizeof(pdu), callback, user_ctx);
}
