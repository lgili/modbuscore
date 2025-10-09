/**
 * @file system_config.h
 * @brief Platform-specific hardware abstraction layer
 * 
 * ADAPT THIS FILE TO YOUR SPECIFIC HARDWARE PLATFORM
 * 
 * This header defines the HAL functions that must be implemented for your
 * target hardware. Examples are provided for common platforms.
 */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 * SYSTEM CLOCK & TIMER
 * =========================================================================== */

/**
 * @brief Initialize system clock (e.g., PLL, HSE, etc.)
 */
void system_clock_init(void);

/**
 * @brief Initialize 1ms tick timer (SysTick or hardware timer)
 */
void timer_init(void);

/**
 * @brief Get millisecond timestamp
 * @return Monotonic millisecond counter (rolls over at UINT32_MAX)
 */
uint32_t millis(void);

/**
 * @brief Blocking delay in milliseconds
 * @param ms Delay duration in milliseconds
 */
void delay_ms(uint32_t ms);

/* ===========================================================================
 * UART CONFIGURATION
 * =========================================================================== */

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN,
    UART_PARITY_ODD
} uart_parity_t;

typedef enum {
    UART_STOP_BITS_1 = 0,
    UART_STOP_BITS_2
} uart_stop_bits_t;

/**
 * @brief Initialize UART for Modbus RTU
 * @param baudrate Baud rate (e.g., 9600, 19200, 115200)
 * @param parity Parity setting
 * @param stop_bits Stop bits setting
 */
void uart_init(uint32_t baudrate, uart_parity_t parity, uart_stop_bits_t stop_bits);

/**
 * @brief Send data via UART (blocking or buffered)
 * @param data Pointer to data buffer
 * @param len Number of bytes to send
 * @return Number of bytes actually sent
 */
size_t uart_send(const uint8_t *data, size_t len);

/**
 * @brief Check how many bytes are available in RX buffer
 * @return Number of bytes available
 */
size_t uart_recv_available(void);

/**
 * @brief Receive data from UART RX buffer
 * @param buffer Destination buffer
 * @param max_len Maximum bytes to read
 * @return Number of bytes actually read
 */
size_t uart_recv(uint8_t *buffer, size_t max_len);

/* ===========================================================================
 * LED (STATUS INDICATOR)
 * =========================================================================== */

/**
 * @brief Initialize LED GPIO
 */
void led_init(void);

/**
 * @brief Turn LED on
 */
void led_on(void);

/**
 * @brief Turn LED off
 */
void led_off(void);

/**
 * @brief Toggle LED state
 */
void led_toggle(void);

/* ===========================================================================
 * PLATFORM-SPECIFIC IMPLEMENTATIONS
 * =========================================================================== */

#if defined(STM32F4)
    #include "stm32f4xx_hal.h"
    // Add STM32F4-specific includes here
#elif defined(STM32G0)
    #include "stm32g0xx_hal.h"
    // Add STM32G0-specific includes here
#elif defined(ESP32)
    #include "driver/uart.h"
    #include "driver/gpio.h"
    #include "freertos/FreeRTOS.h"
    // Add ESP32-specific includes here
#elif defined(NRF52)
    #include "nrf.h"
    #include "nrf_gpio.h"
    #include "nrf_uarte.h"
    // Add nRF52-specific includes here
#else
    #warning "No platform defined - implement HAL functions manually"
#endif

/* ===========================================================================
 * EXAMPLE IMPLEMENTATION (STM32 HAL)
 * =========================================================================== */

#ifdef STM32F4

// Global variables (defined in main.c or separate hal.c)
extern UART_HandleTypeDef huart1;
extern volatile uint32_t system_ticks_ms;

static inline uint32_t millis(void)
{
    return system_ticks_ms;
}

static inline void delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

static inline size_t uart_send(const uint8_t *data, size_t len)
{
    if (HAL_UART_Transmit(&huart1, (uint8_t*)data, len, 1000) == HAL_OK) {
        return len;
    }
    return 0;
}

static inline size_t uart_recv_available(void)
{
    // Implement using UART RX buffer or DMA
    // This is a placeholder - use actual buffer implementation
    return 0;
}

static inline size_t uart_recv(uint8_t *buffer, size_t max_len)
{
    // Implement using UART RX buffer or DMA
    // This is a placeholder - use actual buffer implementation
    return 0;
}

static inline void led_on(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
}

static inline void led_off(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

static inline void led_toggle(void)
{
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
}

#endif /* STM32F4 */

/* ===========================================================================
 * NOTES FOR PORTING
 * =========================================================================== */

/*
 * UART RX BUFFER IMPLEMENTATION:
 * 
 * For production code, implement a circular buffer for UART RX:
 * 
 * 1. DMA Circular Mode (Recommended):
 *    - Configure DMA in circular mode
 *    - Track head/tail indices
 *    - Use IDLE line interrupt to detect frame end
 * 
 * 2. Interrupt-Based:
 *    - UART RX interrupt writes to circular buffer
 *    - uart_recv_available() returns (head - tail) % size
 *    - uart_recv() copies from buffer and advances tail
 * 
 * 3. Polling (Simple but inefficient):
 *    - Check UART status register in main loop
 *    - Read data register when RX not empty
 * 
 * See: embedded/ports/stm32-ll-dma-idle/ for reference implementation
 */

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_CONFIG_H */
