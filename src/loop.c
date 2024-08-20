#include "uv.h"
#include "internal.h"

int uv_loop_init(uv_loop_t* loop) {
  int err;

  loop->active_handles = 0;
  loop->nfds = 0;
  loop->watchers = NULL;
  loop->nwatchers = 0;
  uv__queue_init(&loop->watcher_queue);

  loop->signal_pipefd[0] = -1;
  loop->signal_pipefd[1] = -1;
  loop->backend_fd = -1;

  err = uv__platform_loop_init(loop);
  if (err) {
    goto fail_platform_init;
  }

  uv__signal_global_once_init();

  return 0;

fail_platform_init:
  uv__free(loop->watchers);
  loop->nwatchers = 0;
  return err;
}
