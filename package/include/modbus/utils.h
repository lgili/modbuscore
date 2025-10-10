/**
 * @file modbus_utils.h
 * @brief Utility functions for Modbus protocol operations.
 *
 * This header provides helper functions to safely read data from buffers,
 * as well as functions to sort and search arrays of Modbus variables.
 * Additionally, it includes functions to calculate CRC-16 checksums, which are
 * essential for ensuring data integrity in Modbus communications.
 *
 * **Key Features:**
 * - Safe reading of 8-bit and 16-bit unsigned integers from buffers.
 * - Sorting and searching utilities for arrays of `variable_modbus_t`.
 * - CRC-16 calculation using both bit-by-bit and table-driven approaches.
 *
 * **Usage:**
 * - Include this header in your Modbus Client or Server implementation to utilize
 *   the provided utility functions.
 * - Example:
 *   ```c
 *   #include "utils.h"
 *   
 *   void example_usage(const uint8_t *data, uint16_t size) {
 *       uint16_t index = 0;
 *       uint8_t value8;
 *       uint16_t value16;
 *       
 *       if (modbus_read_uint8(data, &index, size, &value8)) {
 *           // Successfully read an 8-bit value
 *       }
 *       
 *       if (modbus_read_uint16(data, &index, size, &value16)) {
 *           // Successfully read a 16-bit value
 *       }
 *   }
 *   ```
 * 
 * @note
 * - Ensure that the buffer provided to the read functions is properly allocated and contains sufficient data.
 * - The sorting function uses selection sort; for larger datasets, consider implementing a more efficient sorting algorithm.
 *
 * @author
 * Luiz Carlos Gili
 * 
 * @date
 * 2024-12-20
 *
 * @addtogroup ModbusUtils
 * @{
 */

#ifndef MODBUS_UTILS_H
#define MODBUS_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>


