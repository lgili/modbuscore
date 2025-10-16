/**
 * @file profiles.h
 * @brief Simplified profile-based configuration for ModbusCore
 *
 * This header provides four predefined profiles that configure the entire library
 * with sensible defaults for common use cases. Users can select a profile with a
 * single define and optionally override specific settings.
 *
 * USAGE:
 * ------
 * Option 1: Via source code (before including modbus headers)
 *   #define MB_USE_PROFILE_SIMPLE
 *   #include <modbus/modbus.h>
 *
 * Option 2: Via CMake
 *   cmake -DMB_PROFILE=SIMPLE ..
 *
 * Option 3: Via compiler flags
 *   gcc -DMB_USE_PROFILE_SIMPLE mycode.c -lmodbus
 *
 * PROFILES:
 * ---------
 * - SIMPLE:    Desktop/testing - maximum simplicity, all features enabled
 * - EMBEDDED:  MCU/IoT - minimal footprint, essential features only
 * - GATEWAY:   Industrial gateway - multi-protocol, performance optimized
 * - FULL:      Maximum features - everything enabled for advanced use cases
 *
 * @note If no profile is selected, defaults to SIMPLE for easy getting started.
 */

#ifndef MODBUS_PROFILES_H
#define MODBUS_PROFILES_H

/* ========================================================================== */
/*                            PROFILE SELECTION                               */
/* ========================================================================== */

/**
 * Define exactly ONE of these before including this header:
 * - MB_USE_PROFILE_SIMPLE
 * - MB_USE_PROFILE_EMBEDDED
 * - MB_USE_PROFILE_GATEWAY
 * - MB_USE_PROFILE_FULL
 * - MB_USE_PROFILE_CUSTOM (then define all settings manually)
 */

#if !defined(MB_USE_PROFILE_SIMPLE) && \
    !defined(MB_USE_PROFILE_EMBEDDED) && \
    !defined(MB_USE_PROFILE_GATEWAY) && \
    !defined(MB_USE_PROFILE_FULL) && \
    !defined(MB_USE_PROFILE_CUSTOM)
    /* Default to SIMPLE if nothing specified */
    #define MB_USE_PROFILE_SIMPLE
#endif

/* Validate only one profile is selected */
#ifdef MB_USE_PROFILE_SIMPLE
    #define MB_PROFILE_COUNT 1
#elif defined(MB_USE_PROFILE_EMBEDDED)
    #define MB_PROFILE_COUNT 1
#elif defined(MB_USE_PROFILE_GATEWAY)
    #define MB_PROFILE_COUNT 1
#elif defined(MB_USE_PROFILE_FULL)
    #define MB_PROFILE_COUNT 1
#elif defined(MB_USE_PROFILE_CUSTOM)
    #define MB_PROFILE_COUNT 1
#else
    #define MB_PROFILE_COUNT 0
#endif

#if MB_PROFILE_COUNT != 1
#error "Exactly ONE profile must be defined (SIMPLE, EMBEDDED, GATEWAY, FULL, or CUSTOM)"
#endif

/* ========================================================================== */
/*                       PROFILE: SIMPLE (Desktop/Testing)                    */
/* ========================================================================== */
/**
 * TARGET: Desktop applications, quick prototyping, testing, CLI tools
 * SIZE:   ~85KB code, ~4KB RAM
 * FEATURES: All enabled for maximum ease of use
 */
#ifdef MB_USE_PROFILE_SIMPLE

/* Basic Settings */
#ifndef MB_CONF_BUILD_CLIENT
#define MB_CONF_BUILD_CLIENT                 1
#endif
#ifndef MB_CONF_BUILD_SERVER
#define MB_CONF_BUILD_SERVER                 1
#endif
#ifndef MB_CONF_TRANSPORT_RTU
#define MB_CONF_TRANSPORT_RTU                1
#endif
#ifndef MB_CONF_TRANSPORT_TCP
#define MB_CONF_TRANSPORT_TCP                1
#endif
#ifndef MB_CONF_TRANSPORT_ASCII
#define MB_CONF_TRANSPORT_ASCII              1
#endif

