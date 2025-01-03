// tcp_windows.c

#include "tcp_windows.h"
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

int tcp_init(tcp_handle_t *tcp, int port) {
    WSADATA wsaData;
    int result;

    // Initialize Winsock
    result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (result != 0) {
        printf("[Error] WSAStartup failed with error: %d\n", result);
        return -1;
    }

    // Create a listening socket
    tcp->listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp->listen_socket == INVALID_SOCKET) {
        printf("[Error] Failed to create listening socket. Error Code: %ld\n", WSAGetLastError());
        WSACleanup();
        return -1;
    }

    // Setup the TCP listening socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(server_addr.sin_zero), '\0', 8);

    result = bind(tcp->listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (result == SOCKET_ERROR) {
        printf("[Error] Bind failed with error: %d\n", WSAGetLastError());
        closesocket(tcp->listen_socket);
        WSACleanup();
        return -1;
    }

    result = listen(tcp->listen_socket, 1);
    if (result == SOCKET_ERROR) {
        printf("[Error] Listen failed with error: %d\n", WSAGetLastError());
        closesocket(tcp->listen_socket);
        WSACleanup();
        return -1;
    }

    printf("[Info] TCP server listening on port %d...\n", port);

    // Accept a client socket
    tcp->client_socket = accept(tcp->listen_socket, NULL, NULL);
    if (tcp->client_socket == INVALID_SOCKET) {
        printf("[Error] Accept failed with error: %d\n", WSAGetLastError());
        closesocket(tcp->listen_socket);
        WSACleanup();
        return -1;
    }

    printf("[Info] Client connected.\n");

    return 0;
}

int tcp_write(tcp_handle_t *tcp, uint8_t *data, uint16_t length) {
    int bytes_sent = send(tcp->client_socket, (const char*)data, length, 0);
    if (bytes_sent == SOCKET_ERROR) {
        printf("[Error] Send failed with error: %d\n", WSAGetLastError());
        return -1;
    }

    printf("[Info] Sent %d bytes to client.\n", bytes_sent);
    return bytes_sent;
}

int tcp_read(tcp_handle_t *tcp, uint8_t *buffer, uint16_t length) {
    int bytes_received = recv(tcp->client_socket, (char*)buffer, length, 0);
    if (bytes_received == SOCKET_ERROR) {
        printf("[Error] Receive failed with error: %d\n", WSAGetLastError());
        return -1;
    } else if (bytes_received == 0) {
        printf("[Info] Connection closed by client.\n");
        return 0;
    }

    printf("[Info] Received %d bytes from client.\n", bytes_received);
    return bytes_received;
}

void tcp_close(tcp_handle_t *tcp) {
    // Shutdown the connection since no more data will be sent
    int result = shutdown(tcp->client_socket, SD_SEND);
    if (result == SOCKET_ERROR) {
        printf("[Error] Shutdown failed with error: %d\n", WSAGetLastError());
    }

    // Close the client socket
    closesocket(tcp->client_socket);
    printf("[Info] Client socket closed.\n");

    // Close the listening socket
    closesocket(tcp->listen_socket);
    printf("[Info] Listening socket closed.\n");

    // Cleanup Winsock
    WSACleanup();
    printf("[Info] Winsock cleaned up.\n");
}
