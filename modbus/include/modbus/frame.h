/**
 * @file frame.h
 * @brief Helpers to assemble and dissect Modbus Application Data Units.
 */

#ifndef MODBUS_FRAME_H
#define MODBUS_FRAME_H

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Lightweight view over a Modbus ADU payload.
 */
typedef struct {
    mb_u8 unit_id;           /**< Slave / server address (a.k.a. unit identifier). */
    mb_u8 function;          /**< Function code. */
    const mb_u8 *payload;    /**< Pointer to bytes immediately after the function code. */
    mb_size_t payload_len;   /**< Number of payload bytes (may be zero). */
} mb_adu_view_t;

/**
 * @brief Builds an RTU frame from the supplied ADU view.
 */
mb_err_t mb_frame_rtu_encode(const mb_adu_view_t *adu,
                             mb_u8 *out_adu,
                             mb_size_t out_cap,
                             mb_size_t *out_len);

/**
 * @brief Parses an RTU frame, validating CRC and extracting the ADU view.
 */
mb_err_t mb_frame_rtu_decode(const mb_u8 *adu,
                             mb_size_t adu_len,
                             mb_adu_view_t *out);

mb_err_t mb_frame_ascii_encode(const mb_adu_view_t *adu,
                               mb_u8 *out_ascii,
                               mb_size_t out_cap,
                               mb_size_t *out_len);

mb_err_t mb_frame_ascii_decode(const mb_u8 *ascii,
                               mb_size_t ascii_len,
                               mb_adu_view_t *out,
                               mb_u8 *payload_buf,
                               mb_size_t payload_cap);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_FRAME_H */