/* Function Codes - All enabled */
#ifndef MB_CONF_ENABLE_FC01
#define MB_CONF_ENABLE_FC01                  1  /* Read Coils */
#endif
#ifndef MB_CONF_ENABLE_FC02
#define MB_CONF_ENABLE_FC02                  1  /* Read Discrete Inputs */
#endif
#ifndef MB_CONF_ENABLE_FC03
#define MB_CONF_ENABLE_FC03                  1  /* Read Holding Registers */
#endif
#ifndef MB_CONF_ENABLE_FC04
#define MB_CONF_ENABLE_FC04                  1  /* Read Input Registers */
#endif
#ifndef MB_CONF_ENABLE_FC05
#define MB_CONF_ENABLE_FC05                  1  /* Write Single Coil */
#endif
#ifndef MB_CONF_ENABLE_FC06
#define MB_CONF_ENABLE_FC06                  1  /* Write Single Register */
#endif
#ifndef MB_CONF_ENABLE_FC07
#define MB_CONF_ENABLE_FC07                  1  /* Read Exception Status */
#endif
#ifndef MB_CONF_ENABLE_FC0F
#define MB_CONF_ENABLE_FC0F                  1  /* Write Multiple Coils */
#endif
#ifndef MB_CONF_ENABLE_FC10
#define MB_CONF_ENABLE_FC10                  1  /* Write Multiple Registers */
#endif
#ifndef MB_CONF_ENABLE_FC11
#define MB_CONF_ENABLE_FC11                  1  /* Report Server ID */
#endif
#ifndef MB_CONF_ENABLE_FC16
#define MB_CONF_ENABLE_FC16                  1  /* Mask Write Register */
#endif
#ifndef MB_CONF_ENABLE_FC17
#define MB_CONF_ENABLE_FC17                  1  /* Read/Write Multiple Registers */
#endif

/* Diagnostics & Observability */
#ifndef MB_CONF_DIAG_ENABLE_COUNTERS
#define MB_CONF_DIAG_ENABLE_COUNTERS         1
#endif
#ifndef MB_CONF_DIAG_ENABLE_TRACE
#define MB_CONF_DIAG_ENABLE_TRACE            1
#endif
#ifndef MB_CONF_DIAG_TRACE_DEPTH
#define MB_CONF_DIAG_TRACE_DEPTH             128
#endif

/* Advanced Features */
#ifndef MB_CONF_ENABLE_POWER_MANAGEMENT
#define MB_CONF_ENABLE_POWER_MANAGEMENT      1
#endif
#ifndef MB_CONF_ENABLE_COMPAT_LIBMODBUS
#define MB_CONF_ENABLE_COMPAT_LIBMODBUS      1
#endif
#ifndef MB_CONF_ENABLE_IOVEC_STATS
#define MB_CONF_ENABLE_IOVEC_STATS           1
#endif
#ifndef MB_CONF_ENABLE_QOS
#define MB_CONF_ENABLE_QOS                   0  /* Not needed for simple use */
#endif
#ifndef MB_CONF_ENABLE_ISR_MODE
#define MB_CONF_ENABLE_ISR_MODE              0  /* Not needed on desktop */
#endif
#ifndef MB_CONF_ENABLE_ASSERTIONS
#define MB_CONF_ENABLE_ASSERTIONS            1  /* Helpful during development */
#endif

/* Buffer Sizes - Generous for desktop */
#ifndef MODBUS_RECEIVE_BUFFER_SIZE
#define MODBUS_RECEIVE_BUFFER_SIZE           512
#endif
#ifndef MODBUS_SEND_BUFFER_SIZE
#define MODBUS_SEND_BUFFER_SIZE              512
#endif
#ifndef MAX_SIZE_HOLDING_REGISTERS
#define MAX_SIZE_HOLDING_REGISTERS           256
#endif
#ifndef MB_TCP_MAX_CONNECTIONS
#define MB_TCP_MAX_CONNECTIONS               8
#endif

