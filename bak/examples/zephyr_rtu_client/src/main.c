/**
 * @file main.c
 * @brief Zephyr RTOS + Modbus RTU Client Example
 * 
 * This example demonstrates production-ready integration of Modbus library
 * with Zephyr RTOS, showcasing:
 * - Workqueue-based polling (k_work_submit)
 * - Timer-driven periodic requests (k_timer)
 * - Mutex-protected shared data (k_mutex)
 * - UART callback integration (uart_irq_callback_set)
 * - Thread-safe logging (LOG_MODULE_REGISTER)
 * 
 * Architecture:
 *   UART ISR → Workqueue → mb_client_poll() → Callback → Update registers
 *              ↑
 *           k_timer (1 sec) → Send FC03 request
 * 
 * Target: nRF52840, STM32F4, ESP32, or any Zephyr-supported board with UART
 * 
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "modbus/client.h"
#include "modbus/transport/rtu.h"

LOG_MODULE_REGISTER(modbus_example, LOG_LEVEL_INF);

/* ===========================================================================
 * CONFIGURATION (adjust to your board in prj.conf)
 * =========================================================================== */

/* UART device (defined in devicetree: zephyr,shell-uart or custom overlay) */
#define UART_DEVICE_NODE DT_ALIAS(modbus_uart)
#if !DT_NODE_HAS_STATUS(UART_DEVICE_NODE, okay)
#error "Unsupported board: modbus_uart devicetree alias is not defined"
#endif

/* LED GPIO (optional, for status indication) */
#define LED_NODE DT_ALIAS(led0)
#if DT_NODE_HAS_STATUS(LED_NODE, okay)
#define HAS_LED 1
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
#else
#define HAS_LED 0
#endif

/* Modbus configuration */
#define MODBUS_SLAVE_ADDR       1
#define MODBUS_START_REGISTER   0x0000
#define MODBUS_REGISTER_COUNT   10
#define REQUEST_INTERVAL_MS     1000
#define POLL_BUDGET             8  /* Max steps per workqueue iteration */

/* ===========================================================================
 * MODBUS CLIENT CONTEXT
 * =========================================================================== */

static mb_client_t client;
static mb_transport_rtu_t rtu_transport;

/* Modbus PDU buffer (256 bytes = max Modbus frame) */
static uint8_t tx_buffer[256];
static uint8_t rx_buffer[256];

/* Transport I/O buffers for RTU (T1.5/T3.5 timing) */
static uint8_t uart_tx_buf[256];
static uint8_t uart_rx_buf[256];
static size_t uart_rx_len = 0;

/* ===========================================================================
 * SHARED DATA (protected by mutex)
 * =========================================================================== */

K_MUTEX_DEFINE(register_mutex);
static uint16_t register_values[MODBUS_REGISTER_COUNT];
static volatile uint32_t successful_reads = 0;
static volatile uint32_t failed_reads = 0;

/* ===========================================================================
 * ZEPHYR WORKQUEUE AND TIMER
 * =========================================================================== */

/* Workqueue for Modbus polling (runs in thread context) */
static struct k_work modbus_poll_work;

/* Timer for periodic Modbus requests (1 second) */
static struct k_timer request_timer;

/* ===========================================================================
 * UART DEVICE
 * =========================================================================== */

static const struct device *uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* ===========================================================================
 * TRANSPORT LAYER CALLBACKS
 * =========================================================================== */

/**
 * @brief Send data via UART
 * 
 * Called by Modbus RTU transport to transmit frame.
 * Blocks until all bytes are sent (polling mode for simplicity).
 * 
 * @param transport Transport instance
 * @param data Data to send
 * @param len Number of bytes
 * @return Number of bytes sent, or 0 on error
 */
static size_t transport_send(mb_transport_t *transport, const uint8_t *data, size_t len)
{
    (void)transport;
    
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return 0;
    }
    
    /* Copy to UART TX buffer */
    if (len > sizeof(uart_tx_buf)) {
        LOG_ERR("TX buffer overflow: %zu > %zu", len, sizeof(uart_tx_buf));
        return 0;
    }
    memcpy(uart_tx_buf, data, len);
    
    /* Send via UART (polling mode) */
    for (size_t i = 0; i < len; i++) {
        uart_poll_out(uart_dev, uart_tx_buf[i]);
    }
    
    LOG_HEXDUMP_DBG(data, len, "TX");
    return len;
}

