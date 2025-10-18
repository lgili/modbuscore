#include <assert.h>

#include <modbuscore/transport/win32_rtu.h>

int main(void)
{
#ifdef _WIN32
    /* Placeholder: no hardware loopback; ensure API compiles. */
    mbc_win32_rtu_config_t cfg = {
        .port_name = "COM1",
    };
    mbc_transport_iface_t iface;
    mbc_win32_rtu_ctx_t *ctx = NULL;
    mbc_status_t status = mbc_win32_rtu_create(&cfg, &iface, &ctx);
    if (mbc_status_is_ok(status)) {
        mbc_win32_rtu_destroy(ctx);
    }
    return 0;
#else
    mbc_transport_iface_t iface;
    mbc_win32_rtu_ctx_t *ctx = NULL;
    mbc_win32_rtu_config_t cfg = {
        .port_name = "COM1",
    };
    mbc_status_t status = mbc_win32_rtu_create(&cfg, &iface, &ctx);
    assert(status == MBC_STATUS_UNSUPPORTED);
    return 0;
#endif
}