/* Timeouts */
#ifndef MASTER_DEFAULT_TIMEOUT_MS
#define MASTER_DEFAULT_TIMEOUT_MS            1000
#endif
#ifndef MB_CONF_CLIENT_SUBSTATE_DEADLINE_MS
#define MB_CONF_CLIENT_SUBSTATE_DEADLINE_MS  5U
#endif
#ifndef MB_CONF_SERVER_SUBSTATE_DEADLINE_MS
#define MB_CONF_SERVER_SUBSTATE_DEADLINE_MS  5U
#endif

/* Internal Profile Marker */
#ifndef MB_CONF_PROFILE
#define MB_CONF_PROFILE                      MB_CONF_PROFILE_FULL
#endif

#endif /* MB_USE_PROFILE_SIMPLE */

/* ========================================================================== */
/*                      PROFILE: EMBEDDED (MCU/IoT)                           */
/* ========================================================================== */
/**
 * TARGET: Microcontrollers (STM32, ESP32, nRF), battery-powered devices
 * SIZE:   ~26KB code, ~1.5KB RAM
 * FEATURES: Essential only - client OR server, RTU preferred, minimal diagnostics
 */
#ifdef MB_USE_PROFILE_EMBEDDED

/* Basic Settings - Choose client OR server (not both to save memory) */
#ifndef MB_CONF_BUILD_CLIENT
#define MB_CONF_BUILD_CLIENT                 1  /* Default: client for IoT sensors */
#endif
#ifndef MB_CONF_BUILD_SERVER
#define MB_CONF_BUILD_SERVER                 0  /* Disable server to save ~8KB */
#endif

/* Transports - RTU preferred for embedded (no TCP overhead) */
#define MB_CONF_TRANSPORT_RTU                1
#define MB_CONF_TRANSPORT_TCP                0  /* Save ~6KB by disabling TCP */
#define MB_CONF_TRANSPORT_ASCII              0  /* Rarely used, save ~2KB */

/* Function Codes - Only most common ones */
#define MB_CONF_ENABLE_FC01                  1  /* Read Coils */
#define MB_CONF_ENABLE_FC02                  0  /* Discrete inputs less common */
#define MB_CONF_ENABLE_FC03                  1  /* Read Holding - ESSENTIAL */
#define MB_CONF_ENABLE_FC04                  1  /* Read Input - ESSENTIAL */
#define MB_CONF_ENABLE_FC05                  1  /* Write Single Coil */
#define MB_CONF_ENABLE_FC06                  1  /* Write Single Register */
#define MB_CONF_ENABLE_FC07                  0  /* Exception status rarely used */
#define MB_CONF_ENABLE_FC0F                  1  /* Write Multiple Coils */
#define MB_CONF_ENABLE_FC10                  1  /* Write Multiple Registers */
#define MB_CONF_ENABLE_FC11                  0  /* Report ID not needed */
#define MB_CONF_ENABLE_FC16                  0  /* Mask write less common */
#define MB_CONF_ENABLE_FC17                  0  /* R/W multiple less common */

/* Diagnostics - Minimal for production */
#define MB_CONF_DIAG_ENABLE_COUNTERS         1  /* Keep for basic monitoring */
#define MB_CONF_DIAG_ENABLE_TRACE            0  /* Disable to save RAM */
#define MB_CONF_DIAG_TRACE_DEPTH             0

/* Advanced Features */
#define MB_CONF_ENABLE_POWER_MANAGEMENT      1  /* CRITICAL for battery devices */
#define MB_CONF_ENABLE_COMPAT_LIBMODBUS      0  /* Save code space */
#define MB_CONF_ENABLE_IOVEC_STATS           0  /* No overhead */
#define MB_CONF_ENABLE_QOS                   0  /* Not needed for simple devices */
#define MB_CONF_ENABLE_ISR_MODE              1  /* Important for fast MCU response */
#define MB_CONF_ENABLE_ASSERTIONS            0  /* Disable in production */

