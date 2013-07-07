/*
 * Copyright 2012 Facebook, Inc.
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

#ifndef PHENOM_DEFS_H
#define PHENOM_DEFS_H

/**
 * # Base Environment
 *
 * Including `phenom/defs.h` sets the base environment for using
 * phenom.  This header should be included first (most phenom headers
 * will pull this in explicitly) so that the compilation environment
 * exposes the more modern unix compilation features of your system.
 */

#define PHENOM_TARGET_CPU_X86_64 1
#define PHENOM_TARGET_CPU_X86    2

#ifndef _REENTRANT
# define _REENTRANT
#endif
#define __EXTENSIONS__ 1
#define _XOPEN_SOURCE 600
#define _BSD_SOURCE
#define _POSIX_C_SOURCE 200809
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE

#include <phenom/config.h>

/* This is working around eccentricities in the CK build system */
#if PHENOM_TARGET_CPU == PHENOM_TARGET_CPU_X86_64
# ifndef __x86_64__
#  define __x86_64__ 1
# endif
#elif PHENOM_TARGET_CPU == PHENOM_TARGET_CPU_X86
# ifndef __x86__
#  define __x86__ 1
# endif
#else
# error unsupported target platform
#endif

#ifdef __FreeBSD__
/* need this to get u_short so we can include sys/event.h.
 * This has to happen before we include sys/types.h */
# include <sys/cdefs.h>
# define __BSD_VISIBLE 1
#endif

#include <sys/types.h>

#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_SYS_TIMERFD_H
# include <sys/timerfd.h>
#endif
#ifdef HAVE_SYS_EVENTFD_H
# include <sys/eventfd.h>
#endif
#ifdef HAVE_SYS_EPOLL_H
# include <sys/epoll.h>
#endif
#ifdef HAVE_SYS_EVENT_H
# include <sys/event.h>
#endif

#ifdef HAVE_PTHREAD_H
# include <pthread.h>
#endif
#ifdef HAVE_PTHREAD_NP_H
# include <pthread_np.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>

#ifdef HAVE_SYS_CPUSET_H
# include <sys/cpuset.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
#ifdef HAVE_SYS_PROCESSOR_H
# include <sys/processor.h>
#endif
#ifdef HAVE_SYS_PROCSET_H
# include <sys/procset.h>
#endif

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#ifdef HAVE_PORT_CREATE
# include <port.h>
# include <sys/poll.h>
#endif

#include <sys/uio.h>

#if defined(__APPLE__)
/* for thread affinity */
#include <mach/thread_policy.h>
#include <mach/mach_init.h>
#include <mach/thread_act.h>
/* clock */
#include <mach/mach_time.h>
#endif

/**
 * ## Pedantic compilation
 *
 * To stave off undefined or unexpected conditions, Phenom is
 * compiled in an extremely unforgiving mode that causes warnings
 * to be treated as errors.
 *
 * There are a couple of useful source annotations that you can
 * use to avoid triggering some classes of error.
 *
 * ### unused_parameter
 *
 * ```
 * void myfunc(int not_used)
 * {
 *    unused_parameter(not_used);
 * }
 * ```
 *
 * ### Result not ignored
 *
 * Some compilation environments are very strict and will raise
 * warnings if you ignore return values of certain functions.
 * In some cases you really do want to ignore these results.
 * Here's how to tell the compiler to leave you alone:
 *
 * ```
 * void myfunc(void)
 * {
 *    ignore_result(poll(&pfd, 1, 100));
 * }
 * ```
 */

#if defined(PHENOM_IMPL)
// Use this to eliminate 'unused parameter' warnings
# define unused_parameter(x)  (void)x

// Use this to cleanly indicate that we intend to ignore
// the result of functions marked with warn_unused_result
# if defined(__USE_FORTIFY_LEVEL) && __USE_FORTIFY_LEVEL > 0
#  define ignore_result(x) \
  do { __typeof__(x) _res = x; (void)_res; } while(0)
# else
#  define ignore_result(x) x
# endif

# ifdef __GNUC__
#  define likely(x)    __builtin_expect(!!(x), 1)
#  define unlikely(x)  __builtin_expect(!!(x), 0)
# else
#  define likely(x)    (x)
#  define unlikely(x)  (x)
# endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Generic result type
 *
 * If you wish to avoid TLS overheads with errno if/when a function fails,
 * you may choose to implement your return values in terms of the ph_result_t
 * type.  The value `PH_OK` is defined to 0 and means success.  All other
 * values are interpreted as something not quite working out for a variety
 * of reasons.
 *
 * * `PH_OK` - success!
 * * `PH_NOMEM` - insufficient memory
 * * `PH_BUSY` - too busy to complete now (try later)
 * * `PH_ERR` - generic failure (sorry)
 * * `PH_NOENT` - requested item has no entry, could not be found
 * * `PH_EXISTS` - requested item is already present
 */
typedef uint32_t ph_result_t;
#define PH_OK      0
#define PH_NOMEM   1
#define PH_BUSY    2
#define PH_ERR     3 /* programmer too lazy */
#define PH_NOENT   4
#define PH_EXISTS  5


#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:et:
 */