/**
 * @brief Receive data from UART
 * 
 * Called by Modbus RTU transport to read received bytes.
 * Returns data accumulated by UART ISR.
 * 
 * @param transport Transport instance
 * @param buffer Destination buffer
 * @param max_len Maximum bytes to read
 * @return Number of bytes read
 */
static size_t transport_recv(mb_transport_t *transport, uint8_t *buffer, size_t max_len)
{
    (void)transport;
    
    if (uart_rx_len == 0) {
        return 0;
    }
    
    size_t to_copy = (uart_rx_len < max_len) ? uart_rx_len : max_len;
    memcpy(buffer, uart_rx_buf, to_copy);
    
    /* Shift remaining bytes (if any) */
    if (to_copy < uart_rx_len) {
        memmove(uart_rx_buf, uart_rx_buf + to_copy, uart_rx_len - to_copy);
    }
    uart_rx_len -= to_copy;
    
    LOG_HEXDUMP_DBG(buffer, to_copy, "RX");
    return to_copy;
}

/**
 * @brief Get current timestamp in milliseconds
 * 
 * Used by Modbus RTU for T1.5/T3.5 timeout calculations.
 * 
 * @return Milliseconds since boot
 */
static uint32_t transport_get_time_ms(void)
{
    return k_uptime_get_32();
}

/**
 * @brief Idle/yield function (optional)
 * 
 * Called by Modbus library during long operations.
 * In Zephyr, we can yield to other threads.
 */
static void transport_yield(void)
{
    k_yield();
}

/* ===========================================================================
 * UART INTERRUPT HANDLER
 * =========================================================================== */

/**
 * @brief UART RX interrupt callback
 * 
 * Accumulates received bytes into uart_rx_buf.
 * After frame boundary (idle line or timeout), submits work to poll Modbus.
 * 
 * NOTE: This runs in ISR context - must be fast and non-blocking!
 */
static void uart_isr_callback(const struct device *dev, void *user_data)
{
    (void)user_data;
    
    if (!uart_irq_update(uart_dev)) {
        return;
    }
    
    /* RX data ready? */
    if (uart_irq_rx_ready(uart_dev)) {
        uint8_t byte;
        while (uart_fifo_read(uart_dev, &byte, 1) == 1) {
            if (uart_rx_len < sizeof(uart_rx_buf)) {
                uart_rx_buf[uart_rx_len++] = byte;
            } else {
                LOG_WRN("RX buffer overflow");
            }
        }
        
        /* Submit workqueue to process Modbus frame
         * Note: In production, you might want to use a timer to detect
         * frame boundary (T3.5 = 3.5 characters of silence) instead of
         * submitting work on every byte. For simplicity, we submit immediately.
         */
        k_work_submit(&modbus_poll_work);
    }
}

/* ===========================================================================
 * MODBUS CALLBACKS
 * =========================================================================== */

/**
 * @brief Callback for FC03 Read Holding Registers response
 * 
 * Called by Modbus client when FC03 response is received and validated.
 * Extracts register values and stores them in shared memory.
 * 
 * @param client Modbus client instance
 * @param req Request PDU (what we sent)
 * @param resp Response PDU (what we received)
 */
static void modbus_read_callback(mb_client_t *cli, const mb_pdu_t *req, const mb_pdu_t *resp)
{
    (void)cli;
    (void)req;
    
    /* Parse FC03 response format:
     * [Function Code: 1 byte] [Byte Count: 1 byte] [Register Data: N*2 bytes]
     */
    const uint8_t *resp_data = mb_pdu_get_data(resp);
    uint8_t byte_count = resp_data[0];
    uint16_t register_count = byte_count / 2;
    
    if (register_count > MODBUS_REGISTER_COUNT) {
        LOG_WRN("Response has more registers than expected: %u > %u",
                register_count, MODBUS_REGISTER_COUNT);
        register_count = MODBUS_REGISTER_COUNT;
    }
    
    /* Extract 16-bit registers (big-endian) */
    k_mutex_lock(&register_mutex, K_FOREVER);
    
    for (uint16_t i = 0; i < register_count; i++) {
        register_values[i] = (resp_data[1 + i*2] << 8) | resp_data[1 + i*2 + 1];
    }
    
    k_mutex_unlock(&register_mutex);
    
    successful_reads++;
    
    LOG_INF("FC03 Success - Registers 0x%04X..0x%04X read",
            MODBUS_START_REGISTER,
            MODBUS_START_REGISTER + register_count - 1);
    
#if HAS_LED
    gpio_pin_set_dt(&led, 1);  /* LED ON = success */
#endif
}

