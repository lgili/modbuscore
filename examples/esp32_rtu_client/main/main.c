/**
 * @file main.c
 * @brief ESP-IDF + Modbus RTU Client Example
 * 
 * This example demonstrates production-ready integration of Modbus library
 * with ESP-IDF (Espressif IoT Development Framework) for ESP32 series.
 * 
 * Features:
 * - FreeRTOS tasks (ESP-IDF is built on FreeRTOS)
 * - ESP32 UART driver with event queue
 * - Timer callback for periodic requests
 * - Mutex-protected shared data
 * - NVS storage for Modbus configuration
 * - WiFi integration ready (optional)
 * 
 * Architecture:
 *   UART Event Task → mb_client_poll() → Callback → Update registers
 *                     ↑
 *                  Timer (1 sec) → Send FC03 request
 * 
 * Target: ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6
 * 
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "modbus/client.h"
#include "modbus/transport/rtu.h"

static const char *TAG = "modbus_example";

/* ===========================================================================
 * CONFIGURATION (adjustable via menuconfig or sdkconfig.h)
 * =========================================================================== */

/* UART Configuration */
#define UART_PORT_NUM           UART_NUM_1
#define UART_BAUD_RATE          9600
#define UART_TX_PIN             GPIO_NUM_17  /* Default ESP32 UART1 TX */
#define UART_RX_PIN             GPIO_NUM_16  /* Default ESP32 UART1 RX */
#define UART_RTS_PIN            UART_PIN_NO_CHANGE  /* RS485 DE/RE */
#define UART_CTS_PIN            UART_PIN_NO_CHANGE

/* UART Buffer sizes */
#define UART_RX_BUF_SIZE        512
#define UART_TX_BUF_SIZE        512
#define UART_QUEUE_SIZE         20

/* GPIO Configuration */
#define LED_GPIO                GPIO_NUM_2   /* ESP32 onboard LED */

/* Modbus Configuration */
#define MODBUS_SLAVE_ADDR       1
#define MODBUS_START_REGISTER   0x0000
#define MODBUS_REGISTER_COUNT   10
#define REQUEST_INTERVAL_MS     1000
#define POLL_BUDGET             8

/* Task Configuration */
#define UART_EVENT_TASK_STACK_SIZE  3072
#define UART_EVENT_TASK_PRIORITY    10
#define APP_TASK_STACK_SIZE         4096
#define APP_TASK_PRIORITY           5

/* ===========================================================================
 * MODBUS CLIENT CONTEXT
 * =========================================================================== */

static mb_client_t client;
static mb_transport_rtu_t rtu_transport;

/* Modbus PDU buffers */
static uint8_t tx_buffer[256];
static uint8_t rx_buffer[256];

/* UART event queue */
static QueueHandle_t uart_queue;

/* ===========================================================================
 * SHARED DATA (protected by mutex)
 * =========================================================================== */

static SemaphoreHandle_t register_mutex;
static uint16_t register_values[MODBUS_REGISTER_COUNT];
static volatile uint32_t successful_reads = 0;
static volatile uint32_t failed_reads = 0;
static volatile uint32_t crc_errors = 0;

/* ===========================================================================
 * TIMER FOR PERIODIC REQUESTS
 * =========================================================================== */

static esp_timer_handle_t request_timer;

/* ===========================================================================
 * TRANSPORT LAYER CALLBACKS
 * =========================================================================== */

/**
 * @brief Send data via UART
 * 
 * Uses ESP32 UART driver for non-blocking transmission.
 * 
 * @param transport Transport instance
 * @param data Data to send
 * @param len Number of bytes
 * @return Number of bytes sent
 */
