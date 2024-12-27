/**
 * @file modbus_utils.h
 * @brief Utility functions for Modbus protocol operations.
 *
 * This file provides helper functions to safely read data from buffers,
 * as well as functions to sort and search arrays of Modbus variables.
 *
 * @author  Luiz Carlos Gili
 * @date    2024-12-20
 *
 * @addtogroup ModbusUtils
 * @{
 */

#ifndef MODBUS_UTILS_H
#define MODBUS_UTILS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif 

#include <modbus/base.h>


/**
 * @brief Safely reads an 8-bit unsigned integer from a buffer.
 *
 * This function checks if there is enough space in the buffer before reading
 * an 8-bit value. If reading would exceed the buffer size, it returns false.
 *
 * @param[in]  buffer       Pointer to the data buffer.
 * @param[in,out] index     Pointer to the current read index in the buffer. It is incremented by one on success.
 * @param[in]  buffer_size  Size of the buffer in bytes.
 * @param[out] value        Pointer to the variable where the read value will be stored.
 *
 * @return True if the value was successfully read, false if not enough data.
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
 * @return True if the value was successfully read, false if not enough data.
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
 */
int modbus_binary_search(variable_modbus_t modbus_variables[], uint16_t low, uint16_t high, uint16_t value);

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
 */
uint16_t modbus_crc_with_table(const uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_UTILS_H */

/** @} */
