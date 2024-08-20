#ifndef UV_UNIX_INTERNAL_H_
#define UV_UNIX_INTERNAL_H_

#include "uv.h"
#include "uv-common.h"

#include <inttypes.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>

#include <poll.h>

#if defined(__clang__) ||                                                     \
    defined(__GNUC__) ||                                                      \
    defined(__INTEL_COMPILER)
# define UV_UNUSED(declaration)     __attribute__((unused)) declaration
#else
# define UV_UNUSED(declaration)     declaration
#endif

#ifdef POLLRDHUP
# define UV__POLLRDHUP POLLRDHUP
#else
# define UV__POLLRDHUP 0x2000
#endif

#ifdef POLLPRI
# define UV__POLLPRI POLLPRI
#else
# define UV__POLLPRI 0
#endif

void uv__io_poll(uv_loop_t* loop, int timeout);

int uv__platform_loop_init(uv_loop_t* loop);

int uv__close(int fd);

int uv__make_pipe(int fds[2], int flags);

void uv__signal_global_once_init(void);

#endif
