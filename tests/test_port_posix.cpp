#include <array>
#include <thread>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/port/posix.h>
#include <modbus/transport_if.h>
}

#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

TEST(PosixPortTest, WrapsExistingSocket)
{
    int fds[2];
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));

    mb_port_posix_socket_t sock{};
    ASSERT_EQ(MB_OK, mb_port_posix_socket_init(&sock, fds[0], true));

    const mb_transport_if_t *iface = mb_port_posix_socket_iface(&sock);
    ASSERT_NE(nullptr, iface);

    const std::array<mb_u8, 4> payload{0xDE, 0xAD, 0xBE, 0xEF};
    mb_transport_io_result_t io{};
    ASSERT_EQ(MB_OK, mb_transport_send(iface, payload.data(), payload.size(), &io));
    EXPECT_EQ(payload.size(), io.processed);

    std::array<mb_u8, 4> received{};
    ASSERT_EQ((ssize_t)payload.size(), read(fds[1], received.data(), received.size()));
    EXPECT_EQ(payload, received);

    const std::array<mb_u8, 3> reply{0x55, 0xAA, 0x11};
    ASSERT_EQ((ssize_t)reply.size(), write(fds[1], reply.data(), reply.size()))
        << strerror(errno);

    io.processed = 0U;
    ASSERT_EQ(MB_OK, mb_transport_recv(iface, received.data(), received.size(), &io));
    EXPECT_EQ(reply.size(), io.processed);
    EXPECT_TRUE(std::equal(reply.begin(), reply.end(), received.begin()));

    mb_port_posix_socket_close(&sock);
    close(fds[1]);
}

TEST(PosixPortTest, TcpClientConnects)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listen_fd, 0);

    int reuse = 1;
    ASSERT_EQ(0, setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
#if defined(__APPLE__)
    addr.sin_len = sizeof(addr);
#endif
    if (bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        const int bind_errno = errno;
        if (bind_errno == EPERM || bind_errno == EACCES) {
            GTEST_SKIP() << "Socket bind not permitted in sandbox: " << strerror(bind_errno);
            return;
        }
        FAIL() << strerror(bind_errno);
    }
    ASSERT_EQ(0, listen(listen_fd, 1));

    socklen_t len = sizeof(addr);
    ASSERT_EQ(0, getsockname(listen_fd, reinterpret_cast<sockaddr *>(&addr), &len)) << strerror(errno);
    const uint16_t port = ntohs(addr.sin_port);

    int accepted_fd = -1;
    std::thread server_thread([&]() {
        sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);
        accepted_fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&peer), &peer_len);
        if (accepted_fd >= 0) {
            const char greeting[] = {0x01, 0x02, 0x03};
            (void)write(accepted_fd, greeting, sizeof(greeting));
        }
    });

    mb_port_posix_socket_t client_sock{};
    ASSERT_EQ(MB_OK, mb_port_posix_tcp_client(&client_sock, "127.0.0.1", port, 2000U));

    server_thread.join();
    close(listen_fd);

    ASSERT_GE(accepted_fd, 0);

    const mb_transport_if_t *iface = mb_port_posix_socket_iface(&client_sock);
    ASSERT_NE(nullptr, iface);

    std::array<mb_u8, 4> buffer{};
    mb_transport_io_result_t io{};
    ASSERT_EQ(MB_OK, mb_transport_recv(iface, buffer.data(), buffer.size(), &io));
    EXPECT_EQ(3U, io.processed);
    EXPECT_EQ(0x01U, buffer[0]);
    EXPECT_EQ(0x02U, buffer[1]);
    EXPECT_EQ(0x03U, buffer[2]);

    mb_port_posix_socket_close(&client_sock);
    close(accepted_fd);
}

} // namespace
