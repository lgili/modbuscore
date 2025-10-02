/**
 * @file modbus_utils.c
 * @brief Implementation of utility functions for Modbus protocol operations.
 *
 * This source file implements the helper functions declared in `modbus_utils.h`.
 * It provides safe reading from buffers, as well as sorting and searching
 * functionalities for arrays of Modbus variables. Additionally, it includes
 * functions to calculate CRC-16 checksums, essential for ensuring data integrity
 * in Modbus communications.
 *
 * **Key Features:**
 * - Safe reading of 8-bit and 16-bit unsigned integers from buffers.
 * - Sorting and searching utilities for arrays of `variable_modbus_t`.
 * - CRC-16 calculation using both bit-by-bit and table-driven approaches.
 *
 * **Notes:**
 * - Ensure that the CRC lookup table is correctly initialized before using `modbus_crc_with_table()`.
 * - The selection sort algorithm used in `modbus_selection_sort()` has a time complexity of O(n²).
 *   For larger datasets, consider implementing a more efficient sorting algorithm.
 *
 * @author
 * Luiz Carlos Gili
 * 
 * @date
 * 2024-12-20
 *
 * @ingroup ModbusUtils
 * @{
 */

#include <modbus/utils.h>
#include <stddef.h>

#ifndef CRC_POLYNOMIAL
#define CRC_POLYNOMIAL 0xA001U /**< CRC polynomial used for Modbus CRC-16 calculation */
#endif

/**
 * @brief Precomputed CRC lookup table for Modbus.
 *
 * This table is used by the `modbus_crc_with_table()` function to quickly compute
 * the CRC for each byte of data. It enhances the efficiency of CRC calculations
 * compared to bit-by-bit methods.
 *
 * @note
 * - Ensure that the table remains unchanged to maintain CRC calculation accuracy.
 */
static const uint16_t crc_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

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
bool modbus_read_uint8(const uint8_t *buffer, uint16_t *index, uint16_t buffer_size, uint8_t *value)
{
    if (!buffer || !index || !value) {
        return false; // Invalid arguments
    }

    if (*index >= buffer_size) {
        return false; // Not enough data to read
    }
    *value = buffer[(*index)++];
    return true;
}

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
bool modbus_read_uint16(const uint8_t *buffer, uint16_t *index, uint16_t buffer_size, uint16_t *value)
{
    if (!buffer || !index || !value) {
        return false; // Invalid arguments
    }

    if ((*index + 1U) >= buffer_size) {
        return false; // Not enough data to read
    }
    const uint8_t data_high = buffer[(*index)++];
    const uint8_t data_low  = buffer[(*index)++];
    const uint16_t high_word = (uint16_t)((uint16_t)data_high << 8U);
    const uint16_t low_word = (uint16_t)data_low;
    *value = (uint16_t)(high_word | low_word);
    return true;
}

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
 * - Selection sort has a time complexity of O(n²). For larger datasets, consider implementing a more efficient sorting algorithm like quicksort or mergesort.
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
void modbus_selection_sort(variable_modbus_t modbus_variables[], int length)
{
    if (!modbus_variables || length <= 1) {
        return; // No need to sort
    }

    for (int i = 0; i < (length - 1); i++) {
        int min_idx = i;
        // Find the minimum element in the unsorted part of the array
        for (int j = i + 1; j < length; j++) {
            if (modbus_variables[j].address < modbus_variables[min_idx].address) {
                min_idx = j;
            }
        }
        // Swap the found minimum element with the first element of the unsorted part
        if (min_idx != i) {
            variable_modbus_t temp = modbus_variables[min_idx];
            modbus_variables[min_idx] = modbus_variables[i];
            modbus_variables[i] = temp;
        }
    }
}

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
int modbus_binary_search(variable_modbus_t modbus_variables[], uint16_t low, uint16_t high, uint16_t value)
{
    if (!modbus_variables || low > high) {
        return -1; // Invalid parameters
    }

    while (low <= high) {
        uint16_t mid = low + (high - low) / 2U;

        if (modbus_variables[mid].address == value) {
            return (int)mid;
        }
        else if (modbus_variables[mid].address < value) {
            low = mid + 1U;
        }
        else {
            if (mid == 0) { // Prevent underflow
                break;
            }
            high = mid - 1U;
        }
    }
    return -1; // Not found
}

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
uint16_t modbus_calculate_crc(const uint8_t *data, uint16_t length)
{
    if (!data) {
        return 0xFFFFU; // Default CRC for invalid data
    }

    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8U; j++) {
            if ((crc & 0x0001U) != 0U) {
                crc >>= 1U;
                crc ^= CRC_POLYNOMIAL;
            } else {
                crc >>= 1U;
            }
        }
    }
    return crc;
}

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
uint16_t modbus_crc_with_table(const uint8_t *data, uint16_t length)
{
    if (!data) {
        return 0xFFFFU; // Default CRC for invalid data
    }

    uint16_t crc = 0xFFFFU;
    for (uint16_t i = 0; i < length; i++) {
        uint8_t index = (uint8_t)(crc ^ data[i]);
        crc = (uint16_t)((crc >> 8U) ^ crc_table[index]);
    }
    return crc;
}

/**
 * @brief Validates the trailing CRC of the provided frame.
 */
bool modbus_crc_validate(const uint8_t *frame, uint16_t length)
{
    if (!frame || length < 2U) {
        return false;
    }

    const uint16_t payload_len = (uint16_t)(length - 2U);
    const uint16_t expected_crc = (uint16_t)((uint16_t)frame[payload_len] | ((uint16_t)frame[payload_len + 1U] << 8U));
    const uint16_t computed_crc = modbus_crc_with_table(frame, payload_len);

    return (computed_crc == expected_crc);
}

/** @} */
