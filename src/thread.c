#include "uv.h"
#include "internal.h"

#include <pthread.h>
#include <assert.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/resource.h>  /* getrlimit() */
#include <unistd.h>  /* getpagesize() */

#include <limits.h>

/* Musl's PTHREAD_STACK_MIN is 2 KB on all architectures, which is
 * too small to safely receive signals on.
 *
 * Musl's PTHREAD_STACK_MIN + MINSIGSTKSZ == 8192 on arm64 (which has
 * the largest MINSIGSTKSZ of the architectures that musl supports) so
 * let's use that as a lower bound.
 *
 * We use a hardcoded value because PTHREAD_STACK_MIN + MINSIGSTKSZ
 * is between 28 and 133 KB when compiling against glibc, depending
 * on the architecture.
 */
static size_t uv__min_stack_size(void) {
  static const size_t min = 8192;

#ifdef PTHREAD_STACK_MIN  /* Not defined on NetBSD. */
  if (min < (size_t) PTHREAD_STACK_MIN) {
    return PTHREAD_STACK_MIN;
  }
#endif  /* PTHREAD_STACK_MIN */

  return min;
}


/* On Linux, threads created by musl have a much smaller stack than threads
 * created by glibc (80 vs. 2048 or 4096 kB.)  Follow glibc for consistency.
 */
static size_t uv__default_stack_size(void) {
#if defined(__PPC__) || defined(__ppc__) || defined(__powerpc__)
  return 4 << 20;  /* glibc default. */
#else
  return 2 << 20;  /* glibc default. */
#endif
}


/* On MacOS, threads other than the main thread are created with a reduced
 * stack size by default.  Adjust to RLIMIT_STACK aligned to the page size.
 */
size_t uv__thread_stack_size(void) {
  struct rlimit lim;

  /* getrlimit() can fail on some aarch64 systems due to a glibc bug where
   * the system call wrapper invokes the wrong system call. Don't treat
   * that as fatal, just use the default stack size instead.
   */
  if (getrlimit(RLIMIT_STACK, &lim)) {
    return uv__default_stack_size();
  }

  if (lim.rlim_cur == RLIM_INFINITY) {
    return uv__default_stack_size();
  }

  /* pthread_attr_setstacksize() expects page-aligned values. */
  lim.rlim_cur -= lim.rlim_cur % (rlim_t) getpagesize();

  if (lim.rlim_cur >= (rlim_t) uv__min_stack_size()) {
    return lim.rlim_cur;
  }

  return uv__default_stack_size();
}


int uv_thread_create(uv_thread_t *tid, void (*entry)(void *arg), void *arg) {
  uv_thread_options_t params;
  params.flags = UV_THREAD_NO_FLAGS;
  return uv_thread_create_ex(tid, &params, entry, arg);
}

int uv_thread_create_ex(uv_thread_t* tid,
                        const uv_thread_options_t* params,
                        void (*entry)(void *arg),
                        void *arg) {
  int err;
  pthread_attr_t* attr;
  pthread_attr_t attr_storage;
  size_t pagesize;
  size_t stack_size;
  size_t min_stack_size;

  /* Used to squelch a -Wcast-function-type warning. */
  union {
    void (*in)(void*);
    void* (*out)(void*);
  } f;

  stack_size = params->flags & UV_THREAD_HAS_STACK_SIZE ? params->stack_size : 0;

  attr = NULL;
  if (stack_size == 0) {
    stack_size = uv__thread_stack_size();
  } else {
    pagesize = (size_t)getpagesize();
    /* Round up to the nearest page boundary. */
    stack_size = (stack_size + pagesize - 1) &~ (pagesize - 1);
    min_stack_size = uv__min_stack_size();
    if (stack_size < min_stack_size) {
      stack_size = min_stack_size;
    }
  }

  if (stack_size > 0) {
    attr = &attr_storage;

    if (pthread_attr_init(attr)) {
      abort();
    }

    if (pthread_attr_setstacksize(attr, stack_size)) {
      abort();
    }
  }

  f.in = entry;
  err = pthread_create(tid, attr, f.out, arg);

  if (attr != NULL) {
    pthread_attr_destroy(attr);
  }

  return err;
}

int uv_thread_join(uv_thread_t *tid) {
  return pthread_join(*tid, NULL);
}


void uv_once(uv_once_t* guard, void (*callback)(void)) {
  if (pthread_once(guard, callback)) {
    abort();
  }
}
