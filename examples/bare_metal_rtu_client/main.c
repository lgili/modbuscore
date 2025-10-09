/**
 * @file main.c
 * @brief Bare-metal Modbus RTU client example
 * 
 * This example demonstrates a minimal Modbus RTU client running in a bare-metal
 * environment (no RTOS). It uses cooperative polling with budget control to
 * avoid blocking the main loop.
 * 
 * Hardware Requirements:
 * - MCU with UART (e.g., STM32, ESP32, nRF52)
 * - RS-485 transceiver connected to UART TX/RX
 * - 1ms system tick timer (SysTick or equivalent)
 * 
 * Features:
 * - Non-blocking poll with budget (4 steps per tick)
 * - Reads 10 holding registers from server (FC03)
 * - Simple timeout and retry logic
 * - LED blinks on successful communication
 */

#include <modbus/client.h>
#include <modbus/transport/rtu.h>
#include <modbus/pdu.h>
#include <modbus/mb_err.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ===========================================================================
 * HARDWARE ABSTRACTION LAYER (HAL) - ADAPT TO YOUR PLATFORM
 * ===========================================================================
 * 
 * Implement these functions for your specific hardware:
 * - uart_init(), uart_send(), uart_recv_available(), uart_recv()
 * - timer_init(), millis()
 * - led_init(), led_on(), led_off()
 */

#include "system_config.h"  // Platform-specific implementations

/* ===========================================================================
 * MODBUS CONFIGURATION
 * =========================================================================== */

#define SERVER_ADDRESS      1       // Modbus server unit ID
#define REGISTER_START      0x0000  // Starting address
#define REGISTER_COUNT      10      // Number of registers to read
#define POLL_BUDGET         4       // Max steps per main loop iteration
#define REQUEST_INTERVAL_MS 1000    // Request every 1 second

/* ===========================================================================
 * TRANSPORT LAYER (UART + RTU)
 * =========================================================================== */

static mb_err_t uart_transport_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    (void)ctx;
    
    size_t sent = uart_send(buf, len);
    
    if (out) {
        out->processed = sent;
    }
    
    return (sent == len) ? MB_OK : MB_ERR_TRANSPORT;
}

static mb_err_t uart_transport_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    (void)ctx;
    
    size_t available = uart_recv_available();
    size_t to_read = (available < cap) ? available : cap;
    
    if (to_read == 0) {
        if (out) {
            out->processed = 0;
        }
        return MB_ERR_TIMEOUT;
    }
    
    size_t received = uart_recv(buf, to_read);
    
    if (out) {
        out->processed = received;
    }
    
    return MB_OK;
}

static mb_time_ms_t uart_transport_now(void *ctx)
{
    (void)ctx;
    return millis();
}

static const mb_transport_if_t uart_transport_iface = {
    .ctx = NULL,
    .send = uart_transport_send,
    .recv = uart_transport_recv,
    .sendv = NULL,  // Optional scatter-gather not used
    .recvv = NULL,  // Optional scatter-gather not used
    .now = uart_transport_now,
    .yield = NULL   // No cooperative yield in bare-metal
};

/* ===========================================================================
 * MODBUS CLIENT STATE
 * =========================================================================== */

static mb_client_t modbus_client;
static mb_client_txn_t transaction_pool[4];  // Pool for 4 concurrent transactions
static mb_u16 register_values[REGISTER_COUNT];
static bool request_in_progress = false;
static uint32_t last_request_time = 0;
static uint32_t successful_reads = 0;
static uint32_t failed_reads = 0;

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
    
    request_in_progress = false;
    
    if (status == MB_OK && response != NULL) {
        // Parse FC03 response
        const mb_u8 *payload;
        mb_u16 reg_count;
        
        mb_err_t parse_err = mb_pdu_parse_read_holding_response(
            response->payload - 1,  // Include function code
            response->payload_len + 1,
            &payload,
            &reg_count
        );
        
        if (parse_err == MB_OK && reg_count == REGISTER_COUNT) {
            // Extract register values (big-endian)
            for (uint16_t i = 0; i < REGISTER_COUNT; i++) {
                register_values[i] = ((uint16_t)payload[i * 2] << 8) | payload[i * 2 + 1];
            }
            
            successful_reads++;
            led_on();  // Indicate success
        } else {
            failed_reads++;
            led_off();
        }
    } else {
        failed_reads++;
        led_off();
    }
}

/* ===========================================================================
 * APPLICATION LOGIC
 * =========================================================================== */

static void send_read_request(void)
{
    if (request_in_progress) {
        return;  // Previous request still pending
    }
    
    // Build FC03 Read Holding Registers request
    mb_u8 pdu_buffer[MB_PDU_MAX];
    mb_size_t pdu_len;
    
    mb_err_t err = mb_pdu_build_read_holding_request(
        pdu_buffer,
        MB_PDU_MAX,
        REGISTER_START,
        REGISTER_COUNT
    );
    
    if (err != MB_OK) {
        return;
    }
    
    // Determine PDU length (FC03: FC(1) + Start(2) + Count(2) = 5 bytes)
    pdu_len = 5;
    
    // Create ADU view
    mb_adu_view_t request_adu = {
        .unit_id = SERVER_ADDRESS,
        .function = 0x03,  // FC03
        .payload = pdu_buffer + 1,  // Skip function code
        .payload_len = pdu_len - 1
    };
    
    // Create client request
    mb_client_request_t request = {
        .flags = 0,
        .request = request_adu,
        .timeout_ms = 1000,
        .max_retries = 2,
        .retry_backoff_ms = 500,
        .callback = modbus_read_callback,
        .user_ctx = NULL
    };
    
    // Submit request
    err = mb_client_submit(&modbus_client, &request, NULL);
    
    if (err == MB_OK) {
        request_in_progress = true;
        last_request_time = millis();
    }
}

/* ===========================================================================
 * MAIN APPLICATION
 * =========================================================================== */

int main(void)
{
    // Initialize hardware
    system_clock_init();
    timer_init();
    uart_init(19200, UART_PARITY_EVEN, UART_STOP_BITS_1);
    led_init();
    
    // Initialize Modbus client
    mb_err_t err = mb_client_init(
        &modbus_client,
        &uart_transport_iface,
        transaction_pool,
        sizeof(transaction_pool) / sizeof(transaction_pool[0])
    );
    
    if (err != MB_OK) {
        // Initialization failed - blink LED rapidly
        while (1) {
            led_on();
            delay_ms(100);
            led_off();
            delay_ms(100);
        }
    }
    
    // Main loop
    while (1) {
        uint32_t now = millis();
        
        // Send periodic read request
        if (now - last_request_time >= REQUEST_INTERVAL_MS) {
            send_read_request();
        }
        
        // Poll Modbus client with budget (non-blocking)
        mb_client_poll_with_budget(&modbus_client, POLL_BUDGET);
        
        // Your application logic here
        // - Process register_values[]
        // - Update outputs
        // - Handle other tasks
        
        // Optional: yield to other tasks or sleep
        // (In bare-metal, you might enter low-power mode here)
    }
    
    return 0;
}