/**
 * @brief Callback for Modbus errors
 * 
 * Called when request times out or receives exception response.
 * 
 * @param client Modbus client instance
 * @param req Request PDU
 * @param error Error code
 */
static void modbus_error_callback(mb_client_t *cli, const mb_pdu_t *req, mb_error_t error)
{
    (void)cli;
    (void)req;
    
    failed_reads++;
    
    LOG_ERR("Modbus error: %d (%s)", error, mb_error_to_string(error));
    
#if HAS_LED
    gpio_pin_set_dt(&led, 0);  /* LED OFF = error */
#endif
}

/* ===========================================================================
 * WORKQUEUE HANDLER (Modbus Polling)
 * =========================================================================== */

/**
 * @brief Workqueue handler for Modbus polling
 * 
 * Called from system workqueue when UART RX data is available.
 * Processes received bytes, validates frames, triggers callbacks.
 * 
 * Uses budget-based polling to prevent CPU starvation.
 */
static void modbus_poll_work_handler(struct k_work *work)
{
    (void)work;
    
    /* Poll client with budget (max 8 steps)
     * Each step processes ~1 byte or 1 state machine transition
     * Budget prevents blocking other threads for too long
     */
    mb_client_poll_with_budget(&client, POLL_BUDGET);
}

/* ===========================================================================
 * TIMER HANDLER (Periodic Modbus Requests)
 * =========================================================================== */

/**
 * @brief Timer callback for periodic FC03 requests
 * 
 * Sends Modbus FC03 (Read Holding Registers) request every 1 second.
 * Runs in timer ISR context - must be fast!
 */
static void request_timer_handler(struct k_timer *timer)
{
    (void)timer;
    
    /* Build FC03 request:
     * [Slave Address] [Function Code=0x03] [Start Addr High] [Start Addr Low]
     * [Register Count High] [Register Count Low] [CRC16]
     */
    mb_pdu_t *req = mb_client_get_request_buffer(&client);
    
    uint8_t *pdu = mb_pdu_get_data(req);
    pdu[0] = 0x03;  /* FC03: Read Holding Registers */
    pdu[1] = (MODBUS_START_REGISTER >> 8) & 0xFF;  /* Start address high */
    pdu[2] = MODBUS_START_REGISTER & 0xFF;         /* Start address low */
    pdu[3] = (MODBUS_REGISTER_COUNT >> 8) & 0xFF;  /* Register count high */
    pdu[4] = MODBUS_REGISTER_COUNT & 0xFF;         /* Register count low */
    
    mb_pdu_set_data_len(req, 5);
    
    /* Send request (async) */
    mb_error_t err = mb_client_send_request(&client, MODBUS_SLAVE_ADDR, req,
                                             modbus_read_callback,
                                             modbus_error_callback,
                                             500);  /* 500ms timeout */
    
    if (err != MB_OK) {
        LOG_ERR("Failed to send FC03 request: %d", err);
    }
}

/* ===========================================================================
 * APPLICATION LOGIC (runs in main thread)
 * =========================================================================== */

/**
 * @brief Application thread - processes register values
 * 
 * In a real application, this would:
 * - Control outputs based on register values
 * - Log data to flash/cloud
 * - Implement business logic
 * 
 * For this example, we just print values periodically.
 */
