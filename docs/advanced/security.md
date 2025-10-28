# Security Guide

Comprehensive guide to securing Modbus communications with ModbusCore.

## Table of Contents

- [Overview](#overview)
- [Threat Model](#threat-model)
- [TLS/SSL for Modbus TCP](#tlsssl-for-modbus-tcp)
- [Authentication & Authorization](#authentication--authorization)
- [Input Validation](#input-validation)
- [Rate Limiting](#rate-limiting)
- [Attack Mitigation](#attack-mitigation)
- [Security Checklist](#security-checklist)

## Overview

Modbus was designed for **trusted networks** and lacks built-in security. ModbusCore provides mechanisms to add security layers:

- **Transport encryption** (TLS/SSL)
- **Input validation** (bounds checking, sanitization)
- **Rate limiting** (DoS prevention)
- **Authentication** (custom validation layers)
- **Fuzzing** (robustness testing)

**⚠️ Warning:** Modbus over public networks requires additional security measures.

---

## Threat Model

### Common Threats

| Threat | Description | Mitigation |
|--------|-------------|------------|
| **Eavesdropping** | Attacker intercepts traffic | TLS/SSL encryption |
| **Man-in-the-middle** | Attacker modifies packets | TLS with certificate validation |
| **Replay attacks** | Attacker replays captured packets | Transaction IDs, timestamps |
| **DoS** | Flood server with requests | Rate limiting, timeouts |
| **Malformed packets** | Invalid data crashes server | Input validation, fuzzing |
| **Unauthorized access** | Attacker sends commands | Authentication, firewall rules |

### Attack Surface

```
┌─────────────────────────────────┐
│  Network Layer                  │  Eavesdropping, MITM
├─────────────────────────────────┤
│  Transport (TCP/RTU)            │  Connection hijacking
├─────────────────────────────────┤
│  Modbus Protocol (MBAP/PDU)     │  Malformed packets
├─────────────────────────────────┤
│  Application Logic              │  Unauthorized commands
└─────────────────────────────────┘
```

---

## TLS/SSL for Modbus TCP

### Why TLS?

- **Encryption** - Prevents eavesdropping
- **Integrity** - Detects tampering
- **Authentication** - Verifies peer identity

### Implementation

ModbusCore doesn't include TLS directly, but you can integrate with **OpenSSL** or **mbedTLS**.

#### Using OpenSSL

```c
#include <openssl/ssl.h>
#include <openssl/err.h>

SSL_CTX *create_tls_context(void) {
    SSL_CTX *ctx;
    
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    // Create context
    const SSL_METHOD *method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    
    // Load certificates
    if (SSL_CTX_load_verify_locations(ctx, "ca-cert.pem", NULL) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }
    
    // Load client certificate (mutual TLS)
    SSL_CTX_use_certificate_file(ctx, "client-cert.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "client-key.pem", SSL_FILETYPE_PEM);
    
    return ctx;
}

// Custom TLS transport wrapper
typedef struct {
    SSL *ssl;
    int sock;
} tls_ctx_t;

mbc_status_t tls_send(void *ctx, const uint8_t *buffer, size_t length,
                      mbc_transport_io_t *out) {
    tls_ctx_t *tls = (tls_ctx_t*)ctx;
    
    int sent = SSL_write(tls->ssl, buffer, (int)length);
    
    if (sent <= 0) {
        int err = SSL_get_error(tls->ssl, sent);
        if (err == SSL_ERROR_WANT_WRITE) {
            return MBC_STATUS_WOULD_BLOCK;
        }
        return MBC_STATUS_TRANSPORT_ERROR;
    }
    
    if (out) {
        out->processed = (size_t)sent;
    }
    
    return MBC_STATUS_OK;
}

mbc_status_t tls_receive(void *ctx, uint8_t *buffer, size_t capacity,
                         mbc_transport_io_t *out) {
    tls_ctx_t *tls = (tls_ctx_t*)ctx;
    
    int received = SSL_read(tls->ssl, buffer, (int)capacity);
    
    if (received <= 0) {
        int err = SSL_get_error(tls->ssl, received);
        if (err == SSL_ERROR_WANT_READ) {
            return MBC_STATUS_WOULD_BLOCK;
        }
        return MBC_STATUS_TRANSPORT_ERROR;
    }
    
    if (out) {
        out->processed = (size_t)received;
    }
    
    return MBC_STATUS_OK;
}

// Create TLS transport interface
mbc_transport_iface_t create_tls_transport(SSL *ssl, int sock) {
    static tls_ctx_t ctx;
    ctx.ssl = ssl;
    ctx.sock = sock;
    
    mbc_transport_iface_t iface = {
        .ctx = &ctx,
        .send = tls_send,
        .receive = tls_receive,
        .now = /* ... */,
        .yield = NULL
    };
    
    return iface;
}
```

#### Complete TLS Client Example

```c
int main(void) {
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8502);  // Secure Modbus port
    inet_pton(AF_INET, "192.168.1.100", &addr.sin_addr);
    
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    
    // Create TLS context
    SSL_CTX *ssl_ctx = create_tls_context();
    SSL *ssl = SSL_new(ssl_ctx);
    SSL_set_fd(ssl, sock);
    
    // TLS handshake
    if (SSL_connect(ssl) != 1) {
        ERR_print_errors_fp(stderr);
        return 1;
    }
    
    printf("TLS connection established\n");
    printf("Cipher: %s\n", SSL_get_cipher(ssl));
    
    // Create ModbusCore transport with TLS
    mbc_transport_iface_t iface = create_tls_transport(ssl, sock);
    
    // Build runtime
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);
    
    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);
    
    // Use secure Modbus...
    
    // Cleanup
    mbc_runtime_shutdown(&runtime);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ssl_ctx);
    close(sock);
    
    return 0;
}
```

---

## Authentication & Authorization

### Custom Authentication Layer

```c
typedef struct {
    char username[32];
    char password[64];
    bool authenticated;
} auth_ctx_t;

bool authenticate_user(auth_ctx_t *ctx, const char *user, const char *pass) {
    // In production: Use secure password hashing (bcrypt, argon2)
    if (strcmp(ctx->username, user) == 0 &&
        strcmp(ctx->password, pass) == 0) {
        ctx->authenticated = true;
        return true;
    }
    return false;
}

// Wrap transport with authentication
mbc_status_t auth_send(void *ctx, const uint8_t *buffer, size_t length,
                       mbc_transport_io_t *out) {
    auth_ctx_t *auth = (auth_ctx_t*)ctx;
    
    if (!auth->authenticated) {
        return MBC_STATUS_TRANSPORT_ERROR;  // Not authenticated
    }
    
    // Forward to actual transport
    return real_transport_send(buffer, length, out);
}
```

### Role-Based Access Control (RBAC)

```c
typedef enum {
    ROLE_READ_ONLY,
    ROLE_READ_WRITE,
    ROLE_ADMIN
} user_role_t;

bool authorize_operation(user_role_t role, uint8_t function_code) {
    switch (function_code) {
        case 0x01:  // Read coils
        case 0x02:  // Read discrete inputs
        case 0x03:  // Read holding registers
        case 0x04:  // Read input registers
            return (role >= ROLE_READ_ONLY);
        
        case 0x05:  // Write single coil
        case 0x06:  // Write single register
        case 0x0F:  // Write multiple coils
        case 0x10:  // Write multiple registers
            return (role >= ROLE_READ_WRITE);
        
        case 0x08:  // Diagnostics
            return (role >= ROLE_ADMIN);
        
        default:
            return false;  // Deny unknown functions
    }
}

// Use in request handler
void handle_request(user_role_t role, mbc_pdu_t *pdu) {
    if (!authorize_operation(role, pdu->function)) {
        // Return exception: Illegal function
        build_exception_response(pdu, 0x01);
        return;
    }
    
    // Process authorized request...
}
```

---

## Input Validation

### PDU Validation

ModbusCore performs automatic validation:

```c
// Automatic bounds checking
mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 200);

// Returns MBC_STATUS_INVALID_PARAMETER if quantity > 125
if (status != MBC_STATUS_OK) {
    // Invalid parameter detected
}
```

### Custom Validation Layer

```c
bool validate_request(const mbc_pdu_t *pdu) {
    switch (pdu->function) {
        case 0x03:  // Read holding registers
        {
            uint16_t start = (pdu->data[0] << 8) | pdu->data[1];
            uint16_t count = (pdu->data[2] << 8) | pdu->data[3];
            
            // Validate address range
            if (start > 9999 || start + count > 10000) {
                return false;  // Out of range
            }
            
            // Validate quantity
            if (count == 0 || count > 125) {
                return false;  // Invalid quantity
            }
            
            return true;
        }
        
        case 0x10:  // Write multiple registers
        {
            uint16_t start = (pdu->data[0] << 8) | pdu->data[1];
            uint16_t count = (pdu->data[2] << 8) | pdu->data[3];
            uint8_t byte_count = pdu->data[4];
            
            // Validate byte count matches register count
            if (byte_count != count * 2) {
                return false;  // Mismatch
            }
            
            // Validate data length
            if (pdu->data_length != 5 + byte_count) {
                return false;  // Invalid length
            }
            
            return true;
        }
        
        default:
            // Unknown function - reject
            return false;
    }
}
```

### Sanitization

```c
// Sanitize address ranges
uint16_t sanitize_address(uint16_t address, uint16_t max_address) {
    return (address > max_address) ? max_address : address;
}

// Sanitize quantities
uint16_t sanitize_quantity(uint16_t quantity, uint16_t max_quantity) {
    if (quantity == 0) return 1;
    if (quantity > max_quantity) return max_quantity;
    return quantity;
}
```

---

## Rate Limiting

### Token Bucket Algorithm

```c
typedef struct {
    uint32_t tokens;
    uint32_t capacity;
    uint32_t refill_rate;  // tokens per second
    uint64_t last_refill_ms;
} rate_limiter_t;

void rate_limiter_init(rate_limiter_t *limiter, uint32_t capacity,
                       uint32_t refill_rate) {
    limiter->tokens = capacity;
    limiter->capacity = capacity;
    limiter->refill_rate = refill_rate;
    limiter->last_refill_ms = get_time_ms();
}

bool rate_limiter_allow(rate_limiter_t *limiter) {
    uint64_t now_ms = get_time_ms();
    uint64_t elapsed_ms = now_ms - limiter->last_refill_ms;
    
    // Refill tokens
    uint32_t new_tokens = (uint32_t)((elapsed_ms * limiter->refill_rate) / 1000);
    if (new_tokens > 0) {
        limiter->tokens = (limiter->tokens + new_tokens > limiter->capacity)
                        ? limiter->capacity
                        : limiter->tokens + new_tokens;
        limiter->last_refill_ms = now_ms;
    }
    
    // Check if request allowed
    if (limiter->tokens > 0) {
        limiter->tokens--;
        return true;  // Allow
    }
    
    return false;  // Rate limited
}

// Usage
rate_limiter_t limiter;
rate_limiter_init(&limiter, 100, 50);  // 100 tokens, 50/sec refill

while (1) {
    // Receive request...
    
    if (!rate_limiter_allow(&limiter)) {
        // Rate limited - drop or delay request
        send_exception_response(0x06);  // Server device busy
        continue;
    }
    
    // Process request...
}
```

### Per-Client Rate Limiting

```c
typedef struct {
    uint32_t client_ip;
    rate_limiter_t limiter;
} client_rate_limit_t;

#define MAX_CLIENTS 100
client_rate_limit_t client_limiters[MAX_CLIENTS];

bool check_rate_limit(uint32_t client_ip) {
    // Find or create client entry
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_limiters[i].client_ip == client_ip) {
            return rate_limiter_allow(&client_limiters[i].limiter);
        }
    }
    
    // New client - add to table
    // ... (with LRU eviction)
    
    return true;
}
```

---

## Attack Mitigation

### Malformed Packet Detection

```c
bool is_valid_mbap_header(const uint8_t *frame, size_t length) {
    if (length < 7) {
        return false;  // Too short
    }
    
    // Check protocol ID (must be 0)
    uint16_t protocol_id = (frame[2] << 8) | frame[3];
    if (protocol_id != 0) {
        return false;  // Invalid protocol ID
    }
    
    // Check length field
    uint16_t declared_length = (frame[4] << 8) | frame[5];
    if (declared_length + 6 != length) {
        return false;  // Length mismatch
    }
    
    return true;
}
```

### CRC Validation (RTU)

```c
// Always verify CRC before processing
bool frame_valid = mbc_rtu_verify_crc(frame, frame_length);

if (!frame_valid) {
    // Drop invalid frame
    log_security_event("CRC validation failed");
    return MBC_STATUS_CRC_ERROR;
}

// Process valid frame...
```

### Replay Attack Prevention

```c
typedef struct {
    uint16_t last_transaction_id;
    uint64_t last_timestamp_ms;
} replay_detector_t;

bool is_replay_attack(replay_detector_t *detector, uint16_t txid,
                      uint64_t timestamp_ms) {
    // Check if transaction ID is reused too quickly
    if (txid == detector->last_transaction_id &&
        timestamp_ms - detector->last_timestamp_ms < 1000) {
        return true;  // Possible replay
    }
    
    // Update state
    detector->last_transaction_id = txid;
    detector->last_timestamp_ms = timestamp_ms;
    
    return false;
}
```

---

## Security Checklist

### ✅ Network Security

- [ ] Use TLS/SSL for encryption
- [ ] Validate certificates (no self-signed in production)
- [ ] Use firewall rules to restrict access
- [ ] Segment Modbus network from internet
- [ ] Use VPN for remote access
- [ ] Monitor network traffic for anomalies

### ✅ Application Security

- [ ] Validate all inputs (address ranges, quantities)
- [ ] Implement rate limiting
- [ ] Add authentication layer
- [ ] Use role-based access control
- [ ] Log security events
- [ ] Handle exceptions gracefully

### ✅ Protocol Security

- [ ] Verify CRC on all RTU frames
- [ ] Check MBAP header validity
- [ ] Validate PDU structure
- [ ] Detect replay attacks
- [ ] Reject malformed packets
- [ ] Implement timeouts

### ✅ Development Security

- [ ] Compile with security flags (`-fstack-protector`, `-D_FORTIFY_SOURCE=2`)
- [ ] Run static analysis (clang-tidy, cppcheck)
- [ ] Perform fuzz testing (AFL++)
- [ ] Conduct security audits
- [ ] Keep dependencies updated
- [ ] Follow secure coding practices

---

## Hardening Compiler Flags

```cmake
# CMakeLists.txt
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(modbuscore PRIVATE
        -fstack-protector-strong    # Stack canaries
        -D_FORTIFY_SOURCE=2          # Buffer overflow detection
        -fPIE                        # Position-independent executable
        -Wformat                     # Format string vulnerabilities
        -Wformat-security            # More format checks
        -Werror=format-security      # Treat as error
    )
    
    target_link_options(modbuscore PRIVATE
        -Wl,-z,relro                 # Read-only relocations
        -Wl,-z,now                   # Full RELRO
        -pie                         # PIE executable
    )
endif()
```

---

## Incident Response

### Logging Security Events

```c
typedef enum {
    SECURITY_EVENT_AUTH_FAILED,
    SECURITY_EVENT_RATE_LIMITED,
    SECURITY_EVENT_INVALID_PACKET,
    SECURITY_EVENT_CRC_ERROR,
    SECURITY_EVENT_REPLAY_DETECTED
} security_event_t;

void log_security_event(security_event_t event, uint32_t client_ip,
                        const char *details) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
             localtime(&now));
    
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_ip, ip_str, sizeof(ip_str));
    
    syslog(LOG_WARNING, "[SECURITY] %s - %s from %s: %s",
           timestamp, security_event_name(event), ip_str, details);
    
    // Also send to SIEM/monitoring system
    send_alert_to_monitoring(event, client_ip, details);
}
```

### Automatic Response

```c
typedef struct {
    uint32_t client_ip;
    uint32_t violation_count;
    uint64_t ban_until_ms;
} blacklist_entry_t;

void handle_security_violation(uint32_t client_ip) {
    blacklist_entry_t *entry = find_or_create_entry(client_ip);
    
    entry->violation_count++;
    
    if (entry->violation_count > 5) {
        // Ban for 1 hour
        entry->ban_until_ms = get_time_ms() + (60 * 60 * 1000);
        
        log_security_event(SECURITY_EVENT_BLACKLISTED, client_ip,
                          "Exceeded violation threshold");
    }
}
```

---

## Next Steps

- [Embedded Guide](embedded.md) - Deploy securely on embedded systems
- [Fuzzing Guide](fuzzing.md) - Test for security vulnerabilities
- [Performance Guide](performance.md) - Optimize without compromising security

---

**Prev**: [Performance ←](performance.md) | **Next**: [Embedded →](embedded.md)
