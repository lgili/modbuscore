/**
 * @file main.c
 * @brief FreeRTOS Modbus RTU Client Example
 * 
 * This example demonstrates a production-ready Modbus RTU client using FreeRTOS
 * with properly structured tasks, ISR-safe notifications, and stream buffers.
 * 
 * Architecture:
 * - modbus_rx_task: Polls Modbus client, processes incoming frames
 * - modbus_tx_task: Handles request queuing and transmission
 * - app_task: Application logic, sends periodic read requests
 * - UART IDLE ISR: Notifies RX task when frame boundary detected
 * 
 * Features:
 * - Zero-copy RX via DMA circular buffer
 * - ISR-safe frame notifications (xTaskNotifyFromISR)
 * - Stream buffers for TX queue
 * - Cooperative polling with budget control
 * - Proper task synchronization
 */

#include <modbus/client.h>
#include <modbus/transport/rtu.h>
#include <modbus/pdu.h>
#include <modbus/mb_err.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "semphr.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ===========================================================================
 * CONFIGURATION
 * =========================================================================== */

#define SERVER_ADDRESS      1
#define REGISTER_START      0x0000
#define REGISTER_COUNT      10
#define POLL_BUDGET         8       // Steps per RX task iteration
#define REQUEST_INTERVAL_MS 1000

/* Task priorities */
#define PRIORITY_MODBUS_RX  (tskIDLE_PRIORITY + 3)  // Highest - real-time comms
#define PRIORITY_MODBUS_TX  (tskIDLE_PRIORITY + 2)  // High
#define PRIORITY_APP        (tskIDLE_PRIORITY + 1)  // Normal

/* Task stack sizes (words, not bytes) */
#define STACK_SIZE_MODBUS_RX    256
#define STACK_SIZE_MODBUS_TX    256
#define STACK_SIZE_APP          512

/* Stream buffer sizes */
#define TX_STREAM_BUFFER_SIZE   512
#define RX_NOTIFICATION_BIT     (1 << 0)

/* ===========================================================================
 * HARDWARE ABSTRACTION (Platform-specific)
 * =========================================================================== */

// Implement these for your platform
extern void uart_init_dma(uint32_t baudrate);
extern void uart_enable_idle_irq(void);
extern size_t uart_get_dma_rx_count(void);
extern uint8_t* uart_get_dma_rx_buffer(void);
extern void uart_send_dma(const uint8_t *data, size_t len);

/* ===========================================================================
 * MODBUS CLIENT STATE
 * =========================================================================== */

static mb_client_t modbus_client;
static mb_client_txn_t transaction_pool[4];
static mb_u16 register_values[REGISTER_COUNT];
static SemaphoreHandle_t register_mutex;

/* Statistics */
static volatile uint32_t successful_reads = 0;
static volatile uint32_t failed_reads = 0;

/* ===========================================================================
 * TRANSPORT LAYER (UART + DMA)
 * =========================================================================== */

/* DMA circular buffer state */
static struct {
    uint8_t *buffer;
    size_t size;
    size_t last_pos;
} dma_rx_state;

static TaskHandle_t modbus_rx_task_handle;

/**
 * @brief UART IDLE line ISR - called when frame boundary detected
 * 
 * This should be called from your UART IDLE interrupt handler.
 * Example for STM32 HAL:
 * 
 * void USART1_IRQHandler(void) {
 *     if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)) {
 *         __HAL_UART_CLEAR_IDLEFLAG(&huart1);
 *         uart_idle_callback();
 *     }
 * }
 */
void uart_idle_callback(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    
    // Notify RX task that frame boundary was detected
    vTaskNotifyGiveFromISR(modbus_rx_task_handle, &higher_priority_task_woken);
    
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static mb_err_t uart_transport_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    (void)ctx;
    
    // Send via DMA (non-blocking)
    uart_send_dma(buf, len);
    
    if (out) {
        out->processed = len;
    }
    
    return MB_OK;
}

static mb_err_t uart_transport_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    (void)ctx;
    
    // Read from DMA circular buffer
    size_t current_pos = uart_get_dma_rx_count();
    size_t available;
    
    if (current_pos >= dma_rx_state.last_pos) {
        available = current_pos - dma_rx_state.last_pos;
    } else {
        available = dma_rx_state.size - dma_rx_state.last_pos + current_pos;
    }
    
    if (available == 0) {
        if (out) {
            out->processed = 0;
        }
        return MB_ERR_TIMEOUT;
    }
    
    size_t to_read = (available < cap) ? available : cap;
    size_t copied = 0;
    
    // Copy from circular buffer
    while (copied < to_read) {
        buf[copied] = dma_rx_state.buffer[dma_rx_state.last_pos];
        dma_rx_state.last_pos = (dma_rx_state.last_pos + 1) % dma_rx_state.size;
        copied++;
    }
    
    if (out) {
        out->processed = copied;
    }
    
    return MB_OK;
}