static size_t transport_send(mb_transport_t *transport, const uint8_t *data, size_t len)
{
    (void)transport;
    
    /* Send via ESP32 UART driver (blocking with timeout) */
    int sent = uart_write_bytes(UART_PORT_NUM, (const char *)data, len);
    
    if (sent < 0) {
        ESP_LOGE(TAG, "UART write failed");
        return 0;
    }
    
    /* Wait for transmission complete */
    uart_wait_tx_done(UART_PORT_NUM, pdMS_TO_TICKS(100));
    
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, data, len, ESP_LOG_DEBUG);
    return (size_t)sent;
}

/**
 * @brief Receive data from UART
 * 
 * Reads available bytes from UART RX buffer.
 * Non-blocking - returns immediately with available data.
 * 
 * @param transport Transport instance
 * @param buffer Destination buffer
 * @param max_len Maximum bytes to read
 * @return Number of bytes read
 */
static size_t transport_recv(mb_transport_t *transport, uint8_t *buffer, size_t max_len)
{
    (void)transport;
    
    /* Check how many bytes are available in RX buffer */
    size_t available = 0;
    uart_get_buffered_data_len(UART_PORT_NUM, &available);
    
    if (available == 0) {
        return 0;
    }
    
    /* Read up to max_len bytes */
    size_t to_read = (available < max_len) ? available : max_len;
    int read = uart_read_bytes(UART_PORT_NUM, buffer, to_read, 0);
    
    if (read < 0) {
        ESP_LOGE(TAG, "UART read failed");
        return 0;
    }
    
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, buffer, read, ESP_LOG_DEBUG);
    return (size_t)read;
}

/**
 * @brief Get current timestamp in milliseconds
 * 
 * Uses ESP32 high-resolution timer.
 * 
 * @return Milliseconds since boot
 */
static uint32_t transport_get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief Yield function
 * 
 * Allows other FreeRTOS tasks to run.
 */
static void transport_yield(void)
{
    taskYIELD();
}

/* ===========================================================================
 * MODBUS CALLBACKS
 * =========================================================================== */

/**
 * @brief Callback for FC03 Read Holding Registers response
 * 
 * Extracts register values and stores them in shared memory.
 */
static void modbus_read_callback(mb_client_t *cli, const mb_pdu_t *req, const mb_pdu_t *resp)
{
    (void)cli;
    (void)req;
    
    const uint8_t *resp_data = mb_pdu_get_data(resp);
    uint8_t byte_count = resp_data[0];
    uint16_t register_count = byte_count / 2;
    
    if (register_count > MODBUS_REGISTER_COUNT) {
        ESP_LOGW(TAG, "Response has more registers than expected: %u > %u",
                 register_count, MODBUS_REGISTER_COUNT);
        register_count = MODBUS_REGISTER_COUNT;
    }
    
    /* Extract 16-bit registers (big-endian) */
    xSemaphoreTake(register_mutex, portMAX_DELAY);
    
    for (uint16_t i = 0; i < register_count; i++) {
        register_values[i] = (resp_data[1 + i*2] << 8) | resp_data[1 + i*2 + 1];
    }
    
    xSemaphoreGive(register_mutex);
    
    successful_reads++;
    
    ESP_LOGI(TAG, "FC03 Success - Read %u registers from 0x%04X",
             register_count, MODBUS_START_REGISTER);
    
    /* LED ON = success */
    gpio_set_level(LED_GPIO, 1);
}

/**
 * @brief Callback for Modbus errors
 */
static void modbus_error_callback(mb_client_t *cli, const mb_pdu_t *req, mb_error_t error)
{
    (void)cli;
    (void)req;
    
    failed_reads++;
    
    if (error == MB_ERR_CRC) {
        crc_errors++;
    }
    
    ESP_LOGE(TAG, "Modbus error: %d (%s)", error, mb_error_to_string(error));
    
    /* LED OFF = error */
    gpio_set_level(LED_GPIO, 0);
}

/* ===========================================================================
 * TIMER CALLBACK (Periodic Modbus Requests)
 * =========================================================================== */

