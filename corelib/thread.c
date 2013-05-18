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

#include "phenom/work.h"
#include "phenom/log.h"
#ifdef __sun__
# include <sys/lwp.h>
#endif

/* If the system supports the __thread extension, then __ph_thread_self
 * is magically allocated and made to be a thread local version of the
 * thread struct.  This allows us to avoid the function call and other
 * overheads of pthread_getspecific() in the hot path.
 * However, we still want to guarantee that we clean things up if/when
 * the thread exits, so we also register the thread struct with
 * pthread_setspecific so that we can trigger the TLS key destructor */

#ifdef HAVE___THREAD
__thread ph_thread_t __ph_thread_self;
#else
static ph_memtype_def_t mt_def = {
  "thread", "thread", sizeof(ph_thread_t), PHENOM_MEM_FLAGS_ZERO
};
static ph_memtype_t mt_thread;
#endif
pthread_key_t __ph_thread_key;

static void destroy_thread(void *ptr)
{
  ph_thread_t *thr = ptr;

  ck_epoch_unregister(&__ph_trigger_epoch, thr->trigger_record);

#ifndef HAVE___THREAD
  ph_mem_free(mt_thread, thr);
#endif
}

bool ph_thread_init(void)
{
  pthread_key_create(&__ph_thread_key, destroy_thread);

#ifndef HAVE___THREAD
  mt_thread = ph_memtype_register(&mt_def);
  if (mt_thread == PHENOM_MEMTYPE_INVALID) {
    return false;
  }
#endif

  return true;
}

static void init_thread(ph_thread_t *thr)
{
#ifdef HAVE___THREAD
  memset(thr, 0, sizeof(*thr));
  thr->is_init = true;
#endif

  thr->trigger_record = ck_epoch_recycle(&__ph_trigger_epoch);
  if (!thr->trigger_record) {
    /* trigger records are never freed, but they can be recycled */
    thr->trigger_record = malloc(sizeof(*thr->trigger_record));
    if (!thr) {
      ph_panic("fatal OOM in init_thread");
    }
    ck_epoch_register(&__ph_trigger_epoch, thr->trigger_record);
  }
  ck_fifo_mpmc_init(&thr->triggers,
      ph_mem_alloc(__ph_sched_mt_thread_trigger));

  thr->thr = pthread_self();
#ifdef __sun__
  thr->lwpid = _lwp_self();
#endif
}

struct ph_thread_boot_data {
  ph_thread_t **thr;
  void *(*func)(void*);
  void *arg;
};

static ph_thread_t *ph_thread_init_myself(void)
{
  ph_thread_t *me;

#ifndef HAVE___THREAD
  me = ph_mem_alloc(mt_thread);
  if (!me) {
    ph_panic("fatal OOM in ph_thread_init_myself()");
  }
#else
  me = &__ph_thread_self;
#endif

  pthread_setspecific(__ph_thread_key, me);
  init_thread(me);

  return me;
}

static void *ph_thread_boot(void *arg)
{
  struct ph_thread_boot_data data;
  ph_thread_t *me;

  /* copy in the boot data from the stack of our creator */
  memcpy(&data, arg, sizeof(data));

  me = ph_thread_init_myself();

  /* this publishes that we're ready to run to
   * the thread that spawned us */
  ck_pr_store_ptr(data.thr, me);

  return data.func(data.arg);
}

ph_thread_t *ph_spawn_thread(ph_thread_func func, void *arg)
{
  ph_thread_t *thr = NULL;
  struct ph_thread_boot_data data;
  pthread_t pt;

  data.thr = &thr;
  data.func = func;
  data.arg = arg;

  if (pthread_create(&pt, NULL, ph_thread_boot, &data)) {
    return NULL;
  }

  // semi busy wait for the TLS to be set up
  while (ck_pr_load_ptr(&thr) == 0) {
    ck_pr_stall();
  }

  return thr;
}

ph_thread_t *ph_thread_self_slow(void)
{
  ph_thread_t *thr = pthread_getspecific(__ph_thread_key);

  if (likely(thr != NULL)) {
    return thr;
  }

  return ph_thread_init_myself();
}

void ph_thread_set_name(const char *name)
{
  ph_thread_t *thr = ph_thread_self();

  if (!thr) {
    return;
  }

#ifdef HAVE_PTHREAD_SET_NAME_NP
  /* OpenBSDish */
  pthread_set_name_np(thr->thr, name);
#elif defined(HAVE_PTHREAD_SETNAME_NP) && defined(__linux__)
  pthread_setname_np(thr->thr, name);
#elif defined(HAVE_PTHREAD_SETNAME_NP) && defined(__MACH__)
  pthread_setname_np(name);
#else
  unused_parameter(name);
#endif
}

bool ph_thread_set_affinity(ph_thread_t *me, int affinity)
{
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
# ifdef __linux__
  cpu_set_t set;
# else /* FreeBSD */
  cpuset_t set;
#endif

  CPU_ZERO(&set);
  CPU_SET(affinity, &set);

  return pthread_setaffinity_np(me->thr, sizeof(set), &set) == 0;
#elif defined(__APPLE__)
  thread_affinity_policy_data_t data;

  data.affinity_tag = affinity + 1;
  return thread_policy_set(pthread_mach_thread_np(me->thr),
      THREAD_AFFINITY_POLICY,
      (thread_policy_t)&data, THREAD_AFFINITY_POLICY_COUNT) == 0;
#elif defined(HAVE_CPUSET_SETAFFINITY)
  /* untested bsdish */
  cpuset_t set;

  CPU_ZERO(&set);
  CPU_SET(affinity, &set);
  cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, sizeof(set), set);
#elif defined(HAVE_PROCESSOR_BIND)
  return processor_bind(P_LWPID, me->lwpid, affinity, NULL) == 0;
#else
  unused_parameter(me);
  unused_parameter(affinity);
#endif
  return true;
}


/* vim:ts=2:sw=2:et:
 */