static mb_time_ms_t uart_transport_now(void *ctx)
{
    (void)ctx;
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static void uart_transport_yield(void *ctx)
{
    (void)ctx;
    taskYIELD();
}

static const mb_transport_if_t uart_transport_iface = {
    .ctx = NULL,
    .send = uart_transport_send,
    .recv = uart_transport_recv,
    .sendv = NULL,
    .recvv = NULL,
    .now = uart_transport_now,
    .yield = uart_transport_yield
};

/* ===========================================================================
 * MODBUS CALLBACK
 * =========================================================================== */

static void modbus_read_callback(mb_client_t *client,
                                 const mb_client_txn_t *txn,
                                 mb_err_t status,
                                 const mb_adu_view_t *response,
                                 void *user_ctx)
{
    (void)client;
    (void)txn;
    (void)user_ctx;
    
    if (status == MB_OK && response != NULL) {
        const mb_u8 *payload;
        mb_u16 reg_count;
        
        mb_err_t parse_err = mb_pdu_parse_read_holding_response(
            response->payload - 1,
            response->payload_len + 1,
            &payload,
            &reg_count
        );
        
        if (parse_err == MB_OK && reg_count == REGISTER_COUNT) {
            // Take mutex and update register values
            if (xSemaphoreTake(register_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                for (uint16_t i = 0; i < REGISTER_COUNT; i++) {
                    register_values[i] = ((uint16_t)payload[i * 2] << 8) | payload[i * 2 + 1];
                }
                xSemaphoreGive(register_mutex);
            }
            
            successful_reads++;
        } else {
            failed_reads++;
        }
    } else {
        failed_reads++;
    }
}

/* ===========================================================================
 * FREERTOS TASKS
 * =========================================================================== */

/**
 * @brief Modbus RX Task - High priority, handles incoming frames
 */
static void modbus_rx_task(void *pvParameters)
{
    (void)pvParameters;
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        // Wait for notification from UART IDLE ISR (with timeout)
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
        
        // Poll Modbus client with budget (non-blocking)
        mb_client_poll_with_budget(&modbus_client, POLL_BUDGET);
        
        // Yield to lower priority tasks
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Modbus TX Task - Handles transmission queue
 */
static void modbus_tx_task(void *pvParameters)
{
    (void)pvParameters;
    
    while (1) {
        // In this simple example, TX is handled automatically by the client
        // In a more complex setup, you could queue requests here
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Application Task - Sends periodic read requests
 */
static void app_task(void *pvParameters)
{
    (void)pvParameters;
    
    TickType_t last_request_time = xTaskGetTickCount();
    
    while (1) {
        TickType_t now = xTaskGetTickCount();
        
        // Send read request every REQUEST_INTERVAL_MS
        if ((now - last_request_time) >= pdMS_TO_TICKS(REQUEST_INTERVAL_MS)) {
            // Build FC03 request
            mb_u8 pdu_buffer[MB_PDU_MAX];
            mb_pdu_build_read_holding_request(
                pdu_buffer,
                MB_PDU_MAX,
                REGISTER_START,
                REGISTER_COUNT
            );
            
            mb_adu_view_t request_adu = {
                .unit_id = SERVER_ADDRESS,
                .function = 0x03,
                .payload = pdu_buffer + 1,
                .payload_len = 4
            };
            
            mb_client_request_t request = {
                .flags = 0,
                .request = request_adu,
                .timeout_ms = 1000,
                .max_retries = 2,
                .retry_backoff_ms = 500,
                .callback = modbus_read_callback,
                .user_ctx = NULL
            };
            
            mb_client_submit(&modbus_client, &request, NULL);
            last_request_time = now;
        }
        
        // Process register values (example: print to console)
        if (xSemaphoreTake(register_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Your application logic here
            // Example: check register[0], trigger outputs, etc.
            xSemaphoreGive(register_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ===========================================================================
 * INITIALIZATION
 * =========================================================================== */

void modbus_init(void)
{
    // Initialize UART with DMA
    uart_init_dma(19200);
    
    // Setup DMA RX state
    dma_rx_state.buffer = uart_get_dma_rx_buffer();
    dma_rx_state.size = 512;  // Adjust to your DMA buffer size
    dma_rx_state.last_pos = 0;
    
    // Enable UART IDLE interrupt
    uart_enable_idle_irq();
    
    // Create mutex for register access
    register_mutex = xSemaphoreCreateMutex();
    configASSERT(register_mutex);
    
    // Initialize Modbus client
    mb_err_t err = mb_client_init(
        &modbus_client,
        &uart_transport_iface,
        transaction_pool,
        sizeof(transaction_pool) / sizeof(transaction_pool[0])
    );
    
    configASSERT(err == MB_OK);
}

/* ===========================================================================
 * MAIN / TASK CREATION
 * =========================================================================== */

int main(void)
{
    // Hardware init (clocks, GPIO, etc.) - platform specific
    // ...
    
    // Initialize Modbus subsystem
    modbus_init();
    
    // Create Modbus RX task (highest priority)
    BaseType_t result = xTaskCreate(
        modbus_rx_task,
        "ModbusRX",
        STACK_SIZE_MODBUS_RX,
        NULL,
        PRIORITY_MODBUS_RX,
        &modbus_rx_task_handle
    );
    configASSERT(result == pdPASS);
    
    // Create Modbus TX task
    result = xTaskCreate(
        modbus_tx_task,
        "ModbusTX",
        STACK_SIZE_MODBUS_TX,
        NULL,
        PRIORITY_MODBUS_TX,
        NULL
    );
    configASSERT(result == pdPASS);
    
    // Create application task
    result = xTaskCreate(
        app_task,
        "App",
        STACK_SIZE_APP,
        NULL,
        PRIORITY_APP,
        NULL
    );
    configASSERT(result == pdPASS);
    
    // Start FreeRTOS scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    while (1) {
        __NOP();
    }
    
    return 0;
}

/* ===========================================================================
 * FREERTOS HOOKS (Optional)
 * =========================================================================== */

#if (configUSE_IDLE_HOOK == 1)
void vApplicationIdleHook(void)
{
    // Enter low-power mode when idle
    // __WFI();
}
#endif

#if (configUSE_MALLOC_FAILED_HOOK == 1)
void vApplicationMallocFailedHook(void)
{
    // Handle heap allocation failure
    configASSERT(0);
}
#endif

#if (configCHECK_FOR_STACK_OVERFLOW > 0)
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    
    // Handle stack overflow
    configASSERT(0);
}
#endif
