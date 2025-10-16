/**
 * @file modbus_transport.c
 * @brief Mock transport implementation for testing purposes.
 *
 * This file provides a mock implementation of the functions declared in
 * modbus_transport.h. It uses internal static buffers to simulate reading
 * and writing data, allowing you to run tests without actual hardware.
 * 
 * Adjust or expand this mock as needed for more complex test scenarios.
 * 
 * @author
 * @date 2024-12-20
 */

#include <modbus/modbus.h>
#include <modbus/internal/transport_core.h>
#include <string.h>

/* Mock buffers and state */
#define MOCK_BUFFER_SIZE 256

static uint8_t rx_mock_buffer[MOCK_BUFFER_SIZE];
static uint16_t rx_mock_count = 0;  /* Number of bytes currently in the RX buffer */
static uint16_t rx_mock_index = 0;  /* Read index for the RX buffer */

static uint8_t tx_mock_buffer[MOCK_BUFFER_SIZE];
static uint16_t tx_mock_count = 0;  /* Number of bytes written to TX buffer */

static uint16_t mock_time_reference = 0; /* Mock time reference counter */

/* Forward declarations for internal helpers used by the shims */
static int32_t mock_read(uint8_t *buf, uint16_t count);
static int32_t mock_write(const uint8_t *buf, uint16_t count);

