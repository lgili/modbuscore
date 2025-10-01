/**
 * \file
 *
 * \brief Log: lightweight logging for embedded systems
 *
 * A quick intro by example:
 *
 *     #include "ulog.h"
 *
 *     // To use uLog, you must define a function to process logging messages.
 *     // It can write the messages to a console, to a file, to an in-memory
 *     // buffer: the choice is yours.  And you get to choose the format of
 *     // the message.  This example prints to the console.  One caveat: msg
 *     // is a static string and will be over-written at the next call to ULOG.
 *     // You may print it or copy it, but saving a pointer to it will lead to
 *     // confusion and astonishment.
 *     //
 *     void my_console_logger(ulog_level_t level, const char *msg) {
 *         printf("%s [%s]: %s\n",
 *             get_timestamp(),
 *             ulog_level_name(level),
 *             msg);
 *     }
 *
 *     int main() {
 *         LOG_INIT();
 *
 *         // log to the console messages that are WARNING or more severe.  You
 *         // can re-subscribe at any point to change the severity level.
 *         LOG_SUBSCRIBE(my_console_logger, LOG_WARNING);
 *
 *         // log to a file messages that are DEBUG or more severe
 *         LOG_SUBSCRIBE(my_file_logger, LOG_DEBUG);
 *
 *         int arg = 42;
 *         LOG_INFO("Arg is %d", arg);  // logs to file but not console
 *     }
 */

#ifndef LOG_H_
#define LOG_H_

#ifdef __cplusplus
extern "C" {
    #endif
#include <modbus/conf.h>

typedef enum {
  LOG_TRACE_LEVEL=100,
  LOG_DEBUG_LEVEL,
  LOG_INFO_LEVEL,
  LOG_WARNING_LEVEL,
  LOG_ERROR_LEVEL,
  LOG_CRITICAL_LEVEL,
  LOG_ALWAYS_LEVEL
} log_level_t;


// The following macros enable or disable uLog.  If `LOG_ENABLED` is
// defined at compile time, a macro such as `LOG_INFO(...)` expands
// into `log_message(LOG_INFO_LEVEL, ...)`.  If `LOG_ENABLED` is not
// defined, then the same macro expands into `do {} while(0)` and will
// not generate any code at all.  
//
// There are two ways to enable uLog: you can uncomment the following
// line, or -- if it is commented out -- you can add -DLOG_ENABLED to
// your compiler switches.
//#define LOG_ENABLED

#ifdef LOG_ENABLED
  #define LOG_INIT() log_init()
  #define LOG_SUBSCRIBE(a, b) log_subscribe(a, b)
  #define LOG_UNSUBSCRIBE(a) log_unsubscribe(a)
  #define LOG_LEVEL_NAME(a) log_level_name(a)
  #define LOG(...) log_message(__VA_ARGS__)
  #define LOG_TRACE(...) log_message(LOG_TRACE_LEVEL, __VA_ARGS__)
  #define LOG_DEBUG(...) log_message(LOG_DEBUG_LEVEL, __VA_ARGS__)
  #define LOG_INFO(...) log_message(LOG_INFO_LEVEL, __VA_ARGS__)
  #define LOG_WARNING(...) log_message(LOG_WARNING_LEVEL, __VA_ARGS__)
  #define LOG_ERROR(...) log_message(LOG_ERROR_LEVEL, __VA_ARGS__)
  #define LOG_CRITICAL(...) log_message(LOG_CRITICAL_LEVEL, __VA_ARGS__)
  #define LOG_ALWAYS(...) log_message(LOG_ALWAYS_LEVEL, __VA_ARGS__)
#else
  // uLog vanishes when disabled at compile time...
  #define LOG_INIT() do {} while(0)
  #define LOG_SUBSCRIBE(a, b) do {} while(0)
  #define LOG_UNSUBSCRIBE(a) do {} while(0)
  #define LOG_LEVEL_NAME(a) do {} while(0)
  #define LOG(s, f, ...) do {} while(0)
  #define LOG_TRACE(f, ...) do {} while(0)
  #define LOG_DEBUG(f, ...) do {} while(0)
  #define LOG_INFO(f, ...) do {} while(0)
  #define LOG_WARNING(f, ...) do {} while(0)
  #define LOG_ERROR(f, ...) do {} while(0)
  #define LOG_CRITICAL(f, ...) do {} while(0)
  #define LOG_ALWAYS(f, ...) do {} while(0)
#endif

typedef enum {
  LOG_ERR_NONE = 0,
  LOG_ERR_SUBSCRIBERS_EXCEEDED,
  LOG_ERR_NOT_SUBSCRIBED,
} log_err_t;

// define the maximum number of concurrent subscribers
#ifndef LOG_MAX_SUBSCRIBERS
#define LOG_MAX_SUBSCRIBERS 6
#endif
// maximum length of formatted log message
#ifndef LOG_MAX_MESSAGE_LENGTH
#define LOG_MAX_MESSAGE_LENGTH 120
#endif
/**
 * @brief: prototype for uLog subscribers.
 */
typedef void (*log_function_t)(log_level_t severity, char *msg);

void log_init(void);
log_err_t log_subscribe(log_function_t fn, log_level_t threshold);
log_err_t log_unsubscribe(log_function_t fn);
const char *log_level_name(log_level_t level);
void log_message(log_level_t severity, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* LOG_H_ */