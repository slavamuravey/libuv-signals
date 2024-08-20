#include "uv.h"
#include "internal.h"

#include <stddef.h>
#include <errno.h>
#include <sys/epoll.h>
#include <assert.h>
#include <string.h>

void uv__io_poll(uv_loop_t* loop, int timeout) {
  struct epoll_event events[1024];
  struct epoll_event* pe;
  struct epoll_event e;
  struct uv__queue* q;
  uv__io_t* w;
  int epollfd;
  int nfds;
  int fd;
  int op;
  int i;

  assert(timeout >= -1);

  epollfd = loop->backend_fd;

  memset(&e, 0, sizeof(e));

  while (!uv__queue_empty(&loop->watcher_queue)) {
    q = uv__queue_head(&loop->watcher_queue);
    w = uv__queue_data(q, uv__io_t, watcher_queue);
    uv__queue_remove(q);
    uv__queue_init(q);

    op = EPOLL_CTL_MOD;
    if (w->events == 0) {
      op = EPOLL_CTL_ADD;
    }

    w->events = w->pevents;
    e.events = w->pevents;
    e.data.fd = w->fd;
    fd = w->fd;

    if (!epoll_ctl(epollfd, op, fd, &e)) {
      continue;
    }

    assert(op == EPOLL_CTL_ADD);
    assert(errno == EEXIST);

    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &e)) {
      abort();
    }
  }

  for (;;) {
    nfds = epoll_wait(epollfd, events, ARRAY_SIZE(events), timeout);

    if (nfds == -1) {
      assert(errno == EINTR);
    } else if (nfds == 0) {
      assert(timeout != -1);
    }

    if (nfds == -1) {
      continue;
    }

    if (nfds == 0) {
      return;
    }

    for (i = 0; i < nfds; i++) {
      pe = events + i;
      fd = pe->data.fd;

      if (fd == -1) {
        continue;
      }

      assert(fd >= 0);
      assert((unsigned) fd < loop->nwatchers);

      w = loop->watchers[fd];

      assert(w == &loop->signal_io_watcher);
    }

    loop->signal_io_watcher.cb(loop, &loop->signal_io_watcher, POLLIN);

    break;
  }
}

int uv__platform_loop_init(uv_loop_t* loop) {
  loop->backend_fd = epoll_create(1);

  if (loop->backend_fd == -1) {
    return -1;
  }

  return 0;
}