/**
 * @brief ESP timer callback for periodic FC03 requests
 * 
 * Sends Modbus FC03 (Read Holding Registers) request every 1 second.
 * Runs in timer task context.
 */
static void request_timer_callback(void *arg)
{
    (void)arg;
    
    /* Build FC03 request */
    mb_pdu_t *req = mb_client_get_request_buffer(&client);
    
    uint8_t *pdu = mb_pdu_get_data(req);
    pdu[0] = 0x03;  /* FC03: Read Holding Registers */
    pdu[1] = (MODBUS_START_REGISTER >> 8) & 0xFF;
    pdu[2] = MODBUS_START_REGISTER & 0xFF;
    pdu[3] = (MODBUS_REGISTER_COUNT >> 8) & 0xFF;
    pdu[4] = MODBUS_REGISTER_COUNT & 0xFF;
    
    mb_pdu_set_data_len(req, 5);
    
    /* Send request (async) */
    mb_error_t err = mb_client_send_request(&client, MODBUS_SLAVE_ADDR, req,
                                             modbus_read_callback,
                                             modbus_error_callback,
                                             500);  /* 500ms timeout */
    
    if (err != MB_OK) {
        ESP_LOGE(TAG, "Failed to send FC03 request: %d", err);
    }
}

/* ===========================================================================
 * UART EVENT TASK
 * =========================================================================== */

/**
 * @brief UART event handler task
 * 
 * Monitors UART events (RX data, errors, etc.) and polls Modbus client.
 * This is where Modbus frame processing happens.
 */
static void uart_event_task(void *pvParameters)
{
    (void)pvParameters;
    uart_event_t event;
    
    ESP_LOGI(TAG, "UART event task started");
    
    while (1) {
        /* Wait for UART event (blocking) */
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    /* Data received - poll Modbus client with budget */
                    mb_client_poll_with_budget(&client, POLL_BUDGET);
                    break;
                
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "UART FIFO overflow");
                    uart_flush_input(UART_PORT_NUM);
                    xQueueReset(uart_queue);
                    break;
                
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "UART ring buffer full");
                    uart_flush_input(UART_PORT_NUM);
                    xQueueReset(uart_queue);
                    break;
                
                case UART_BREAK:
                    ESP_LOGW(TAG, "UART break detected");
                    break;
                
                case UART_PARITY_ERR:
                    ESP_LOGW(TAG, "UART parity error");
                    break;
                
                case UART_FRAME_ERR:
                    ESP_LOGW(TAG, "UART frame error");
                    break;
                
                default:
                    break;
            }
        }
    }
}

/* ===========================================================================
 * APPLICATION TASK
 * =========================================================================== */

/**
 * @brief Application task - processes register values
 * 
 * In a real application, this would implement business logic:
 * - Control outputs based on Modbus data
 * - Publish data to MQTT/HTTP
 * - Store to SD card
 * - Update display
 */
static void app_task(void *pvParameters)
{
    (void)pvParameters;
    
    ESP_LOGI(TAG, "Application task started");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));  /* 5 seconds */
        
        /* Read register values under mutex protection */
        xSemaphoreTake(register_mutex, portMAX_DELAY);
        
        ESP_LOGI(TAG, "=== Modbus Statistics ===");
        ESP_LOGI(TAG, "Successful reads: %lu", successful_reads);
        ESP_LOGI(TAG, "Failed reads: %lu", failed_reads);
        ESP_LOGI(TAG, "CRC errors: %lu", crc_errors);
        ESP_LOGI(TAG, "Register values:");
        
        for (int i = 0; i < MODBUS_REGISTER_COUNT; i++) {
            ESP_LOGI(TAG, "  [%04X] = 0x%04X (%u)",
                     MODBUS_START_REGISTER + i,
                     register_values[i],
                     register_values[i]);
        }
        
        xSemaphoreGive(register_mutex);
        
        /* Report heap usage */
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    }
}