static mb_err_t mock_send_shim(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    (void)ctx;
    if (!buf) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (len > UINT16_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const int32_t written = mock_write(buf, (uint16_t)len);
    if (written < 0) {
        return MODBUS_ERROR_TRANSPORT;
    }

    if (out) {
        out->processed = (mb_size_t)written;
    }

    return (written == (int32_t)len) ? MODBUS_ERROR_NONE : MODBUS_ERROR_TRANSPORT;
}

static mb_err_t mock_recv_shim(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    (void)ctx;
    if (!buf || cap == 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (cap > UINT16_MAX) {
        cap = UINT16_MAX;
    }

    const int32_t read_count = mock_read(buf, (uint16_t)cap);
    if (read_count < 0) {
        return MODBUS_ERROR_TRANSPORT;
    }

    if (out) {
        out->processed = (mb_size_t)read_count;
    }

    return (read_count > 0) ? MODBUS_ERROR_NONE : MODBUS_ERROR_TIMEOUT;
}

static mb_time_ms_t mock_now_shim(void *ctx)
{
    (void)ctx;
    return mock_time_reference;
}

static void mock_yield_shim(void *ctx)
{
    (void)ctx;
}

static const mb_transport_if_t mock_transport_iface = {
    .ctx = NULL,
    .send = mock_send_shim,
    .recv = mock_recv_shim,
    .now = mock_now_shim,
    .yield = mock_yield_shim,
};

/**
 * @brief Mock read function.
 *
 * Tries to read `count` bytes from the rx_mock_buffer. If the buffer doesn't
 * have enough data, it reads what it can. If you want to simulate timeouts or
 * errors, you can modify the logic here.
 *
 * @param buf   Pointer to the buffer to store read data.
 * @param count Number of bytes to read.
 * @return int32_t Number of bytes read, or negative on error.
 */
static int32_t mock_read(uint8_t *buf, uint16_t count) {
    uint16_t bytes_available = (rx_mock_count > rx_mock_index) ? (rx_mock_count - rx_mock_index) : 0;
    if (bytes_available == 0) {
        // No data available, could simulate a timeout
        return 0; // Returns 0 indicating no data read
    }
    uint16_t bytes_to_read = (count < bytes_available) ? count : bytes_available;
    memcpy(buf, &rx_mock_buffer[rx_mock_index], bytes_to_read);
    rx_mock_index += bytes_to_read;
    return bytes_to_read;
}

// static int32_t mock_read_event(uint8_t *buf, uint16_t count) {
//     uint16_t bytes_available = (rx_mock_count > rx_mock_index) ? (rx_mock_count - rx_mock_index) : 0;
//     if (bytes_available == 0) {
//         // No data available, could simulate a timeout
//         return 0; // Returns 0 indicating no data read
//     }
//     uint16_t bytes_to_read = (count < bytes_available) ? count : bytes_available;
//     // memcpy(buf, &rx_mock_buffer[rx_mock_index], bytes_to_read);
//     // rx_mock_index += bytes_to_read;
//     for (size_t i = 0; i < bytes_available; i++)
//     {
//         modbus_server_receive_data_from_uart_event()
//     }
    
//     return bytes_to_read;
// }

/**
 * @brief Mock write function.
 *
 * Writes `count` bytes to the tx_mock_buffer. If it exceeds the buffer,
 * it returns an error.
 *
 * @param buf   Pointer to the data to write.
 * @param count Number of bytes to write.
 * @return int32_t Number of bytes written, or negative on error.
 */
static int32_t mock_write(const uint8_t *buf, uint16_t count) {
    if (count > (MOCK_BUFFER_SIZE - tx_mock_count)) {
        // Not enough space
        return -1; // Error
    }
    memcpy(&tx_mock_buffer[tx_mock_count], buf, count);
    tx_mock_count += count;
    return count;
}

/**
 * @brief Mock function to get a reference in milliseconds.
 *
 * Just returns a counter. You can increment this counter manually in your test
 * to simulate time passing.
 *
 * @return uint16_t The current mock time reference.
 */
static uint16_t mock_get_reference_msec(void) {
    return mock_time_reference;
}

/**
 * @brief Mock function to measure elapsed time in milliseconds.
 *
 * Returns the difference between the current counter and the given reference.
 *
 * @param ref Reference time.
 * @return uint16_t Elapsed time since ref.
 */
static uint16_t mock_measure_time_msec(uint16_t ref) {
    if (mock_time_reference >= ref) {
        return (uint16_t)(mock_time_reference - ref);
    } else {
        // If overflow occurred, just handle wrap-around (simple approach)
        return (uint16_t)(0xFFFFU - ref + mock_time_reference + 1U);
    }
}

/* The following are optional and are currently not implemented or needed for the mock. */
static uint16_t mock_change_baudrate(uint16_t baudrate) {
    (void)baudrate;
    return 19200U; // Just return a default value
}

static void mock_restart_uart(void) {
    // Reset counters and buffers
    rx_mock_count = 0;
    rx_mock_index = 0;
    tx_mock_count = 0;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
static uint8_t mock_write_gpio(uint8_t gpio, uint8_t value) {
    (void)gpio;
    (void)value;
    // No-op. Return success.
    return 0;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

static uint8_t mock_parse_bootloader_request(uint8_t *buffer, uint16_t *buffer_size) {
    (void)buffer;
    (void)buffer_size;
    // Not implemented
    return 0;
}

/* Public mock helpers ----------------------------------------------------- */
int mock_inject_rx_data(const uint8_t *data, uint16_t length);
uint16_t mock_get_tx_data(uint8_t *data, uint16_t maxlen);
void mock_clear_tx_buffer(void);
void mock_advance_time(uint16_t ms);
void modbus_transport_init_mock(modbus_transport_t *transport);
uint16_t get_current_time_ms(void);
const mb_transport_if_t *mock_transport_get_iface(void);

/* 
 * Public Functions to Control the Mock 
 * ------------------------------------
 * These are not part of the standard interface but are provided so that test
 * code can inject data and control time.
 */

/**
 * @brief Inject data into the RX buffer to simulate incoming data.
 *
 * @param data   Pointer to the data to inject.
 * @param length Number of bytes of data to inject.
 * @return int 0 on success, -1 if not enough space.
 */
int mock_inject_rx_data(const uint8_t *data, uint16_t length) {
    if (rx_mock_index == rx_mock_count) {
        rx_mock_index = 0;
        rx_mock_count = 0;
    }
    if (length > (MOCK_BUFFER_SIZE - rx_mock_count)) {
        return -1; // No space
    }
    memcpy(&rx_mock_buffer[rx_mock_count], data, length);
    rx_mock_count += length;
    return 0;
}

/**
 * @brief Retrieve data from the TX buffer.
 *
 * @param data   Pointer to the buffer to store TX data.
 * @param maxlen Maximum number of bytes to retrieve.
 * @return uint16_t Number of bytes retrieved.
 */
uint16_t mock_get_tx_data(uint8_t *data, uint16_t maxlen) {
    uint16_t to_copy = (tx_mock_count < maxlen) ? tx_mock_count : maxlen;
    memcpy(data, tx_mock_buffer, to_copy);
    return to_copy;
}

/**
 * @brief Clear TX buffer.
 */
void mock_clear_tx_buffer(void) {
    tx_mock_count = 0;
}

/**
 * @brief Advance the mock time by a given amount.
 *
 * @param ms Number of milliseconds to advance.
 */
void mock_advance_time(uint16_t ms) {
    mock_time_reference = (uint16_t)(mock_time_reference + ms);
}

/**
 * @brief Initialize a modbus_transport_t structure with the mock functions.
 *
 * @param transport Pointer to a modbus_transport_t structure to initialize.
 */
void modbus_transport_init_mock(modbus_transport_t *transport) {
    if (!transport) {
        return;
    }
    transport->transport = MODBUS_TRANSPORT_RTU; // Example
    transport->read = mock_read;
    transport->write = mock_write;
    transport->get_reference_msec = mock_get_reference_msec;
    transport->measure_time_msec = mock_measure_time_msec;
    transport->change_baudrate = mock_change_baudrate;
    transport->restart_uart = mock_restart_uart;
    transport->write_gpio = mock_write_gpio;
    transport->parse_bootloader_request = mock_parse_bootloader_request;
    transport->arg = NULL;

    // Clear buffers and counters
    rx_mock_count = 0;
    rx_mock_index = 0;
    tx_mock_count = 0;
    mock_time_reference = 0;
}

uint16_t get_current_time_ms(void) {
    return mock_time_reference;
}

const mb_transport_if_t *mock_transport_get_iface(void)
{
    return &mock_transport_iface;
}
