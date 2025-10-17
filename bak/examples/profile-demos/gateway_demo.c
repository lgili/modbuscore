/**
 * @file gateway_demo.c
 * @brief Demonstration of GATEWAY profile for industrial systems
 *
 * This example shows how to use the GATEWAY profile for high-performance
 * protocol bridging and multi-device scenarios.
 *
 * Features:
 * - ~75KB code, ~6KB RAM
 * - Both client & server (for bridging)
 * - All transports (RTU, TCP, ASCII)
 * - QoS with priority queues
 * - High throughput optimization
 *
 * Build:
 *   cmake --preset profile-gateway
 *   cmake --build --preset profile-gateway
 *   ./build/profile-gateway/examples/profile-demos/gateway_demo
 */

/* STEP 1: Select GATEWAY profile for industrial use */
#define MB_USE_PROFILE_GATEWAY

/* STEP 2: Include what we need */
#include <modbus/client.h>
#include <modbus/server.h>
#include <modbus/mb_err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * This demo simulates a Modbus TCP ↔ RTU gateway:
 * - TCP Server listens for requests from SCADA
 * - RTU Client forwards requests to field devices
 * - Responses are bridged back to SCADA
 */

int main(void)
{
    printf("=== ModbusCore GATEWAY Profile Demo ===\n");
    printf("Profile: GATEWAY (Industrial)\n");
    printf("Features: High performance, QoS, bridging\n");
    printf("Use case: Modbus TCP ↔ RTU gateway\n\n");

    /* Feature check */
    printf("=== GATEWAY Profile Features ===\n");
    printf("Client:        %s (for forwarding to RTU devices)\n",
           MB_CONF_BUILD_CLIENT ? "✓ ENABLED" : "✗ DISABLED");
    printf("Server:        %s (for accepting TCP connections)\n",
           MB_CONF_BUILD_SERVER ? "✓ ENABLED" : "✗ DISABLED");
    printf("RTU:           %s\n", MB_CONF_TRANSPORT_RTU ? "✓" : "✗");
    printf("TCP:           %s\n", MB_CONF_TRANSPORT_TCP ? "✓" : "✗");
    printf("ASCII:         %s\n", MB_CONF_TRANSPORT_ASCII ? "✓" : "✗");
    printf("QoS:           %s (CRITICAL for gateways)\n",
           MB_CONF_ENABLE_QOS ? "✓ ENABLED" : "✗ DISABLED");
    printf("Diagnostics:   %s\n",
           MB_CONF_DIAG_ENABLE_COUNTERS ? "✓ Full" : "Basic");

    printf("\n");

    #if MB_CONF_ENABLE_QOS
    printf("=== QoS Configuration ===\n");
    printf("High priority queue:   %d slots\n", MB_CONF_QOS_HIGH_QUEUE_CAPACITY);
    printf("Normal priority queue: %d slots\n", MB_CONF_QOS_NORMAL_QUEUE_CAPACITY);
    printf("\nQoS prevents head-of-line blocking:\n");
    printf("  • Critical alarms get priority\n");
    printf("  • Normal polling doesn't block urgent requests\n");
    printf("  • Latency SLAs can be met\n");
    #endif

    printf("\n");

    /* Demonstrate gateway architecture */
    printf("=== Gateway Architecture ===\n");
    printf("\n");
    printf("  SCADA/HMI                Field Devices\n");
    printf("     |                           |\n");
    printf("     | Modbus TCP             RTU RS-485\n");
    printf("     |                           |\n");
    printf("     +--> TCP Server    Client --+\n");
    printf("          (this gateway)\n");
    printf("\n");

    printf("Benefits:\n");
    printf("  • Protocol conversion (TCP ↔ RTU)\n");
    printf("  • Multiple TCP clients → single RTU bus\n");
    printf("  • QoS for critical transactions\n");
    printf("  • Full diagnostics for troubleshooting\n");

    printf("\n");

    /* Memory and performance info */
    printf("=== Performance Characteristics ===\n");
    printf("TCP Connections:    %d simultaneous clients\n", MB_TCP_MAX_CONNECTIONS);
    printf("Buffer Size:        %d bytes (high throughput)\n", MODBUS_RECEIVE_BUFFER_SIZE);
    printf("Est. Code Size:     ~75KB\n");
    printf("Est. RAM Usage:     ~6KB static + per-connection overhead\n");

    printf("\n");

    /* Typical gateway workflow */
    printf("=== Typical Gateway Workflow ===\n");
    printf("\n1. Initialize TCP server (listen for SCADA)\n");
    printf("   • Accepts multiple client connections\n");
    printf("   • Each connection can make requests\n");
    printf("\n2. Initialize RTU client (connect to field devices)\n");
    printf("   • Single RS-485 bus with multiple slave IDs\n");
    printf("   • Queues requests from all TCP clients\n");
    printf("\n3. Bridge requests:\n");
    printf("   • Receive Modbus TCP request from SCADA\n");
    printf("   • Extract slave ID and function code\n");
    printf("   • Forward to RTU bus with QoS priority\n");
    printf("   • Wait for RTU response\n");
    printf("   • Send response back via TCP\n");
    printf("\n4. Monitor with diagnostics:\n");
    printf("   • Track requests per device\n");
    printf("   • Monitor error rates\n");
    printf("   • Measure response times\n");
    printf("   • Log exceptions\n");

    printf("\n");

    /* Configuration example */
    printf("=== Configuration Example ===\n\n");
    printf("/* In your gateway code: */\n\n");
    printf("#define MB_USE_PROFILE_GATEWAY\n");
    printf("#include <modbus/modbus.h>\n\n");
    printf("/* TCP Server side */\n");
    printf("mb_server_t tcp_server;\n");
    printf("/* ... initialize TCP transport ... */\n");
    printf("mb_server_init(&tcp_server, tcp_iface, 1, ...);\n\n");
    printf("/* RTU Client side */\n");
    printf("mb_client_t rtu_client;\n");
    printf("/* ... initialize RTU transport ... */\n");
    printf("mb_client_init(&rtu_client, rtu_iface, ...);\n\n");
    printf("/* Bridge loop */\n");
    printf("while (running) {\n");
    printf("    mb_server_poll(&tcp_server);  /* Accept TCP requests */\n");
    printf("    /* Forward to RTU client with QoS */\n");
    printf("    mb_client_poll(&rtu_client);  /* Process RTU queue */\n");
    printf("}\n");

    printf("\n");

    /* Comparison with other profiles */
    printf("=== Why GATEWAY vs Other Profiles? ===\n\n");
    printf("SIMPLE:\n");
    printf("  • Too basic for production gateways\n");
    printf("  • No QoS for prioritization\n");
    printf("  • Single connection focus\n\n");
    printf("EMBEDDED:\n");
    printf("  • Too constrained (client OR server, not both)\n");
    printf("  • Minimal buffers hurt throughput\n");
    printf("  • Missing QoS\n\n");
    printf("GATEWAY: ✓ BEST FIT\n");
    printf("  • Both client & server\n");
    printf("  • QoS for critical transactions\n");
    printf("  • All transports\n");
    printf("  • High throughput\n");
    printf("  • Full diagnostics\n\n");
    printf("FULL:\n");
    printf("  • Overkill (power management not needed)\n");
    printf("  • Larger footprint with no benefit\n");

    printf("\n=== Demo Complete ===\n");
    printf("The GATEWAY profile is perfect for:\n");
    printf("  • Modbus protocol gateways\n");
    printf("  • Industrial PLCs\n");
    printf("  • Multi-device masters\n");
    printf("  • High-throughput systems\n");
    printf("  • Production environments\n");

    return EXIT_SUCCESS;
}
