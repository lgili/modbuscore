/*
 * @file features.h
 * @brief Runtime view of the compile-time feature selection.
 */

#ifndef MODBUS_FEATURES_H
#define MODBUS_FEATURES_H

#include <stdbool.h>
#include <stddef.h>

#include <modbus/mb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Snapshot of the build-time Modbus configuration toggles.
 */
typedef struct mb_feature_profile {
    mb_u8 profile_id;
    const char *profile_name;
    bool build_client;
    bool build_server;
    bool transport_rtu;
    bool transport_ascii;
    bool transport_tcp;
    bool port_mutex;
    bool port_posix;
    const mb_u8 *function_codes;
    size_t function_code_count;
} mb_feature_profile_t;

/**
 * @brief Global feature descriptor emitted into .rodata.
 */
extern const mb_feature_profile_t mb_feature_profile;

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_FEATURES_H */
