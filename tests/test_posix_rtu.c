#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <modbuscore/transport/posix_rtu.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/crc.h>

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

static bool read_exact(int fd, uint8_t *buffer, size_t length)
{
    size_t total = 0U;
    while (total < length) {
        ssize_t n = read(fd, buffer + total, length - total);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            return false;
        }
        if (n == 0) {
            return false;
        }
        total += (size_t)n;
    }
    return true;
}

static void test_posix_rtu_engine_client(void)
{
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master_fd < 0) {
        printf("POSIX RTU engine client test skipped (PTY not available: %s)\n", strerror(errno));
        return;
    }

    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        printf("POSIX RTU engine client test skipped (PTY setup failed: %s)\n", strerror(errno));
        close(master_fd);
        return;
    }

    char *slave_path = ptsname(master_fd);
    if (!slave_path) {
        printf("POSIX RTU engine client test skipped (ptsname failed: %s)\n", strerror(errno));
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
        printf("POSIX RTU engine client test skipped (device open failed with status=%d, errno=%s)\n",
               status, strerror(errno));
        close(master_fd);
        return;
    }

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
        .response_timeout_ms = 1000,
    };
    assert(mbc_engine_init(&engine, &engine_cfg) == MBC_STATUS_OK);

    /* Build RTU request: [Unit ID][FC][Start Addr H][Start Addr L][Qty H][Qty L] */
    /* FC03: Read Holding Registers, Unit=0x11, Start=0, Qty=1 */
    uint8_t request_frame[] = {0x11, 0x03, 0x00, 0x00, 0x00, 0x01};

    /* Submit request frame */
    assert(mbc_engine_submit_request(&engine, request_frame, sizeof(request_frame)) == MBC_STATUS_OK);

    uint8_t observed_request[sizeof(request_frame) + 2U];
    assert(read_exact(master_fd, observed_request, sizeof(observed_request)));
    assert(memcmp(observed_request, request_frame, sizeof(request_frame)) == 0);
    uint16_t observed_crc = (uint16_t)((uint16_t)observed_request[sizeof(request_frame)] |
                                       ((uint16_t)observed_request[sizeof(request_frame) + 1U] << 8));
    uint16_t expected_crc = mbc_crc16(request_frame, sizeof(request_frame));
    assert(observed_crc == expected_crc);

    /* Simulate server response on master PTY */
    /* FC03 response: [Unit ID][FC][Byte Count][Data H][Data L] */
    uint8_t response_frame[] = {0x11, 0x03, 0x02, 0x00, 0x2A};
    uint8_t response_with_crc[sizeof(response_frame) + 2U];
    memcpy(response_with_crc, response_frame, sizeof(response_frame));
    uint16_t response_crc = mbc_crc16(response_frame, sizeof(response_frame));
    response_with_crc[sizeof(response_frame)] = (uint8_t)(response_crc & 0xFFU);
    response_with_crc[sizeof(response_frame) + 1U] = (uint8_t)((response_crc >> 8) & 0xFFU);
    ssize_t w = write(master_fd, response_with_crc, sizeof(response_with_crc));
    assert(w == (ssize_t)sizeof(response_with_crc));

    /* Engine receives and decodes response */
    mbc_pdu_t response_pdu = {0};
    bool response_ready = false;
    for (int i = 0; i < 10 && !response_ready; ++i) {
        status = mbc_engine_step(&engine, sizeof(response_with_crc));
        assert(status == MBC_STATUS_OK);
        response_ready = mbc_engine_take_pdu(&engine, &response_pdu);
        if (!response_ready) {
            mbc_transport_yield(&iface);
            usleep(1000);
        }
    }
    assert(response_ready);

    const uint8_t *register_data = NULL;
    size_t register_count = 0U;
    assert(mbc_pdu_parse_read_holding_response(&response_pdu,
                                               &register_data,
                                               &register_count) == MBC_STATUS_OK);
    assert(register_count == 1U);
    assert(register_data[0] == 0x00U && register_data[1] == 0x2AU);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_rtu_destroy(ctx);
    close(master_fd);
}

