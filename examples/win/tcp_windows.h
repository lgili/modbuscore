

#ifndef TCP_WINDOWS_H
#define TCP_WINDOWS_H

#include <stdint.h>
#include <windows.h>

// TCP Handle structure
typedef struct {
    SOCKET listen_socket;
    SOCKET client_socket;
    int port;
} tcp_handle_t;

// Initializes the TCP server and starts listening on the specified port
// Returns 0 on success, non-zero on failure
int tcp_init(tcp_handle_t *tcp, int port);

// Writes data to the connected client
// Returns number of bytes written, or -1 on failure
int tcp_write(tcp_handle_t *tcp, uint8_t *data, uint16_t length);

// Reads data from the connected client
// Returns number of bytes read, or -1 on failure
int tcp_read(tcp_handle_t *tcp, uint8_t *buffer, uint16_t length);

// Closes the TCP connection and cleans up resources
void tcp_close(tcp_handle_t *tcp);

#endif // TCP_WINDOWS_H