static void app_thread(void)
{
    while (1) {
        k_sleep(K_SECONDS(5));
        
        /* Read register values under mutex protection */
        k_mutex_lock(&register_mutex, K_FOREVER);
        
        LOG_INF("=== Modbus Statistics ===");
        LOG_INF("Successful reads: %u", successful_reads);
        LOG_INF("Failed reads: %u", failed_reads);
        LOG_INF("Register values:");
        for (int i = 0; i < MODBUS_REGISTER_COUNT; i++) {
            LOG_INF("  [%04X] = 0x%04X (%u)",
                    MODBUS_START_REGISTER + i,
                    register_values[i],
                    register_values[i]);
        }
        
        k_mutex_unlock(&register_mutex);
    }
}

/* ===========================================================================
 * MAIN - INITIALIZATION
 * =========================================================================== */

int main(void)
{
    LOG_INF("=== Zephyr + Modbus RTU Client Example ===");
    LOG_INF("Built: %s %s", __DATE__, __TIME__);
    
    /* -----------------------------------------------------------------------
     * 1. Initialize LED (optional)
     * ----------------------------------------------------------------------- */
#if HAS_LED
    if (!gpio_is_ready_dt(&led)) {
        LOG_WRN("LED device not ready");
    } else {
        gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
        LOG_INF("LED initialized on pin %d", led.pin);
    }
#endif
    
    /* -----------------------------------------------------------------------
     * 2. Initialize UART
     * ----------------------------------------------------------------------- */
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return -1;
    }
    
    /* Configure UART: 9600 baud, 8N1 (set in prj.conf or devicetree) */
    struct uart_config uart_cfg = {
        .baudrate = 9600,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };
    
    int ret = uart_configure(uart_dev, &uart_cfg);
    if (ret != 0) {
        LOG_ERR("Failed to configure UART: %d", ret);
        return -1;
    }
    
    /* Enable RX interrupt */
    uart_irq_callback_set(uart_dev, uart_isr_callback);
    uart_irq_rx_enable(uart_dev);
    
    LOG_INF("UART initialized: 9600 8N1");
    
    /* -----------------------------------------------------------------------
     * 3. Initialize Modbus RTU Transport
     * ----------------------------------------------------------------------- */
    mb_transport_rtu_config_t rtu_config = {
        .baudrate = 9600,
        .parity = MB_PARITY_NONE,
        .stop_bits = 1,
    };
    
    mb_error_t err = mb_transport_rtu_init(&rtu_transport, &rtu_config);
    if (err != MB_OK) {
        LOG_ERR("Failed to init RTU transport: %d", err);
        return -1;
    }
    
    /* Set transport callbacks */
    rtu_transport.base.send = transport_send;
    rtu_transport.base.recv = transport_recv;
    rtu_transport.base.get_time_ms = transport_get_time_ms;
    rtu_transport.base.yield = transport_yield;
    
    LOG_INF("Modbus RTU transport initialized");
    
    /* -----------------------------------------------------------------------
     * 4. Initialize Modbus Client
     * ----------------------------------------------------------------------- */
    mb_client_config_t client_config = {
        .transport = &rtu_transport.base,
        .tx_buffer = tx_buffer,
        .tx_buffer_size = sizeof(tx_buffer),
        .rx_buffer = rx_buffer,
        .rx_buffer_size = sizeof(rx_buffer),
    };
    
    err = mb_client_init(&client, &client_config);
    if (err != MB_OK) {
        LOG_ERR("Failed to init Modbus client: %d", err);
        return -1;
    }
    
    LOG_INF("Modbus client initialized");
    
    /* -----------------------------------------------------------------------
     * 5. Initialize Workqueue and Timer
     * ----------------------------------------------------------------------- */
    k_work_init(&modbus_poll_work, modbus_poll_work_handler);
    k_timer_init(&request_timer, request_timer_handler, NULL);
    
    /* Start timer (periodic, 1 second interval) */
    k_timer_start(&request_timer, K_MSEC(REQUEST_INTERVAL_MS),
                  K_MSEC(REQUEST_INTERVAL_MS));
    
    LOG_INF("Modbus request timer started (%d ms interval)", REQUEST_INTERVAL_MS);
    
    /* -----------------------------------------------------------------------
     * 6. Run Application Thread
     * ----------------------------------------------------------------------- */
    LOG_INF("Initialization complete - entering main loop");
    
    app_thread();  /* Never returns */
    
    return 0;
}
