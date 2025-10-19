#include <modbuscore/transport/rtu_uart.h>
#include <modbuscore/transport/win32_rtu.h>

#ifdef _WIN32

#include <stdio.h> /* For _snprintf */
#include <windows.h>

typedef struct mbc_win32_rtu_ctx {
    HANDLE handle;
    mbc_rtu_uart_ctx_t* rtu;
} mbc_win32_rtu_ctx_internal_t;

static uint64_t qpc_now_us(void)
{
    LARGE_INTEGER freq, counter;
    if (!QueryPerformanceFrequency(&freq) || !QueryPerformanceCounter(&counter)) {
        return 0ULL;
    }
    return (uint64_t)(counter.QuadPart * 1000000ULL / freq.QuadPart);
}

static size_t win32_backend_write(void* ctx, const uint8_t* data, size_t length)
{
    mbc_win32_rtu_ctx_internal_t* win32 = ctx;
    DWORD written = 0;
    if (!WriteFile(win32->handle, data, (DWORD)length, &written, NULL)) {
        return 0U;
    }
    return (size_t)written;
}

static size_t win32_backend_read(void* ctx, uint8_t* data, size_t capacity)
{
    mbc_win32_rtu_ctx_internal_t* win32 = ctx;
    DWORD read = 0;
    if (!ReadFile(win32->handle, data, (DWORD)capacity, &read, NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING || err == ERROR_NO_MORE_ITEMS) {
            return 0U;
        }
        return 0U;
    }
    return (size_t)read;
}

static void win32_backend_flush(void* ctx)
{
    mbc_win32_rtu_ctx_internal_t* win32 = ctx;
    FlushFileBuffers(win32->handle);
}

static uint64_t win32_backend_now(void* ctx)
{
    (void)ctx;
    return qpc_now_us();
}

static void win32_backend_delay(void* ctx, uint32_t micros)
{
    (void)ctx;
    if (micros >= 1000U) {
        Sleep(micros / 1000U);
    } else {
        Sleep(0);
    }
}

static mbc_status_t configure_port(HANDLE handle, const mbc_win32_rtu_config_t* config)
{
    DCB dcb;
    SecureZeroMemory(&dcb, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle, &dcb)) {
        return MBC_STATUS_IO_ERROR;
    }

    dcb.BaudRate = config->baud_rate ? config->baud_rate : 9600;
    dcb.ByteSize = config->data_bits ? config->data_bits : 8;

    switch (config->parity ? config->parity : 'N') {
    case 'E':
    case 'e':
        dcb.Parity = EVENPARITY;
        break;
    case 'O':
    case 'o':
        dcb.Parity = ODDPARITY;
        break;
    default:
        dcb.Parity = NOPARITY;
        break;
    }

    dcb.StopBits = ((config->stop_bits ? config->stop_bits : 1) == 2) ? TWOSTOPBITS : ONESTOPBIT;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fNull = FALSE;

    if (!SetCommState(handle, &dcb)) {
        return MBC_STATUS_IO_ERROR;
    }

    COMMTIMEOUTS timeouts = {
        .ReadIntervalTimeout = 1,
        .ReadTotalTimeoutMultiplier = 0,
        .ReadTotalTimeoutConstant = 0,
        .WriteTotalTimeoutMultiplier = 0,
        .WriteTotalTimeoutConstant = 0,
    };
    if (!SetCommTimeouts(handle, &timeouts)) {
        return MBC_STATUS_IO_ERROR;
    }

    return MBC_STATUS_OK;
}

mbc_status_t mbc_win32_rtu_create(const mbc_win32_rtu_config_t* config,
                                  mbc_transport_iface_t* out_iface, mbc_win32_rtu_ctx_t** out_ctx)
{
    if (!config || !config->port_name || !out_iface || !out_ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    char device[32];
    _snprintf(device, sizeof(device), "\\\\.\\%s", config->port_name);

    HANDLE handle = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return MBC_STATUS_IO_ERROR;
    }

    mbc_status_t status = configure_port(handle, config);
    if (!mbc_status_is_ok(status)) {
        CloseHandle(handle);
        return status;
    }

    mbc_win32_rtu_ctx_internal_t* ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        CloseHandle(handle);
        return MBC_STATUS_NO_RESOURCES;
    }
    ctx->handle = handle;

    mbc_rtu_uart_config_t uart_cfg = {
        .backend =
            {
                .ctx = ctx,
                .write = win32_backend_write,
                .read = win32_backend_read,
                .flush = win32_backend_flush,
                .now_us = win32_backend_now,
                .delay_us = win32_backend_delay,
            },
        .baud_rate = config->baud_rate,
        .data_bits = config->data_bits,
        .parity_bits = (config->parity == 'E' || config->parity == 'O') ? 1U : 0U,
        .stop_bits = config->stop_bits,
        .guard_time_us = config->guard_time_us,
        .rx_buffer_capacity = config->rx_buffer_capacity,
    };

    mbc_transport_iface_t rtu_iface;
    status = mbc_rtu_uart_create(&uart_cfg, &rtu_iface, &ctx->rtu);
    if (!mbc_status_is_ok(status)) {
        CloseHandle(handle);
        free(ctx);
        return status;
    }

    *out_iface = rtu_iface;
    *out_ctx = ctx;
    return MBC_STATUS_OK;
}

void mbc_win32_rtu_destroy(mbc_win32_rtu_ctx_t* ctx)
{
    mbc_win32_rtu_ctx_internal_t* win32 = (mbc_win32_rtu_ctx_internal_t*)ctx;
    if (!win32) {
        return;
    }
    mbc_rtu_uart_destroy(win32->rtu);
    if (win32->handle && win32->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(win32->handle);
    }
    free(win32);
}

void mbc_win32_rtu_reset(mbc_win32_rtu_ctx_t* ctx)
{
    mbc_win32_rtu_ctx_internal_t* win32 = (mbc_win32_rtu_ctx_internal_t*)ctx;
    if (!win32) {
        return;
    }
    mbc_rtu_uart_reset(win32->rtu);
}

#else

mbc_status_t mbc_win32_rtu_create(const mbc_win32_rtu_config_t* config,
                                  mbc_transport_iface_t* out_iface, mbc_win32_rtu_ctx_t** out_ctx)
{
    (void)config;
    (void)out_iface;
    (void)out_ctx;
    return MBC_STATUS_UNSUPPORTED;
}

void mbc_win32_rtu_destroy(mbc_win32_rtu_ctx_t* ctx) { (void)ctx; }

void mbc_win32_rtu_reset(mbc_win32_rtu_ctx_t* ctx) { (void)ctx; }

#endif
