#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <modbus/port/posix.h>
#include <modbus/transport_if.h>

int main(void)
{
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        perror("socketpair");
        return 1;
    }

    mb_port_posix_socket_t transport;
    if (mb_port_posix_socket_init(&transport, fds[0], true) != MB_OK) {
        fprintf(stderr, "Failed to wrap socket\n");
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    const mb_transport_if_t *iface = mb_port_posix_socket_iface(&transport);
    if (iface == NULL) {
        fprintf(stderr, "POSIX interface unavailable\n");
        mb_port_posix_socket_close(&transport);
        close(fds[1]);
        return 1;
    }

    const char message[] = "Hello from POSIX";
    mb_transport_io_result_t io = {0};
    if (mb_transport_send(iface, (const mb_u8 *)message, (mb_size_t)strlen(message), &io) != MB_OK) {
        fprintf(stderr, "Transport send failed\n");
        mb_port_posix_socket_close(&transport);
        close(fds[1]);
        return 1;
    }

    printf("Sent %zu bytes to peer\n", (size_t)io.processed);

    char peer_buffer[64];
    const ssize_t peer_bytes = read(fds[1], peer_buffer, sizeof(peer_buffer));
    if (peer_bytes < 0) {
        perror("read");
        mb_port_posix_socket_close(&transport);
        close(fds[1]);
        return 1;
    }

    printf("Peer observed: %.*s\n", (int)peer_bytes, peer_buffer);

    const char response[] = "Ack";
    if (write(fds[1], response, sizeof(response)) < 0) {
        perror("write");
        mb_port_posix_socket_close(&transport);
        close(fds[1]);
        return 1;
    }

    io.processed = 0U;
    mb_u8 rx_buf[16];
    const mb_err_t err = mb_transport_recv(iface, rx_buf, sizeof(rx_buf), &io);
    if (err == MB_OK) {
        printf("Received %zu bytes\n", (size_t)io.processed);
        printf("Payload: %.*s\n", (int)io.processed, rx_buf);
    } else if (err == MB_ERR_TIMEOUT) {
        printf("No data available (timeout)\n");
    } else {
        printf("Receive failed (%d)\n", (int)err);
    }

    mb_port_posix_socket_close(&transport);
    close(fds[1]);
    return 0;
}
