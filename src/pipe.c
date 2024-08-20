#include "uv.h"
#include "internal.h"
#include <errno.h>
#include <unistd.h>
#include <assert.h>

int uv_pipe(uv_os_fd_t fds[2], int read_flags, int write_flags) {
  uv_os_fd_t temp[2];
  int err;
  int flags = O_CLOEXEC;

  assert(
    ((read_flags & UV_NONBLOCK_PIPE) && (write_flags & UV_NONBLOCK_PIPE)) || 
    (!(read_flags & UV_NONBLOCK_PIPE) && !(write_flags & UV_NONBLOCK_PIPE))
  );

  if ((read_flags & UV_NONBLOCK_PIPE) && (write_flags & UV_NONBLOCK_PIPE)) {
    flags |= UV_FS_O_NONBLOCK;
  }

  if (pipe2(temp, flags)) {
    return errno;
  }

  if (flags & UV_FS_O_NONBLOCK) {
    fds[0] = temp[0];
    fds[1] = temp[1];
    return 0;
  }

  fds[0] = temp[0];
  fds[1] = temp[1];
  return 0;

fail:
  uv__close(temp[0]);
  uv__close(temp[1]);
  return err;
}

int uv__make_pipe(int fds[2], int flags) {
  return uv_pipe(fds,
                 flags & UV_NONBLOCK_PIPE,
                 flags & UV_NONBLOCK_PIPE);
}