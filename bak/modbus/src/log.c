/**
MIT License

Copyright (c) 2019 R. Dunbar Poor <rdpoor@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/**
 * \file ulog.c
 *
 * \brief uLog: lightweight logging for embedded systems
 *
 * See ulog.h for sparse documentation.
 */

#include <modbus/internal/log.h>

#ifdef LOG_ENABLED  // whole file...

#include <stdio.h>
#include <string.h>
#include <stdarg.h>


// =============================================================================
// types and definitions

typedef struct {
  log_function_t fn;
  log_level_t threshold;
} subscriber_t;

// =============================================================================
// local storage

static subscriber_t s_subscribers[LOG_MAX_SUBSCRIBERS];
static char s_message[LOG_MAX_MESSAGE_LENGTH];

// =============================================================================
// user-visible code

void log_init(void) {
  memset(s_subscribers, 0, sizeof(s_subscribers));
}

// search the s_subscribers table to install or update fn
log_err_t log_subscribe(log_function_t fn, log_level_t threshold) {
  int available_slot = -1;
  int i;
  for (i=0; i<LOG_MAX_SUBSCRIBERS; i++) {
    if (s_subscribers[i].fn == fn) {
      // already subscribed: update threshold and return immediately.
      s_subscribers[i].threshold = threshold;
      return LOG_ERR_NONE;

    } else if (s_subscribers[i].fn == NULL) {
      // found a free slot
      available_slot = i;
    }
  }
  // fn is not yet a subscriber.  assign if possible.
  if (available_slot == -1) {
    return LOG_ERR_SUBSCRIBERS_EXCEEDED;
  }
  s_subscribers[available_slot].fn = fn;
  s_subscribers[available_slot].threshold = threshold;
  return LOG_ERR_NONE;
}

// search the s_subscribers table to remove
log_err_t log_unsubscribe(log_function_t fn) {
  int i;
  for (i=0; i<LOG_MAX_SUBSCRIBERS; i++) {
    if (s_subscribers[i].fn == fn) {
      s_subscribers[i].fn = NULL;    // mark as empty
      return LOG_ERR_NONE;
    }
  }
  return LOG_ERR_NOT_SUBSCRIBED;
}

const char *log_level_name(log_level_t severity) {
  switch(severity) {
   case LOG_TRACE_LEVEL: return "\x1b[94mTRACE\x1b[0m";
   case LOG_DEBUG_LEVEL: return "\x1b[36mDEBUG\x1b[0m";
   case LOG_INFO_LEVEL: return "\x1b[32mINFO\x1b[0m";
   case LOG_WARNING_LEVEL: return "\x1b[33mWARNING\x1b[0m";
   case LOG_ERROR_LEVEL: return "\x1b[31mERROR\x1b[0m";
   case LOG_CRITICAL_LEVEL: return "\x1b[35mCRITICAL\x1b[0m";
   case LOG_ALWAYS_LEVEL: return "ALWAYS";
   default: return "UNKNOWN";
  }
}

void log_message(log_level_t severity, const char *fmt, ...) {
  va_list ap;
  int i;
  va_start(ap, fmt);
  vsnprintf(s_message, LOG_MAX_MESSAGE_LENGTH, fmt, ap);
  va_end(ap);

  for (i=0; i<LOG_MAX_SUBSCRIBERS; i++) {
    if (s_subscribers[i].fn != NULL) {
      if (severity >= s_subscribers[i].threshold) {
        s_subscribers[i].fn(severity, s_message);
      }
    }
  }
}

// =============================================================================
// private code

#endif  // #ifdef LOG_ENABLED