/* ===========================================================================
 * INITIALIZATION FUNCTIONS
 * =========================================================================== */

/**
 * @brief Initialize GPIO (LED)
 */
static void init_gpio(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_GPIO, 0);
    
    ESP_LOGI(TAG, "GPIO initialized - LED on GPIO%d", LED_GPIO);
}

/**
 * @brief Initialize UART
 */
static esp_err_t init_uart(void)
{
    /* Configure UART parameters */
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    esp_err_t ret = uart_param_config(UART_PORT_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %d", ret);
        return ret;
    }
    
    /* Set UART pins */
    ret = uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_RTS_PIN, UART_CTS_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %d", ret);
        return ret;
    }
    
    /* Install UART driver with event queue */
    ret = uart_driver_install(UART_PORT_NUM, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE,
                               UART_QUEUE_SIZE, &uart_queue, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "UART initialized: %d baud, 8N1, TX=%d, RX=%d",
             UART_BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
    
    return ESP_OK;
}

/**
 * @brief Initialize Modbus client
 */
static esp_err_t init_modbus(void)
{
    /* Initialize RTU transport */
    mb_transport_rtu_config_t rtu_config = {
        .baudrate = UART_BAUD_RATE,
        .parity = MB_PARITY_NONE,
        .stop_bits = 1,
    };
    
    mb_error_t err = mb_transport_rtu_init(&rtu_transport, &rtu_config);
    if (err != MB_OK) {
        ESP_LOGE(TAG, "Failed to init RTU transport: %d", err);
        return ESP_FAIL;
    }
    
    /* Set transport callbacks */
    rtu_transport.base.send = transport_send;
    rtu_transport.base.recv = transport_recv;
    rtu_transport.base.get_time_ms = transport_get_time_ms;
    rtu_transport.base.yield = transport_yield;
    
    ESP_LOGI(TAG, "Modbus RTU transport initialized");
    
    /* Initialize Modbus client */
    mb_client_config_t client_config = {
        .transport = &rtu_transport.base,
        .tx_buffer = tx_buffer,
        .tx_buffer_size = sizeof(tx_buffer),
        .rx_buffer = rx_buffer,
        .rx_buffer_size = sizeof(rx_buffer),
    };
    
    err = mb_client_init(&client, &client_config);
    if (err != MB_OK) {
        ESP_LOGE(TAG, "Failed to init Modbus client: %d", err);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Modbus client initialized");
    
    return ESP_OK;
}

/* ===========================================================================
 * MAIN
 * =========================================================================== */

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP-IDF + Modbus RTU Client Example ===");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    
    /* Initialize NVS (for WiFi/config storage if needed) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    /* Initialize GPIO */
    init_gpio();
    
    /* Initialize UART */
    ESP_ERROR_CHECK(init_uart());
    
    /* Initialize Modbus */
    ESP_ERROR_CHECK(init_modbus());
    
    /* Create mutex for register access */
    register_mutex = xSemaphoreCreateMutex();
    if (register_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }
    
    /* Create UART event task */
    xTaskCreate(uart_event_task, "uart_event_task",
                UART_EVENT_TASK_STACK_SIZE, NULL,
                UART_EVENT_TASK_PRIORITY, NULL);
    
    /* Create application task */
    xTaskCreate(app_task, "app_task",
                APP_TASK_STACK_SIZE, NULL,
                APP_TASK_PRIORITY, NULL);
    
    /* Create periodic request timer */
    esp_timer_create_args_t timer_args = {
        .callback = request_timer_callback,
        .name = "modbus_request_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &request_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(request_timer, REQUEST_INTERVAL_MS * 1000));
    
    ESP_LOGI(TAG, "Modbus request timer started (%d ms interval)", REQUEST_INTERVAL_MS);
    ESP_LOGI(TAG, "Initialization complete");
    
    /* Main task can now exit - FreeRTOS scheduler keeps running */
}