/* Buffer Sizes - Tight for embedded */
#define MODBUS_RECEIVE_BUFFER_SIZE           128
#define MODBUS_SEND_BUFFER_SIZE              128
#define MAX_SIZE_HOLDING_REGISTERS           32
#define MB_TCP_MAX_CONNECTIONS               0  /* TCP disabled */

/* Timeouts - Faster for embedded */
#define MASTER_DEFAULT_TIMEOUT_MS            500
#define MB_CONF_CLIENT_SUBSTATE_DEADLINE_MS  1U
#define MB_CONF_SERVER_SUBSTATE_DEADLINE_MS  1U

/* Internal Profile Marker */
#define MB_CONF_PROFILE                      MB_CONF_PROFILE_TINY

#endif /* MB_USE_PROFILE_EMBEDDED */

/* ========================================================================== */
/*                    PROFILE: GATEWAY (Industrial)                           */
/* ========================================================================== */
/**
 * TARGET: Industrial gateways, protocol converters, multi-slave masters
 * SIZE:   ~75KB code, ~6KB RAM
 * FEATURES: Both client & server, all transports, QoS, high performance
 */
#ifdef MB_USE_PROFILE_GATEWAY

/* Basic Settings - Both client and server for bridging */
#define MB_CONF_BUILD_CLIENT                 1
#define MB_CONF_BUILD_SERVER                 1
#define MB_CONF_TRANSPORT_RTU                1
#define MB_CONF_TRANSPORT_TCP                1
#define MB_CONF_TRANSPORT_ASCII              1  /* Some legacy systems need it */

/* Function Codes - All common ones */
#define MB_CONF_ENABLE_FC01                  1
#define MB_CONF_ENABLE_FC02                  1
#define MB_CONF_ENABLE_FC03                  1
#define MB_CONF_ENABLE_FC04                  1
#define MB_CONF_ENABLE_FC05                  1
#define MB_CONF_ENABLE_FC06                  1
#define MB_CONF_ENABLE_FC07                  1
#define MB_CONF_ENABLE_FC0F                  1
#define MB_CONF_ENABLE_FC10                  1
#define MB_CONF_ENABLE_FC11                  1
#define MB_CONF_ENABLE_FC16                  1
#define MB_CONF_ENABLE_FC17                  1

/* Diagnostics - Important for industrial monitoring */
#define MB_CONF_DIAG_ENABLE_COUNTERS         1
#define MB_CONF_DIAG_ENABLE_TRACE            1
#define MB_CONF_DIAG_TRACE_DEPTH             64

/* Advanced Features - Performance critical */
#define MB_CONF_ENABLE_POWER_MANAGEMENT      0  /* Gateways are always powered */
#define MB_CONF_ENABLE_COMPAT_LIBMODBUS      1
#define MB_CONF_ENABLE_IOVEC_STATS           1
#define MB_CONF_ENABLE_QOS                   1  /* CRITICAL for prioritization */
#define MB_CONF_ENABLE_ISR_MODE              0  /* Usually Linux/RTOS based */
#define MB_CONF_ENABLE_ASSERTIONS            1  /* Helpful in production */

/* Buffer Sizes - Large for high throughput */
#define MODBUS_RECEIVE_BUFFER_SIZE           512
#define MODBUS_SEND_BUFFER_SIZE              512
#define MAX_SIZE_HOLDING_REGISTERS           512
#define MB_TCP_MAX_CONNECTIONS               16  /* Multiple TCP clients */

/* QoS Configuration */
#define MB_CONF_QOS_HIGH_QUEUE_CAPACITY      16
#define MB_CONF_QOS_NORMAL_QUEUE_CAPACITY    64