static void test_posix_rtu_engine_client_crc_error(void)
{
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master_fd < 0) {
        printf("POSIX RTU engine client CRC error test skipped (PTY not available: %s)\n", strerror(errno));
        return;
    }

    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        printf("POSIX RTU engine client CRC error test skipped (PTY setup failed: %s)\n", strerror(errno));
        close(master_fd);
        return;
    }

    char *slave_path = ptsname(master_fd);
    if (!slave_path) {
        printf("POSIX RTU engine client CRC error test skipped (ptsname failed: %s)\n", strerror(errno));
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
        printf("POSIX RTU engine client CRC error test skipped (device open failed with status=%d, errno=%s)\n",
               status, strerror(errno));
        close(master_fd);
        return;
    }

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
        .response_timeout_ms = 100,
    };
    assert(mbc_engine_init(&engine, &engine_cfg) == MBC_STATUS_OK);

    uint8_t request_frame[] = {0x11, 0x03, 0x00, 0x00, 0x00, 0x01};
    assert(mbc_engine_submit_request(&engine, request_frame, sizeof(request_frame)) == MBC_STATUS_OK);

    uint8_t observed_request[sizeof(request_frame) + 2U];
    assert(read_exact(master_fd, observed_request, sizeof(observed_request)));

    uint8_t response_frame[] = {0x11, 0x03, 0x02, 0x00, 0x2A};
    uint16_t crc = mbc_crc16(response_frame, sizeof(response_frame));
    crc ^= 0xFFFFU; /* Corrupt CRC intentionally */
    uint8_t response_corrupt[sizeof(response_frame) + 2U];
    memcpy(response_corrupt, response_frame, sizeof(response_frame));
    response_corrupt[sizeof(response_frame)] = (uint8_t)(crc & 0xFFU);
    response_corrupt[sizeof(response_frame) + 1U] = (uint8_t)((crc >> 8) & 0xFFU);
    assert(write(master_fd, response_corrupt, sizeof(response_corrupt)) == (ssize_t)sizeof(response_corrupt));

    bool saw_error = false;
    for (int i = 0; i < 10 && !saw_error; ++i) {
        status = mbc_engine_step(&engine, sizeof(response_corrupt));
        if (status == MBC_STATUS_DECODING_ERROR) {
            saw_error = true;
            break;
        }
        assert(status == MBC_STATUS_OK);
        mbc_transport_yield(&iface);
        usleep(1000);
    }

    assert(saw_error);
    mbc_pdu_t out = {0};
    assert(!mbc_engine_take_pdu(&engine, &out));

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_rtu_destroy(ctx);
    close(master_fd);
}