#ifdef __cplusplus
extern "C"{
#endif 

#include <modbus/base.h>

/**
 * @def MODBUS_CONVERT_CHAR_INTERVAL_TO_MS
 * @brief Converts a character interval time from microseconds to milliseconds based on baud rate.
 *
 * This macro calculates the equivalent time in milliseconds for a given character interval
 * in microseconds (`TIME_US`) and a specified baud rate (`BAUDRATE`). It accounts for
 * the typical 11 bits per character in Modbus RTU (1 start bit, 8 data bits, 1 parity bit, 1 stop bit).
 *
 * @param TIME_US   Character interval time in microseconds.
 * @param BAUDRATE  Communication baud rate.
 * @return Calculated time in milliseconds.
 *
 * @example
 * ```c
 * #define TIME_US 100000 // 100 ms
 * #define BAUDRATE 19200
 * uint16_t interval_ms = MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(TIME_US, BAUDRATE);
 * // interval_ms will be approximately 572 ms
 * ```
 */
#define MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(TIME_US, BAUDRATE) \
        ((1000U * (TIME_US) * 11) / (BAUDRATE))

/**
 * @brief Safely reads an 8-bit unsigned integer from a buffer.
 *
 * This function checks if there is enough space in the buffer before reading
 * an 8-bit value. If reading would exceed the buffer size, it returns `false`.
 *
 * @param[in]  buffer       Pointer to the data buffer.
 * @param[in,out] index     Pointer to the current read index in the buffer. It is incremented by one on success.
 * @param[in]  buffer_size  Size of the buffer in bytes.
 * @param[out] value        Pointer to the variable where the read value will be stored.
 *
 * @return `true` if the value was successfully read, `false` if not enough data.
 *
 * @example
 * ```c
 * uint8_t data[] = {0x12, 0x34};
 * uint16_t idx = 0;
 * uint8_t val;
 * if (modbus_read_uint8(data, &idx, 2, &val)) {
 *     // val = 0x12
 * }
 * ```
 */
bool modbus_read_uint8(const uint8_t *buffer, uint16_t *index, uint16_t buffer_size, uint8_t *value);

/**
 * @brief Safely reads a 16-bit unsigned integer from a buffer (big-endian).
 *
 * This function checks if there is enough space in the buffer before reading
 * a 16-bit value. It reads two bytes from the buffer and combines them into
 * a 16-bit value, interpreting the first read byte as the high-order byte.
 *
 * @param[in]  buffer       Pointer to the data buffer.
 * @param[in,out] index     Pointer to the current read index in the buffer. It is incremented by two on success.
 * @param[in]  buffer_size  Size of the buffer in bytes.
 * @param[out] value        Pointer to the variable where the read 16-bit value will be stored.
 *
 * @return `true` if the value was successfully read, `false` if not enough data.
 *
 * @example
 * ```c
 * uint8_t data[] = {0x12, 0x34};
 * uint16_t idx = 0;
 * uint16_t val;
 * if (modbus_read_uint16(data, &idx, 2, &val)) {
 *     // val = 0x1234
 * }
 * ```
 */
bool modbus_read_uint16(const uint8_t *buffer, uint16_t *index, uint16_t buffer_size, uint16_t *value);

/**
 * @brief Sorts an array of Modbus variables by their address using selection sort.
 *
 * This function organizes the array of `variable_modbus_t` in ascending order
 * based on the `address` field. Selection sort is used due to its simplicity.
 *
 * @param[in,out] modbus_variables Array of Modbus variables to sort.
 * @param[in] length               Number of elements in the array.
 *
 * @note
 * - Selection sort has a time complexity of O(n²). For larger datasets, consider using more efficient sorting algorithms.
 *
 * @example
 * ```c
 * variable_modbus_t vars[3] = {
 *     {.address = 3},
 *     {.address = 1},
 *     {.address = 2}
 * };
 * modbus_selection_sort(vars, 3);
 * // vars now sorted by address: 1, 2, 3
 * ```
 */
void modbus_selection_sort(variable_modbus_t modbus_variables[], int length);

/**
 * @brief Performs a binary search on an array of Modbus variables sorted by address.
 *
 * Given a sorted array of `variable_modbus_t` (sorted by `address`), this function
 * searches for a given address using binary search. It returns the index of the matching
 * variable or -1 if the address is not found.
 *
 * @param[in] modbus_variables Sorted array of Modbus variables.
 * @param[in] low              Lowest index of the search range.
 * @param[in] high             Highest index of the search range.
 * @param[in] value            Address value to search for.
 *
 * @return The index of the variable with the matching address, or -1 if not found.
 *
 * @example
 * ```c
 * variable_modbus_t vars[3] = {
 *     {.address = 1},
 *     {.address = 2},
 *     {.address = 3}
 * };
 * int index = modbus_binary_search(vars, 0, 2, 2);
 * // index = 1
 * ```
 */
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
int modbus_binary_search(variable_modbus_t modbus_variables[], uint16_t low, uint16_t high, uint16_t value);
// NOLINTEND(bugprone-easily-swappable-parameters)

/**
 * @brief Extracts a 32-bit unsigned integer from two holding registers using ABCD order.
 */
uint32_t modbus_get_uint32_abcd(const uint16_t *registers);

/**
 * @brief Extracts a 32-bit unsigned integer from two holding registers using DCBA order.
 */
uint32_t modbus_get_uint32_dcba(const uint16_t *registers);

/**
 * @brief Extracts a 32-bit unsigned integer from two holding registers using BADC order.
 */
uint32_t modbus_get_uint32_badc(const uint16_t *registers);

/**
 * @brief Extracts a 32-bit unsigned integer from two holding registers using CDAB order.
 */
uint32_t modbus_get_uint32_cdab(const uint16_t *registers);

void modbus_set_uint32_abcd(uint32_t value, uint16_t *dest);
void modbus_set_uint32_dcba(uint32_t value, uint16_t *dest);
void modbus_set_uint32_badc(uint32_t value, uint16_t *dest);
void modbus_set_uint32_cdab(uint32_t value, uint16_t *dest);

int32_t modbus_get_int32_abcd(const uint16_t *registers);
int32_t modbus_get_int32_dcba(const uint16_t *registers);
int32_t modbus_get_int32_badc(const uint16_t *registers);
int32_t modbus_get_int32_cdab(const uint16_t *registers);

void modbus_set_int32_abcd(int32_t value, uint16_t *dest);
void modbus_set_int32_dcba(int32_t value, uint16_t *dest);
void modbus_set_int32_badc(int32_t value, uint16_t *dest);
void modbus_set_int32_cdab(int32_t value, uint16_t *dest);

float modbus_get_float_abcd(const uint16_t *registers);
float modbus_get_float_dcba(const uint16_t *registers);
float modbus_get_float_badc(const uint16_t *registers);
float modbus_get_float_cdab(const uint16_t *registers);

void modbus_set_float_abcd(float value, uint16_t *dest);
void modbus_set_float_dcba(float value, uint16_t *dest);
void modbus_set_float_badc(float value, uint16_t *dest);
void modbus_set_float_cdab(float value, uint16_t *dest);

/**
 * @brief Calculates the Modbus CRC-16 using a bit-by-bit algorithm.
 *
 * This function iterates over each byte in the given data buffer and updates the
 * CRC value accordingly. While less efficient than a table-driven approach, it
 * is straightforward and does not require a precomputed table.
 *
 * @param[in] data   Pointer to the data buffer for which to calculate CRC.
 * @param[in] length Number of bytes in the data buffer.
 *
 * @return The calculated CRC-16 value.
 *
 * @note
 * - Bit-by-bit CRC calculation is simple but may be slower for large data buffers.
 *
 * @example
 * ```c
 * uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02};
 * uint16_t crc = modbus_calculate_crc(frame, 6);
 * // crc = 0xC5C9 (example value)
 * ```
 */
uint16_t modbus_calculate_crc(const uint8_t *data, uint16_t length);

/**
 * @brief Calculates the Modbus CRC-16 using a lookup table.
 *
 * This function uses a precomputed lookup table to calculate the Modbus CRC-16
 * more efficiently. It is generally faster than the bit-by-bit approach.
 *
 * @param[in] data   Pointer to the data buffer for which to calculate CRC.
 * @param[in] length Number of bytes in the data buffer.
 *
 * @return The calculated CRC-16 value.
 *
 * @note
 * - Lookup table CRC calculation is faster but requires additional memory for the table.
 * - Ensure that the CRC table is correctly initialized before using this function.
 *
 * @example
 * ```c
 * uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02};
 * uint16_t crc = modbus_crc_with_table(frame, 6);
 * // crc = 0xC5C9 (example value)
 * ```
 */
uint16_t modbus_crc_with_table(const uint8_t *data, uint16_t length);

/**
 * @brief Validates the trailing Modbus CRC-16 in a frame.
 *
 * This helper verifies that the last two bytes of @p frame match the CRC of the
 * preceding bytes. The check uses the table-driven CRC implementation for
 * performance and accepts frames with at least two bytes reserved for the CRC
 * trailer. The CRC stored in the frame is expected in little-endian order
 * (least significant byte first), as defined by the Modbus specification.
 *
 * @param[in] frame   Pointer to the full Modbus frame (PDU/ADU) including CRC.
 * @param[in] length  Total frame length in bytes, including the CRC trailer.
 *
 * @return `true` if the frame pointer is valid, the length is >= 2, and the
 *         CRC matches the calculated value; `false` otherwise.
 */
bool modbus_crc_validate(const uint8_t *frame, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_UTILS_H */

/** @} */