/* Timeouts - Standard for industrial */
#define MASTER_DEFAULT_TIMEOUT_MS            1000
#define MB_CONF_CLIENT_SUBSTATE_DEADLINE_MS  2U
#define MB_CONF_SERVER_SUBSTATE_DEADLINE_MS  2U

/* Internal Profile Marker */
#define MB_CONF_PROFILE                      MB_CONF_PROFILE_FULL

#endif /* MB_USE_PROFILE_GATEWAY */

/* ========================================================================== */
/*                         PROFILE: FULL (Everything)                         */
/* ========================================================================== */
/**
 * TARGET: Development, testing, maximum flexibility
 * SIZE:   ~100KB code, ~8KB RAM
 * FEATURES: Everything enabled - useful for testing and feature exploration
 */
#ifdef MB_USE_PROFILE_FULL

/* Basic Settings - Everything */
#define MB_CONF_BUILD_CLIENT                 1
#define MB_CONF_BUILD_SERVER                 1
#define MB_CONF_TRANSPORT_RTU                1
#define MB_CONF_TRANSPORT_TCP                1
#define MB_CONF_TRANSPORT_ASCII              1

/* Function Codes - All */
#define MB_CONF_ENABLE_FC01                  1
#define MB_CONF_ENABLE_FC02                  1
#define MB_CONF_ENABLE_FC03                  1
#define MB_CONF_ENABLE_FC04                  1
#define MB_CONF_ENABLE_FC05                  1
#define MB_CONF_ENABLE_FC06                  1
#define MB_CONF_ENABLE_FC07                  1
#define MB_CONF_ENABLE_FC0F                  1
#define MB_CONF_ENABLE_FC10                  1
#define MB_CONF_ENABLE_FC11                  1
#define MB_CONF_ENABLE_FC16                  1
#define MB_CONF_ENABLE_FC17                  1

/* Diagnostics - Maximum */
#define MB_CONF_DIAG_ENABLE_COUNTERS         1
#define MB_CONF_DIAG_ENABLE_TRACE            1
#define MB_CONF_DIAG_TRACE_DEPTH             256

/* Advanced Features - All */
#define MB_CONF_ENABLE_POWER_MANAGEMENT      1
#define MB_CONF_ENABLE_COMPAT_LIBMODBUS      1
#define MB_CONF_ENABLE_IOVEC_STATS           1
#define MB_CONF_ENABLE_QOS                   1
#define MB_CONF_ENABLE_ISR_MODE              1
#define MB_CONF_ENABLE_ASSERTIONS            1

/* Buffer Sizes - Maximum */
#define MODBUS_RECEIVE_BUFFER_SIZE           1024
#define MODBUS_SEND_BUFFER_SIZE              1024
#define MAX_SIZE_HOLDING_REGISTERS           1024
#define MB_TCP_MAX_CONNECTIONS               32

/* QoS Configuration - Large queues */
#define MB_CONF_QOS_HIGH_QUEUE_CAPACITY      32
#define MB_CONF_QOS_NORMAL_QUEUE_CAPACITY    128

/* Timeouts */
#define MASTER_DEFAULT_TIMEOUT_MS            2000
#define MB_CONF_CLIENT_SUBSTATE_DEADLINE_MS  5U
#define MB_CONF_SERVER_SUBSTATE_DEADLINE_MS  5U

/* Internal Profile Marker */
#define MB_CONF_PROFILE                      MB_CONF_PROFILE_FULL

#endif /* MB_USE_PROFILE_FULL */

/* ========================================================================== */
/*                         PROFILE: CUSTOM                                    */
/* ========================================================================== */
/**
 * If MB_USE_PROFILE_CUSTOM is defined, user must define all settings manually.
 * This profile doesn't set any defaults - it's for advanced users who want
 * complete control.
 */
#ifdef MB_USE_PROFILE_CUSTOM
    /* No defaults - user must define everything in their project */
    #ifndef MB_CONF_PROFILE
        #define MB_CONF_PROFILE MB_CONF_PROFILE_CUSTOM
    #endif
#endif