static void test_posix_rtu_engine_server(void)
{
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master_fd < 0) {
        printf("POSIX RTU engine server test skipped (PTY not available: %s)\n", strerror(errno));
        return;
    }

    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        printf("POSIX RTU engine server test skipped (PTY setup failed: %s)\n", strerror(errno));
        close(master_fd);
        return;
    }

    char *slave_path = ptsname(master_fd);
    if (!slave_path) {
        printf("POSIX RTU engine server test skipped (ptsname failed: %s)\n", strerror(errno));
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
        printf("POSIX RTU engine server test skipped (device open failed with status=%d, errno=%s)\n",
               status, strerror(errno));
        close(master_fd);
        return;
    }

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_SERVER,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
    };
    assert(mbc_engine_init(&engine, &engine_cfg) == MBC_STATUS_OK);

    /* Simulate client request on master PTY */
    /* FC03: Read Holding Registers, Unit=0x11, Start=0, Qty=1 */
    uint8_t request_frame[] = {0x11, 0x03, 0x00, 0x00, 0x00, 0x01};
    uint8_t request_with_crc[sizeof(request_frame) + 2U];
    memcpy(request_with_crc, request_frame, sizeof(request_frame));
    uint16_t request_crc = mbc_crc16(request_frame, sizeof(request_frame));
    request_with_crc[sizeof(request_frame)] = (uint8_t)(request_crc & 0xFFU);
    request_with_crc[sizeof(request_frame) + 1U] = (uint8_t)((request_crc >> 8) & 0xFFU);
    ssize_t w = write(master_fd, request_with_crc, sizeof(request_with_crc));
    assert(w == (ssize_t)sizeof(request_with_crc));

    /* Engine receives and decodes request */
    mbc_pdu_t decoded_request = {0};
    bool has_request = false;
    for (int i = 0; i < 10 && !has_request; ++i) {
        status = mbc_engine_step(&engine, sizeof(request_with_crc));
        assert(status == MBC_STATUS_OK);
        has_request = mbc_engine_take_pdu(&engine, &decoded_request);
        if (!has_request) {
            usleep(1000);
        }
    }
    assert(has_request);
    assert(decoded_request.function == 0x03U);

    /* Build and send response */
    uint8_t response_frame[] = {0x11, 0x03, 0x02, 0x12, 0x34};
    assert(mbc_engine_submit_request(&engine, response_frame, sizeof(response_frame)) == MBC_STATUS_OK);

    uint8_t observed_response[sizeof(response_frame) + 2U];
    assert(read_exact(master_fd, observed_response, sizeof(observed_response)));
    assert(memcmp(observed_response, response_frame, sizeof(response_frame)) == 0);
    uint16_t observed_crc = (uint16_t)((uint16_t)observed_response[sizeof(response_frame)] |
                                       ((uint16_t)observed_response[sizeof(response_frame) + 1U] << 8));
    uint16_t expected_crc = mbc_crc16(response_frame, sizeof(response_frame));
    assert(observed_crc == expected_crc);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_rtu_destroy(ctx);
    close(master_fd);
}

static void test_posix_rtu_engine_server_crc_error(void)
{
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master_fd < 0) {
        printf("POSIX RTU engine server CRC error test skipped (PTY not available: %s)\n", strerror(errno));
        return;
    }

    if (grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        printf("POSIX RTU engine server CRC error test skipped (PTY setup failed: %s)\n", strerror(errno));
        close(master_fd);
        return;
    }

    char *slave_path = ptsname(master_fd);
    if (!slave_path) {
        printf("POSIX RTU engine server CRC error test skipped (ptsname failed: %s)\n", strerror(errno));
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
        printf("POSIX RTU engine server CRC error test skipped (device open failed with status=%d, errno=%s)\n",
               status, strerror(errno));
        close(master_fd);
        return;
    }

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_SERVER,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
    };
    assert(mbc_engine_init(&engine, &engine_cfg) == MBC_STATUS_OK);

    uint8_t request_frame[] = {0x11, 0x03, 0x00, 0x00, 0x00, 0x01};
    uint16_t crc = mbc_crc16(request_frame, sizeof(request_frame));
    crc ^= 0xFFFFU;
    uint8_t request_bad[sizeof(request_frame) + 2U];
    memcpy(request_bad, request_frame, sizeof(request_frame));
    request_bad[sizeof(request_frame)] = (uint8_t)(crc & 0xFFU);
    request_bad[sizeof(request_frame) + 1U] = (uint8_t)((crc >> 8) & 0xFFU);
    assert(write(master_fd, request_bad, sizeof(request_bad)) == (ssize_t)sizeof(request_bad));

    bool saw_error = false;
    for (int i = 0; i < 10 && !saw_error; ++i) {
        status = mbc_engine_step(&engine, sizeof(request_bad));
        if (status == MBC_STATUS_DECODING_ERROR) {
            saw_error = true;
            break;
        }
        assert(status == MBC_STATUS_OK);
        usleep(1000);
    }

    assert(saw_error);
    mbc_pdu_t pdu = {0};
    assert(!mbc_engine_take_pdu(&engine, &pdu));

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_rtu_destroy(ctx);
    close(master_fd);
}
#endif

int main(void)
{
#ifdef __unix__
    printf("=== POSIX RTU Tests ===\n\n");
    test_posix_rtu_loop();
    test_posix_rtu_engine_client();
    test_posix_rtu_engine_client_crc_error();
    test_posix_rtu_engine_server();
    test_posix_rtu_engine_server_crc_error();
    printf("\n=== All POSIX RTU tests completed ===\n");
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
