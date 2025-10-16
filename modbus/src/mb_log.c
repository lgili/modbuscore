#include <modbus/internal/mb_log.h>

#include <stdbool.h>

#if MB_LOG_ENABLE_STDIO
#include <stdio.h>
#ifndef MB_LOG_STDIO_STREAM
#define MB_LOG_STDIO_STREAM stdout
#endif
#endif

#if MB_LOG_ENABLE_SEGGER_RTT
#include "SEGGER_RTT.h"
#ifndef MB_LOG_RTT_PRINTF
#define MB_LOG_RTT_PRINTF SEGGER_RTT_printf
#endif
#endif

#if MB_LOG_ENABLE_STDIO

#ifndef MB_LOG_STDIO_BUFFER_CAP
#define MB_LOG_STDIO_BUFFER_CAP 256U
#endif

static void mb_log_stdio_sink(mb_log_level_t level, char *msg)
{
    char line[MB_LOG_STDIO_BUFFER_CAP];
    const int written = snprintf(line, sizeof(line), "[%s] %s\n", MB_LOG_LEVEL_NAME(level), msg);
    if (written < 0) {
        return;
    }

    size_t to_write = (size_t)written;
    if (to_write >= sizeof(line)) {
        to_write = sizeof(line) - 1U;
        line[to_write] = '\0';
    }

    const size_t written_bytes = fwrite(line, 1U, to_write, MB_LOG_STDIO_STREAM);
    if (written_bytes != to_write) {
        return;
    }

#if MB_LOG_STDOUT_SYNC_FLUSH
    const int flush_result = fflush(MB_LOG_STDIO_STREAM);
    if (flush_result != 0) {
        return;
    }
#endif
}

mb_log_err_t mb_log_subscribe_stdio(mb_log_level_t threshold)
{
    return MB_LOG_SUBSCRIBE(mb_log_stdio_sink, threshold);
}
#endif

#if MB_LOG_ENABLE_SEGGER_RTT
static void mb_log_rtt_sink(mb_log_level_t level, char *msg)
{
    MB_LOG_RTT_PRINTF(MB_LOG_RTT_CHANNEL, "[%s] %s\n", MB_LOG_LEVEL_NAME(level), msg);
}

mb_log_err_t mb_log_subscribe_rtt(mb_log_level_t threshold)
{
    return MB_LOG_SUBSCRIBE(mb_log_rtt_sink, threshold);
}
#endif

void mb_log_bootstrap_defaults(void)
{
    static bool bootstrapped = false;

    if (!bootstrapped) {
        MB_LOG_INIT();
        bootstrapped = true;
    }

#if MB_LOG_ENABLE_STDIO
    static bool stdio_registered = false;
    if (!stdio_registered) {
        (void)mb_log_subscribe_stdio(MB_LOG_DEFAULT_THRESHOLD);
        stdio_registered = true;
    }
#endif

#if MB_LOG_ENABLE_SEGGER_RTT
    static bool rtt_registered = false;
    if (!rtt_registered) {
        (void)mb_log_subscribe_rtt(MB_LOG_DEFAULT_THRESHOLD);
        rtt_registered = true;
    }
#endif
}