/* ========================================================================== */
/*                       PROFILE COMPATIBILITY CHECK                          */
/* ========================================================================== */

/* Ensure critical settings are defined */
#ifndef MB_CONF_BUILD_CLIENT
    #error "MB_CONF_BUILD_CLIENT must be defined"
#endif

#ifndef MB_CONF_BUILD_SERVER
    #error "MB_CONF_BUILD_SERVER must be defined"
#endif

#if !MB_CONF_BUILD_CLIENT && !MB_CONF_BUILD_SERVER
    #error "At least one of MB_CONF_BUILD_CLIENT or MB_CONF_BUILD_SERVER must be enabled"
#endif

#ifndef MB_CONF_TRANSPORT_RTU
    #error "MB_CONF_TRANSPORT_RTU must be defined"
#endif

#ifndef MB_CONF_TRANSPORT_TCP
    #error "MB_CONF_TRANSPORT_TCP must be defined"
#endif

#if !MB_CONF_TRANSPORT_RTU && !MB_CONF_TRANSPORT_TCP && !MB_CONF_TRANSPORT_ASCII
    #error "At least one transport must be enabled"
#endif

/* ========================================================================== */
/*                          PROFILE SUMMARY                                   */
/* ========================================================================== */

/**
 * Summary of selected profile (useful for debugging):
 * - Profile name
 * - Memory footprint estimate
 * - Key features enabled
 */

#if defined(MB_USE_PROFILE_SIMPLE)
    #define MB_PROFILE_NAME "SIMPLE"
    #define MB_PROFILE_DESCRIPTION "Desktop/Testing - All features enabled"
    #define MB_PROFILE_CODE_SIZE_KB 85
    #define MB_PROFILE_RAM_SIZE_KB 4
#elif defined(MB_USE_PROFILE_EMBEDDED)
    #define MB_PROFILE_NAME "EMBEDDED"
    #define MB_PROFILE_DESCRIPTION "MCU/IoT - Minimal footprint"
    #define MB_PROFILE_CODE_SIZE_KB 26
    #define MB_PROFILE_RAM_SIZE_KB 1
#elif defined(MB_USE_PROFILE_GATEWAY)
    #define MB_PROFILE_NAME "GATEWAY"
    #define MB_PROFILE_DESCRIPTION "Industrial - High performance"
    #define MB_PROFILE_CODE_SIZE_KB 75
    #define MB_PROFILE_RAM_SIZE_KB 6
#elif defined(MB_USE_PROFILE_FULL)
    #define MB_PROFILE_NAME "FULL"
    #define MB_PROFILE_DESCRIPTION "Development - Everything enabled"
    #define MB_PROFILE_CODE_SIZE_KB 100
    #define MB_PROFILE_RAM_SIZE_KB 8
#elif defined(MB_USE_PROFILE_CUSTOM)
    #define MB_PROFILE_NAME "CUSTOM"
    #define MB_PROFILE_DESCRIPTION "User-defined configuration"
    #define MB_PROFILE_CODE_SIZE_KB 0  /* Unknown */
    #define MB_PROFILE_RAM_SIZE_KB 0   /* Unknown */
#endif

/**
 * Helper macro to print profile info at compile time (optional)
 * Usage: Add -DMB_PROFILE_PRINT_INFO to compiler flags
 */
#ifdef MB_PROFILE_PRINT_INFO
    #pragma message("ModbusCore Profile: " MB_PROFILE_NAME)
    #pragma message("  Description: " MB_PROFILE_DESCRIPTION)
    #define MB_STRINGIFY(x) #x
    #define MB_TOSTRING(x) MB_STRINGIFY(x)
    #pragma message("  Estimated code size: ~" MB_TOSTRING(MB_PROFILE_CODE_SIZE_KB) " KB")
    #pragma message("  Estimated RAM usage: ~" MB_TOSTRING(MB_PROFILE_RAM_SIZE_KB) " KB")
#endif

#endif /* MODBUS_PROFILES_H */
