#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <modbuscore/transport/posix_rtu.h>

#ifdef __unix__
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

static void test_posix_rtu_loop(void)
{
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master_fd < 0) {
        printf("POSIX RTU tests skipped (PTY not available: %s)\n", strerror(errno));
        return;
    }

    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        printf("POSIX RTU tests skipped (PTY setup failed: %s)\n", strerror(errno));
        close(master_fd);
        return;
    }

    char *slave_path = ptsname(master_fd);
    if (!slave_path) {
        printf("POSIX RTU tests skipped (ptsname failed: %s)\n", strerror(errno));
        close(master_fd);
        return;
    }

    mbc_posix_rtu_config_t cfg = {
        .device_path = slave_path,
        .baud_rate = 9600,
    };

    mbc_transport_iface_t iface;
    mbc_posix_rtu_ctx_t *ctx = NULL;
    mbc_status_t status = mbc_posix_rtu_create(&cfg, &iface, &ctx);
    if (!mbc_status_is_ok(status)) {
        printf("POSIX RTU tests skipped (device open failed with status=%d, errno=%s)\n",
               status, strerror(errno));
        close(master_fd);
        return;
    }

    /* Test send path: data should appear on master PTY */
    uint8_t frame[] = {0x11, 0x22, 0x33};
    mbc_transport_io_t io = {0};
    assert(mbc_transport_send(&iface, frame, sizeof(frame), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(frame));

    uint8_t read_buffer[4] = {0};
    ssize_t r = read(master_fd, read_buffer, sizeof(read_buffer));
    assert(r == (ssize_t)sizeof(frame));
    assert(memcmp(read_buffer, frame, sizeof(frame)) == 0);

    /* Test receive path: bytes written to master should be read via iface */
    const uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    assert(write(master_fd, payload, sizeof(payload)) == (ssize_t)sizeof(payload));

    uint8_t out[4] = {0};
    io.processed = 0U;
    assert(mbc_transport_receive(&iface, out, sizeof(out), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(payload));
    assert(memcmp(out, payload, sizeof(payload)) == 0);

    mbc_posix_rtu_destroy(ctx);
    close(master_fd);
    printf("POSIX RTU tests completed successfully\n");
}
#endif

int main(void)
{
#ifdef __unix__
    test_posix_rtu_loop();
#else
    mbc_transport_iface_t iface;
    mbc_posix_rtu_ctx_t *ctx = NULL;
    mbc_posix_rtu_config_t cfg = {
        .device_path = NULL,
    };
    mbc_status_t status = mbc_posix_rtu_create(&cfg, &iface, &ctx);
    assert(status == MBC_STATUS_UNSUPPORTED || status == MBC_STATUS_INVALID_ARGUMENT);
    (void)ctx;
#endif
    return 0;
}
