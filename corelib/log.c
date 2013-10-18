/*
 * Copyright 2012-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "phenom/defs.h"
#include "phenom/log.h"
#include "phenom/thread.h"
#include "phenom/sysutil.h"
#include "phenom/printf.h"
#include "phenom/hook.h"
#ifdef __linux__
#include <sys/syscall.h>
#endif
#ifdef __sun__
# include <sys/lwp.h>
#endif

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t log_level = PH_LOG_ERR;
static const char *log_labels[] = {
  "panic",
  "alert",
  "crit",
  "err",
  "warn",
  "notice",
  "info",
  "debug"
};

static bool disable_stderr = false;

uint8_t ph_log_level_set(uint8_t level)
{
  uint8_t old = log_level;

  log_level = level;

  return old;
}

uint8_t ph_log_level_get(void)
{
  return log_level;
}

static void get_tname(ph_thread_t *me, char *buf, uint32_t size)
{
  uint64_t tid;

  if (me) {
    ph_snprintf(buf, size, "%s/%d", me->name, me->tid);
    return;
  }

#if defined(__linux__)
  tid = syscall(SYS_gettid);
#elif defined(__MACH__)
  pthread_threadid_np(pthread_self(), &tid);
#elif defined(__sun__)
  tid = _lwp_self();
#else
  tid = (uint64_t)(intptr_t)self;
#endif

#if defined(__linux__) || defined(__MACH__)
  if (pthread_getname_np(pthread_self(), buf, size) == 0) {
    int len = strlen(buf);
    if (len > 0) {
      ph_snprintf(buf + len, size - len, "/%" PRIu64, tid);
      return;
    }
  }
#endif

  ph_snprintf(buf, size, "lwp/%" PRIu64, tid);
}

void ph_logv(uint8_t level, const char *fmt, va_list ap)
{
  struct timeval now = ph_time_now();
  ph_thread_t *me;
  int len;
  va_list copy;
  char *buf;
  PH_STRING_DECLARE_STACK(mystr, 1024);
  void *args[] = { &level, &mystr };
  static ph_hook_point_t *hook = NULL;
  char tname[32];

  if (level > log_level) {
    return;
  }

  len = strlen(fmt);
  if (len == 0) {
    return;
  }

  me = ph_thread_self();
  get_tname(me, tname, sizeof(tname));
  va_copy(copy, ap);
  ph_string_printf(&mystr,
      "%" PRIi64 ".%03d %s: %s `Pv%s%p%s",
      (int64_t)now.tv_sec, (int)(now.tv_usec / 1000),
      log_labels[level], tname,
      fmt, ph_vaptr(copy),
      fmt[len-1] == '\n' ? "" : "\n");
  va_end(copy);

  if (ph_unlikely(hook == NULL)) {
    hook = ph_hook_point_get_cstr(PH_LOG_HOOK_NAME, true);
  }
  ph_hook_invoke_inner(hook, sizeof(args)/sizeof(args[0]), args);

  if (disable_stderr) {
    return;
  }

  pthread_mutex_lock(&log_lock);
  buf = mystr.buf;
  len = mystr.len;
  while (len) {
    int x = write(STDERR_FILENO, buf, len);
    if (x <= 0) {
      break;
    }
    buf += x;
    len -= x;
  }
  pthread_mutex_unlock(&log_lock);
}

void ph_log_disable_stderr(void)
{
  disable_stderr = true;
}

void ph_log(uint8_t level, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  ph_logv(level, fmt, ap);
  va_end(ap);
}

#if defined(HAVE_BACKTRACE) && defined(HAVE_BACKTRACE_SYMBOLS)
# include <execinfo.h>
#endif

void ph_log_stacktrace(uint8_t level)
{
#if defined(HAVE_BACKTRACE) && defined(HAVE_BACKTRACE_SYMBOLS)
  void *array[24];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace(array, sizeof(array)/sizeof(array[0]));
  strings = backtrace_symbols(array, size);

  for (i = 0; i < size; i++) {
    ph_log(level, "%s", strings[i]);
  }

  free(strings);
#endif
}

void ph_panic(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  ph_logv(PH_LOG_PANIC, fmt, ap);
  va_end(ap);

  ph_log(PH_LOG_PANIC, "Fatal error detected at:");
  ph_log_stacktrace(PH_LOG_PANIC);
  abort();
}

/* vim:ts=2:sw=2:et:
 */

