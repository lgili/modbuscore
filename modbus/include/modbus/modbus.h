/**
 * @file modbus.h
 * @brief ModbusCore v2.0 - Unified Public API
 *
 * This is the main header for the ModbusCore library. It provides a simplified,
 * unified API for Modbus communication.
 *
 * ## Quick Start
 *
 * @code{.c}
 * #define MB_USE_PROFILE_SIMPLE  // Choose a profile
 * #include <modbus/modbus.h>
 *
 * int main(void) {
 *     MB_AUTO(mb, mb_create_tcp("192.168.1.10:502"));
 *     uint16_t reg;
 *     mb_read_holding(mb, 1, 0, 1, &reg);
 *     printf("Register: %u\n", reg);
 *     return 0;
 * }
 * @endcode
 *
 * ## What's Included
 *
 * - **mb_simple.h**: Simplified unified API (recommended for most users)
 * - **mb_err.h**: Error codes and handling
 * - **mb_types.h**: Basic type definitions
 * - **profiles.h**: Configuration profiles (SIMPLE, EMBEDDED, GATEWAY, FULL)
 * - **conf.h**: Build-time configuration
 *
 * ## Advanced Features (Optional)
 *
 * - **mb_embed.h**: Embedded system optimizations
 * - **mb_isr.h**: Interrupt service routine support
 * - **mb_power.h**: Power management features
 * - **mb_qos.h**: Quality of Service features
 *
 * ## Architecture
 *
 * The library now follows a clean separation:
 * - **Public API**: This header and mb_simple.h (what you see here)
 * - **Internal Implementation**: All implementation details are in internal/
 *
 * You should only include this header or mb_simple.h directly. All other
 * headers are internal implementation details.
 *
 * @version 2.0
 * @date 2025-01-15
 * @author ModbusCore Team
 */

#ifndef MODBUS_H
#define MODBUS_H

/* ========================================================================== */
/*                          CORE PUBLIC API                                   */
/* ========================================================================== */

/* Configuration and profiles */
#include <modbus/profiles.h>  /* Must be included before conf.h */
#include <modbus/conf.h>

/* Essential types and errors */
#include <modbus/mb_types.h>
#include <modbus/mb_err.h>
#include <modbus/version.h>

/* Main simplified API - Recommended for all users */
#include <modbus/mb_simple.h>

/* ========================================================================== */
/*                       OPTIONAL ADVANCED FEATURES                           */
/* ========================================================================== */

/*
 * These headers provide advanced features for specific use cases.
 * Most users don't need to include these - mb_simple.h is sufficient.
 */

/* Define defaults for optional features if not already defined */
#ifndef MB_CONF_EMBED_OPTIMIZATIONS
#define MB_CONF_EMBED_OPTIMIZATIONS 0
#endif

#ifndef MB_CONF_ISR_SUPPORT
#define MB_CONF_ISR_SUPPORT 0
#endif

#ifndef MB_CONF_POWER_MGMT
#define MB_CONF_POWER_MGMT 0
#endif

#ifndef MB_CONF_QOS
#define MB_CONF_QOS 0
#endif

#if MB_CONF_EMBED_OPTIMIZATIONS
#include <modbus/mb_embed.h>   /* Embedded system optimizations */
#endif

#if MB_CONF_ISR_SUPPORT
#include <modbus/mb_isr.h>     /* ISR-safe operations */
#endif

#if MB_CONF_POWER_MGMT
#include <modbus/mb_power.h>   /* Power management */
#endif

#if MB_CONF_QOS
#include <modbus/mb_qos.h>     /* Quality of Service */
#endif

/* ========================================================================== */
/*                          LIBRARY INFORMATION                               */
/* ========================================================================== */

/**
 * @mainpage ModbusCore v2.0 - Simple, Powerful, Robust
 *
 * ModbusCore is a modern, easy-to-use Modbus library for C.
 *
 * ## Key Features
 *
 * - **Simple API**: 3 lines for "Hello World" (down from 50+)
 * - **Profile-based config**: 1 line instead of 50+ flags
 * - **Auto-cleanup**: RAII-style resource management
 * - **Zero breaking changes**: Old APIs still available (internal/)
 * - **Fully featured**: TCP, RTU, ASCII transports
 * - **Production ready**: Robust error handling, diagnostics
 *
 * ## Getting Started
 *
 * See examples/api-simple/hello_world.c for the simplest example.
 * Full documentation: docs/PHASE1_SUMMARY.md
 *
 * ## Migration
 *
 * If you were using the old API (client.h, mb_host.h, etc.), you can:
 * 1. Continue using it - it's still available in internal/
 * 2. Migrate to mb_simple.h - much simpler and easier
 *
 * See docs/SIMPLIFICATION_PROGRESS.md for migration guide.
 */

#endif /* MODBUS_H */
