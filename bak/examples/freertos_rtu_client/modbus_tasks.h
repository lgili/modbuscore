/**
 * @file modbus_tasks.h
 * @brief Task declarations and shared data structures
 */

#ifndef MODBUS_TASKS_H
#define MODBUS_TASKS_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 * TASK HANDLES (for notifications and control)
 * =========================================================================== */

extern TaskHandle_t modbus_rx_task_handle;
extern TaskHandle_t modbus_tx_task_handle;
extern TaskHandle_t app_task_handle;

/* ===========================================================================
 * SHARED DATA (protected by mutex)
 * =========================================================================== */

#define REGISTER_COUNT 10

extern uint16_t register_values[REGISTER_COUNT];
extern SemaphoreHandle_t register_mutex;

/* ===========================================================================
 * STATISTICS
 * =========================================================================== */

typedef struct {
    uint32_t successful_reads;
    uint32_t failed_reads;
    uint32_t crc_errors;
    uint32_t timeouts;
    uint32_t exceptions;
} modbus_stats_t;

extern volatile modbus_stats_t modbus_stats;

/* ===========================================================================
 * ISR CALLBACKS (called from UART interrupt)
 * =========================================================================== */

/**
 * @brief UART IDLE line callback - notifies RX task of frame boundary
 * 
 * Call this from your UART IDLE interrupt handler:
 * 
 * void USART1_IRQHandler(void) {
 *     if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)) {
 *         __HAL_UART_CLEAR_IDLEFLAG(&huart1);
 *         uart_idle_callback();
 *     }
 * }
 */
void uart_idle_callback(void);

/**
 * @brief DMA transfer complete callback (optional)
 */
void uart_dma_tx_complete_callback(void);

/* ===========================================================================
 * TASK FUNCTIONS
 * =========================================================================== */

void modbus_rx_task(void *pvParameters);
void modbus_tx_task(void *pvParameters);
void app_task(void *pvParameters);

/* ===========================================================================
 * INITIALIZATION
 * =========================================================================== */

void modbus_init(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TASKS_H */
