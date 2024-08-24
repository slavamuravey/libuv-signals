#include "uv.h"
#include "internal.h"
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

static int uv__loop_alive(const uv_loop_t* loop) {
  return loop->active_handles > 0;
}

int uv_run(uv_loop_t* loop, uv_run_mode mode) {
  int r;

  r = uv__loop_alive(loop);

  while (r) {
    uv__io_poll(loop, -1);
    r = uv__loop_alive(loop);
  }

  return 0;
}

static unsigned int next_power_of_two(unsigned int val) {
  val -= 1;
  val |= val >> 1;
  val |= val >> 2;
  val |= val >> 4;
  val |= val >> 8;
  val |= val >> 16;
  val += 1;
  return val;
}

static void maybe_resize(uv_loop_t* loop, unsigned int len) {
  uv__io_t** watchers;
  void* fake_watcher_list;
  void* fake_watcher_count;
  unsigned int nwatchers;
  unsigned int i;

  if (len <= loop->nwatchers) {
    return;
  }

  /* Preserve fake watcher list and count at the end of the watchers */
  if (loop->watchers != NULL) {
    fake_watcher_list = loop->watchers[loop->nwatchers];
    fake_watcher_count = loop->watchers[loop->nwatchers + 1];
  } else {
    fake_watcher_list = NULL;
    fake_watcher_count = NULL;
  }

  nwatchers = next_power_of_two(len + 2) - 2;
  watchers = uv__reallocf(loop->watchers, (nwatchers + 2) * sizeof(loop->watchers[0]));

  if (watchers == NULL) {
    abort();
  }
  for (i = loop->nwatchers; i < nwatchers; i++) {
    watchers[i] = NULL;
  }
  watchers[nwatchers] = fake_watcher_list;
  watchers[nwatchers + 1] = fake_watcher_count;

  loop->watchers = watchers;
  loop->nwatchers = nwatchers;
}

void uv__io_init(uv__io_t* w, uv__io_cb cb, int fd) {
  assert(cb != NULL);
  assert(fd >= -1);
  uv__queue_init(&w->watcher_queue);
  w->cb = cb;
  w->fd = fd;
  w->events = 0;
  w->pevents = 0;
}

void uv__io_start(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  assert(0 == (events & ~(POLLIN | POLLOUT | UV__POLLRDHUP | UV__POLLPRI)));
  assert(0 != events);
  assert(w->fd >= 0);
  assert(w->fd < INT_MAX);

  w->pevents |= events;
  maybe_resize(loop, w->fd + 1);

  if (w->events == w->pevents) {
    return;
  }

  if (uv__queue_empty(&w->watcher_queue)) {
    uv__queue_insert_tail(&loop->watcher_queue, &w->watcher_queue);
  }

  if (loop->watchers[w->fd] == NULL) {
    loop->watchers[w->fd] = w;
    loop->nfds++;
  }
}

int uv__close(int fd) {
  assert(fd > STDERR_FILENO);  /* Catch stdio close bugs. */
  return close(fd);
}
