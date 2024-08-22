#include "uv.h"
#include "internal.h"

int uv__process_init(uv_loop_t* loop) {
  int err;

  err = uv_signal_init(loop, &loop->child_watcher);
  if (err) {
    return err;
  }
  do {
    if (((&loop->child_watcher)->flags & UV_HANDLE_REF) == 0) {
      break;       
    }                      
    (&loop->child_watcher)->flags &= ~UV_HANDLE_REF;                                             
    if (((&loop->child_watcher)->flags & UV_HANDLE_CLOSING) != 0) {
      break;      
    }                   
    if (((&loop->child_watcher)->flags & UV_HANDLE_ACTIVE) != 0) {
      (&loop->child_watcher)->loop->active_handles--;
    }
  } while (0);
  
  loop->child_watcher.flags |= UV_HANDLE_INTERNAL;
  return 0;
}
