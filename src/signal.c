#include "uv.h"
#include "internal.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef SA_RESTART
# define SA_RESTART 0
#endif

#define RB_BLACK 0
#define RB_RED   1

typedef struct {
  uv_signal_t* handle;
  int signum;
} uv__signal_msg_t;


struct uv__signal_tree_s {
  struct uv_signal_s *rbh_root; /* root of the tree */
};


static int uv__signal_unlock(void);
static int uv__signal_start(uv_signal_t* handle, uv_signal_cb signal_cb, int signum, int oneshot);
static void uv__signal_event(uv_loop_t* loop, uv__io_t* w, unsigned int events);
static int uv__signal_compare(uv_signal_t* w1, uv_signal_t* w2);
static void uv__signal_stop(uv_signal_t* handle);
static void uv__signal_unregister_handler(int signum);

static struct uv__signal_tree_s uv__signal_tree = { NULL };
static int uv__signal_lock_pipefd[2] = { -1, -1 };


static void uv__signal_tree_s_RB_INSERT_COLOR(
  struct uv__signal_tree_s* head, 
  struct uv_signal_s* elm
) {
  struct uv_signal_s *parent, *gparent, *tmp;
  
  while ((parent = elm->tree_entry.rbe_parent) != NULL && parent->tree_entry.rbe_color == RB_RED) {
    gparent = parent->tree_entry.rbe_parent;
    if (parent == gparent->tree_entry.rbe_left) {
      tmp = gparent->tree_entry.rbe_right;
      if (tmp && tmp->tree_entry.rbe_color == RB_RED) {
        tmp->tree_entry.rbe_color = RB_BLACK;
        parent->tree_entry.rbe_color = RB_BLACK;
        gparent->tree_entry.rbe_color = RB_RED;
        elm = gparent;
        
        continue;
      }
      if (parent->tree_entry.rbe_right == elm) {
        tmp = parent->tree_entry.rbe_right;
        if ((parent->tree_entry.rbe_right = tmp->tree_entry.rbe_left) != NULL) {
          tmp->tree_entry.rbe_left->tree_entry.rbe_parent = parent;
        }

        if ((tmp->tree_entry.rbe_parent = parent->tree_entry.rbe_parent) != NULL) {
          if (parent == parent->tree_entry.rbe_parent->tree_entry.rbe_left) {
            parent->tree_entry.rbe_parent->tree_entry.rbe_left = tmp;
          } else {
            parent->tree_entry.rbe_parent->tree_entry.rbe_right = tmp;
          }
        } else {
          head->rbh_root = tmp;
        }
        tmp->tree_entry.rbe_left = parent;
        parent->tree_entry.rbe_parent = tmp;
        tmp = parent;
        parent = elm;
        elm = tmp;
      }

      parent->tree_entry.rbe_color = RB_BLACK;
      gparent->tree_entry.rbe_color = RB_RED;
      
      tmp = gparent->tree_entry.rbe_left;
      if ((gparent->tree_entry.rbe_left = tmp->tree_entry.rbe_right) != NULL) {
        tmp->tree_entry.rbe_right->tree_entry.rbe_parent = gparent;
      }
      if ((tmp->tree_entry.rbe_parent = gparent->tree_entry.rbe_parent) != NULL) {
        if (gparent == gparent->tree_entry.rbe_parent->tree_entry.rbe_left) {
          gparent->tree_entry.rbe_parent->tree_entry.rbe_left = tmp;
        } else {
          gparent->tree_entry.rbe_parent->tree_entry.rbe_right = tmp;
        }
      } else {
        head->rbh_root = tmp;
      }
      tmp->tree_entry.rbe_right = gparent;
      gparent->tree_entry.rbe_parent = tmp;
    } else {
      tmp = gparent->tree_entry.rbe_left;
      if (tmp && tmp->tree_entry.rbe_color == RB_RED) {
        tmp->tree_entry.rbe_color = RB_BLACK;
        parent->tree_entry.rbe_color = RB_BLACK;
        gparent->tree_entry.rbe_color = RB_RED;
        elm = gparent;
        
        continue;
      }
      if (parent->tree_entry.rbe_left == elm) {
        tmp = parent->tree_entry.rbe_left;
        if ((parent->tree_entry.rbe_left = tmp->tree_entry.rbe_right) != NULL) {
          tmp->tree_entry.rbe_right->tree_entry.rbe_parent = parent;
        }

        if ((tmp->tree_entry.rbe_parent = parent->tree_entry.rbe_parent) != NULL) {
          if (parent == parent->tree_entry.rbe_parent->tree_entry.rbe_left) {
            parent->tree_entry.rbe_parent->tree_entry.rbe_left = tmp;
          } else {
            parent->tree_entry.rbe_parent->tree_entry.rbe_right = tmp;
          }
        } else {
          head->rbh_root = tmp;
        }
        tmp->tree_entry.rbe_right = parent;
        parent->tree_entry.rbe_parent = tmp;
        tmp = parent;
        parent = elm;
        elm = tmp;
      }
      parent->tree_entry.rbe_color = RB_BLACK;
      gparent->tree_entry.rbe_color = RB_RED;
      tmp = gparent->tree_entry.rbe_right;
      if ((gparent->tree_entry.rbe_right = tmp->tree_entry.rbe_left) != NULL) {
        tmp->tree_entry.rbe_left->tree_entry.rbe_parent = gparent;
      }
      if ((tmp->tree_entry.rbe_parent = gparent->tree_entry.rbe_parent) != NULL) {
        if (gparent == gparent->tree_entry.rbe_parent->tree_entry.rbe_left) {
          gparent->tree_entry.rbe_parent->tree_entry.rbe_left = tmp;
        } else {
          gparent->tree_entry.rbe_parent->tree_entry.rbe_right = tmp;
        }
      } else {
        head->rbh_root = tmp;
      }
      tmp->tree_entry.rbe_left = gparent;
      gparent->tree_entry.rbe_parent = tmp;
    }
  }
  head->rbh_root->tree_entry.rbe_color = RB_BLACK;
}


static void uv__signal_tree_s_RB_REMOVE_COLOR(
  struct uv__signal_tree_s* head, struct uv_signal_s* parent,
  struct uv_signal_s* elm
) {
  struct uv_signal_s* tmp;

  while ((elm == NULL || elm->tree_entry.rbe_color == RB_BLACK) && elm != head->rbh_root) {
    if (parent->tree_entry.rbe_left == elm) {
      tmp = parent->tree_entry.rbe_right;
      if (tmp->tree_entry.rbe_color == RB_RED) {
        tmp->tree_entry.rbe_color = RB_BLACK;
        parent->tree_entry.rbe_color = RB_RED;
        tmp = parent->tree_entry.rbe_right;
        if ((parent->tree_entry.rbe_right = tmp->tree_entry.rbe_left) != NULL) {
          tmp->tree_entry.rbe_left->tree_entry.rbe_parent = parent;
        }
        if ((tmp->tree_entry.rbe_parent = parent->tree_entry.rbe_parent) != NULL) {
          if (parent == parent->tree_entry.rbe_parent->tree_entry.rbe_left) {
            parent->tree_entry.rbe_parent->tree_entry.rbe_left = tmp;
          } else {
            parent->tree_entry.rbe_parent->tree_entry.rbe_right = tmp;
          }
        } else {
          head->rbh_root = tmp;
        }
        tmp->tree_entry.rbe_left = parent;
        parent->tree_entry.rbe_parent = tmp;
        tmp = parent->tree_entry.rbe_right;
      }
      if ((tmp->tree_entry.rbe_left == NULL || tmp->tree_entry.rbe_left->tree_entry.rbe_color == RB_BLACK) &&
          (tmp->tree_entry.rbe_right == NULL || tmp->tree_entry.rbe_right->tree_entry.rbe_color == RB_BLACK)) {
        tmp->tree_entry.rbe_color = RB_RED;
        elm = parent;
        parent = elm->tree_entry.rbe_parent;
      } else {
        if (tmp->tree_entry.rbe_right == NULL || tmp->tree_entry.rbe_right->tree_entry.rbe_color == RB_BLACK) {
          struct uv_signal_s* oleft;
          
          if ((oleft = tmp->tree_entry.rbe_left) != NULL) {
            oleft->tree_entry.rbe_color = RB_BLACK;
          }
          tmp->tree_entry.rbe_color = RB_RED;
          oleft = tmp->tree_entry.rbe_left;
          if ((tmp->tree_entry.rbe_left = oleft->tree_entry.rbe_right) != NULL) {
            oleft->tree_entry.rbe_right->tree_entry.rbe_parent = tmp;
          }
          if ((oleft->tree_entry.rbe_parent = tmp->tree_entry.rbe_parent) != NULL) {
            if (tmp == tmp->tree_entry.rbe_parent->tree_entry.rbe_left) {
              tmp->tree_entry.rbe_parent->tree_entry.rbe_left = oleft;
            } else {
              tmp->tree_entry.rbe_parent->tree_entry.rbe_right = oleft;
            }
          } else {
            head->rbh_root = oleft;
          }
          oleft->tree_entry.rbe_right = tmp;
          tmp->tree_entry.rbe_parent = oleft;
          tmp = parent->tree_entry.rbe_right;
        }
        tmp->tree_entry.rbe_color = parent->tree_entry.rbe_color;
        parent->tree_entry.rbe_color = RB_BLACK;
        if (tmp->tree_entry.rbe_right) {
          (tmp->tree_entry.rbe_right)->tree_entry.rbe_color = RB_BLACK;
        }
        tmp = parent->tree_entry.rbe_right;
        if ((parent->tree_entry.rbe_right = tmp->tree_entry.rbe_left) != NULL) {
          tmp->tree_entry.rbe_left->tree_entry.rbe_parent = parent;
        }
        if ((tmp->tree_entry.rbe_parent = parent->tree_entry.rbe_parent) != NULL) {
          if (parent == parent->tree_entry.rbe_parent->tree_entry.rbe_left) {
            parent->tree_entry.rbe_parent->tree_entry.rbe_left = tmp;
          } else {
            parent->tree_entry.rbe_parent->tree_entry.rbe_right = tmp;
          }
        } else {
          head->rbh_root = tmp;
        }
        tmp->tree_entry.rbe_left = parent;
        parent->tree_entry.rbe_parent = tmp;
        elm = head->rbh_root;
        
        break;
      }
    } else {
      tmp = parent->tree_entry.rbe_left;
      if (tmp->tree_entry.rbe_color == RB_RED) {
        tmp->tree_entry.rbe_color = RB_BLACK;
        parent->tree_entry.rbe_color = RB_RED;
        tmp = parent->tree_entry.rbe_left;
        if ((parent->tree_entry.rbe_left = tmp->tree_entry.rbe_right) != NULL) {
          tmp->tree_entry.rbe_right->tree_entry.rbe_parent = parent;
        }

        if ((tmp->tree_entry.rbe_parent = parent->tree_entry.rbe_parent) != NULL) {
          if (parent == parent->tree_entry.rbe_parent->tree_entry.rbe_left) {
            parent->tree_entry.rbe_parent->tree_entry.rbe_left = tmp;
          } else {
            parent->tree_entry.rbe_parent->tree_entry.rbe_right = tmp;
          }
        } else {
          head->rbh_root = tmp;
        }
        tmp->tree_entry.rbe_right = parent;
        parent->tree_entry.rbe_parent = tmp;
        tmp = parent->tree_entry.rbe_left;
      }
      if ((tmp->tree_entry.rbe_left == NULL || tmp->tree_entry.rbe_left->tree_entry.rbe_color == RB_BLACK) &&
          (tmp->tree_entry.rbe_right == NULL || tmp->tree_entry.rbe_right->tree_entry.rbe_color == RB_BLACK)) {
        tmp->tree_entry.rbe_color = RB_RED;
        elm = parent;
        parent = elm->tree_entry.rbe_parent;
      } else {
        if (tmp->tree_entry.rbe_left == NULL || tmp->tree_entry.rbe_left->tree_entry.rbe_color == RB_BLACK) {
          struct uv_signal_s* oright;
          
          if ((oright = tmp->tree_entry.rbe_right) != NULL) {
            oright->tree_entry.rbe_color = RB_BLACK;
          }
          tmp->tree_entry.rbe_color = RB_RED;
          oright = tmp->tree_entry.rbe_right;
          if ((tmp->tree_entry.rbe_right = oright->tree_entry.rbe_left) != NULL) {
            oright->tree_entry.rbe_left->tree_entry.rbe_parent = tmp;
          }
          if ((oright->tree_entry.rbe_parent = tmp->tree_entry.rbe_parent) != NULL) {
            if (tmp == tmp->tree_entry.rbe_parent->tree_entry.rbe_left) {
              tmp->tree_entry.rbe_parent->tree_entry.rbe_left = oright;
            } else {
              tmp->tree_entry.rbe_parent->tree_entry.rbe_right = oright;
            }
          } else {
            head->rbh_root = oright;
          }
          oright->tree_entry.rbe_left = tmp;
          tmp->tree_entry.rbe_parent = oright;
          tmp = parent->tree_entry.rbe_left;
        }
        tmp->tree_entry.rbe_color = parent->tree_entry.rbe_color;
        parent->tree_entry.rbe_color = RB_BLACK;
        if (tmp->tree_entry.rbe_left) {
          tmp->tree_entry.rbe_left->tree_entry.rbe_color = RB_BLACK;
        }
        tmp = parent->tree_entry.rbe_left;
        if ((parent->tree_entry.rbe_left = tmp->tree_entry.rbe_right) != NULL) {
          tmp->tree_entry.rbe_right->tree_entry.rbe_parent = parent;
        }
        if ((tmp->tree_entry.rbe_parent = parent->tree_entry.rbe_parent) != NULL) {
          if (parent == parent->tree_entry.rbe_parent->tree_entry.rbe_left) {
            parent->tree_entry.rbe_parent->tree_entry.rbe_left = tmp;
          } else {
            parent->tree_entry.rbe_parent->tree_entry.rbe_right = tmp;
          }
        } else {
          head->rbh_root = tmp;
        }
        tmp->tree_entry.rbe_right = parent;
        parent->tree_entry.rbe_parent = tmp;
        elm = head->rbh_root;
        
        break;
      }
    }
  }
  
  if (elm) {
    elm->tree_entry.rbe_color = RB_BLACK;
  }
}


static struct uv_signal_s* uv__signal_tree_s_RB_REMOVE(
  struct uv__signal_tree_s* head, 
  struct uv_signal_s* elm
) {
  struct uv_signal_s *child, *parent, *old = elm;
  int color;
  
  if (elm->tree_entry.rbe_left == NULL) {
    child = elm->tree_entry.rbe_right;
  } else if (elm->tree_entry.rbe_right == NULL) {
    child = elm->tree_entry.rbe_left;
  } else {
    struct uv_signal_s* left;
    
    elm = elm->tree_entry.rbe_right;
    while ((left = elm->tree_entry.rbe_left) != NULL) {
      elm = left;
    }
    child = elm->tree_entry.rbe_right;
    parent = elm->tree_entry.rbe_parent;
    color = elm->tree_entry.rbe_color;
    if (child) {
      child->tree_entry.rbe_parent = parent;
    }
    if (parent) {
      if (parent->tree_entry.rbe_left == elm) {
        parent->tree_entry.rbe_left = child;
      } else {
        parent->tree_entry.rbe_right = child;
      }
    } else {
      head->rbh_root = child;
    }
    if (elm->tree_entry.rbe_parent == old) {
      parent = elm;
    }
    elm->tree_entry = old->tree_entry;
    if (old->tree_entry.rbe_parent) {
      if (old->tree_entry.rbe_parent->tree_entry.rbe_left == old) {
        old->tree_entry.rbe_parent->tree_entry.rbe_left = elm;
      } else {
        old->tree_entry.rbe_parent->tree_entry.rbe_right = elm;
      }
    } else {
      head->rbh_root = elm;
    }
    old->tree_entry.rbe_left->tree_entry.rbe_parent = elm;
    if (old->tree_entry.rbe_right) {
      old->tree_entry.rbe_right->tree_entry.rbe_parent = elm;
    }
    if (parent) {
      left = parent;
      while ((left = left->tree_entry.rbe_parent) != NULL) {}
    }
    goto color;
  }
  parent = elm->tree_entry.rbe_parent;
  color = elm->tree_entry.rbe_color;
  if (child) {
    child->tree_entry.rbe_parent = parent;
  }
  if (parent) {
    if (parent->tree_entry.rbe_left == elm) {
      parent->tree_entry.rbe_left = child;
    } else {
      parent->tree_entry.rbe_right = child;
    }
  } else {
    head->rbh_root = child;
  }
color:
  if (color == 0) {
    uv__signal_tree_s_RB_REMOVE_COLOR(head, parent, child);
  }

  return (old);
}


static struct uv_signal_s* uv__signal_tree_s_RB_INSERT(
  struct uv__signal_tree_s* head, 
  struct uv_signal_s* elm
) {
  struct uv_signal_s* tmp;
  struct uv_signal_s* parent = NULL;
  
  int comp = 0;
  tmp = head->rbh_root;
  
  while (tmp) {
    parent = tmp;
    comp = uv__signal_compare(elm, parent);
    if (comp < 0) {
      tmp = tmp->tree_entry.rbe_left;
    } else if (comp > 0) {
      tmp = tmp->tree_entry.rbe_right;
    } else {
      return tmp;
    }
  }
  elm->tree_entry.rbe_parent = parent;
  elm->tree_entry.rbe_left = elm->tree_entry.rbe_right = NULL;
  elm->tree_entry.rbe_color = RB_RED;
  if (parent != NULL) {
    if (comp < 0) {
      parent->tree_entry.rbe_left = elm;
    } else {
      parent->tree_entry.rbe_right = elm;
    }
  } else {
    head->rbh_root = elm;
  }
  
  uv__signal_tree_s_RB_INSERT_COLOR(head, elm);
  
  return NULL;
}


static struct uv_signal_s* uv__signal_tree_s_RB_NFIND(
  struct uv__signal_tree_s* head, 
  struct uv_signal_s* elm
) {
  struct uv_signal_s* tmp = head->rbh_root;
  struct uv_signal_s* res = NULL;
  int comp;
  
  while (tmp) {
    comp = uv__signal_compare(elm, tmp);
    if (comp < 0) {
      res = tmp;
      tmp = tmp->tree_entry.rbe_left;
    } else if (comp > 0) {
      tmp = tmp->tree_entry.rbe_right;
    } else {
      return tmp;
    }
  }
  
  return res;
}


static struct uv_signal_s* uv__signal_tree_s_RB_NEXT(
  struct uv_signal_s* elm
) {
  if (elm->tree_entry.rbe_right) {
    elm = elm->tree_entry.rbe_right;
    while (elm->tree_entry.rbe_left) {
      elm = elm->tree_entry.rbe_left;
    }
  } else {
    if (elm->tree_entry.rbe_parent && (elm == elm->tree_entry.rbe_parent->tree_entry.rbe_left)) {
      elm = elm->tree_entry.rbe_parent;
    } else {
      while (elm->tree_entry.rbe_parent && (elm == elm->tree_entry.rbe_parent->tree_entry.rbe_right)) {
        elm = elm->tree_entry.rbe_parent;
      }
      elm = elm->tree_entry.rbe_parent;
    }
  }
  
  return elm;
}


static void uv__signal_global_reinit(void);

static void uv__signal_global_init(void) {
  uv__signal_global_reinit();
}


void uv__signal_cleanup(void) {
  if (uv__signal_lock_pipefd[0] != -1) {
    uv__close(uv__signal_lock_pipefd[0]);
    uv__signal_lock_pipefd[0] = -1;
  }

  if (uv__signal_lock_pipefd[1] != -1) {
    uv__close(uv__signal_lock_pipefd[1]);
    uv__signal_lock_pipefd[1] = -1;
  }
}


static void uv__signal_global_reinit(void) {
  uv__signal_cleanup();

  if (uv__make_pipe(uv__signal_lock_pipefd, 0)) {
    abort();
  }

  if (uv__signal_unlock()) {
    abort();
  }
}


void uv__signal_global_once_init(void) {
  uv__signal_global_init();
}


static int uv__signal_lock(void) {
  int r;
  char data;

  do {
    r = read(uv__signal_lock_pipefd[0], &data, sizeof data);
  } while (r < 0 && errno == EINTR);

  return (r < 0) ? -1 : 0;
}


static int uv__signal_unlock(void) {
  int r;
  char data = 42;

  do {
    r = write(uv__signal_lock_pipefd[1], &data, sizeof data);
  } while (r < 0 && errno == EINTR);

  return (r < 0) ? -1 : 0;
}


static void uv__signal_block_and_lock(sigset_t* saved_sigmask) {
  sigset_t new_mask;

  if (sigfillset(&new_mask)) {
    abort();
  }

  /* to shut up valgrind */
  sigemptyset(saved_sigmask);
  if (pthread_sigmask(SIG_SETMASK, &new_mask, saved_sigmask)) {
    abort();
  }

  if (uv__signal_lock()) {
    abort();
  }
}


static void uv__signal_unlock_and_unblock(sigset_t* saved_sigmask) {
  if (uv__signal_unlock()) {
    abort();
  }

  if (pthread_sigmask(SIG_SETMASK, saved_sigmask, NULL)) {
    abort();
  }
}


static uv_signal_t* uv__signal_first_handle(int signum) {
  /* This function must be called with the signal lock held. */
  uv_signal_t lookup;
  uv_signal_t* handle;

  lookup.signum = signum;
  lookup.flags = 0;
  lookup.loop = NULL;

  handle = uv__signal_tree_s_RB_NFIND(&uv__signal_tree, &lookup);

  if (handle != NULL && handle->signum == signum) {
    return handle;
  }

  return NULL;
}


static void uv__signal_handler(int signum) {
  uv__signal_msg_t msg;
  uv_signal_t* handle;
  int saved_errno;

  saved_errno = errno;
  memset(&msg, 0, sizeof msg);

  if (uv__signal_lock()) {
    errno = saved_errno;
    return;
  }

  for (handle = uv__signal_first_handle(signum);
       handle != NULL && handle->signum == signum;
       handle = uv__signal_tree_s_RB_NEXT(handle)) {
    int r;

    msg.signum = signum;
    msg.handle = handle;

    /* write() should be atomic for small data chunks, so the entire message
     * should be written at once. In theory the pipe could become full, in
     * which case the user is out of luck.
     */
    do {
      r = write(handle->loop->signal_pipefd[1], &msg, sizeof msg);
    } while (r == -1 && errno == EINTR);

    assert(r == sizeof msg || (r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)));

    if (r != -1) {
      handle->caught_signals++;
    }
  }

  uv__signal_unlock();
  errno = saved_errno;
}


static int uv__signal_register_handler(int signum, int oneshot) {
  /* When this function is called, the signal lock must be held. */
  struct sigaction sa;

  /* XXX use a separate signal stack? */
  memset(&sa, 0, sizeof(sa));
  if (sigfillset(&sa.sa_mask)) {
    abort();
  }
  sa.sa_handler = uv__signal_handler;
  sa.sa_flags = SA_RESTART;
  if (oneshot) {
    sa.sa_flags |= SA_RESETHAND;
  }

  /* XXX save old action so we can restore it later on? */
  if (sigaction(signum, &sa, NULL)) {
    return errno;
  }

  return 0;
}


static void uv__signal_unregister_handler(int signum) {
  /* When this function is called, the signal lock must be held. */
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;

  /* sigaction can only fail with EINVAL or EFAULT; an attempt to deregister a
   * signal implies that it was successfully registered earlier, so EINVAL
   * should never happen.
   */
  if (sigaction(signum, &sa, NULL)) {
    abort();
  }
}


static int uv__signal_loop_once_init(uv_loop_t* loop) {
  int err;

  /* Return if already initialized. */
  if (loop->signal_pipefd[0] != -1) {
    return 0;
  }

  err = uv__make_pipe(loop->signal_pipefd, UV_NONBLOCK_PIPE);
  if (err) {
    return err;
  }

  uv__io_init(&loop->signal_io_watcher, uv__signal_event, loop->signal_pipefd[0]);
  uv__io_start(loop, &loop->signal_io_watcher, POLLIN);

  return 0;
}


int uv_signal_init(uv_loop_t* loop, uv_signal_t* handle) {
  int err;

  err = uv__signal_loop_once_init(loop);
  if (err) {
    return err;
  }

  handle->loop = loop;
  handle->flags = UV_HANDLE_REF;  /* Ref the loop when active. */
  handle->signum = 0;
  handle->caught_signals = 0;
  handle->dispatched_signals = 0;

  return 0;
}


void uv__signal_close(uv_signal_t* handle) {
  uv__signal_stop(handle);
}


int uv_signal_start(uv_signal_t* handle, uv_signal_cb signal_cb, int signum) {
  return uv__signal_start(handle, signal_cb, signum, 0);
}


int uv_signal_start_oneshot(uv_signal_t* handle, uv_signal_cb signal_cb, int signum) {
  return uv__signal_start(handle, signal_cb, signum, 1);
}


static int uv__signal_start(uv_signal_t* handle, uv_signal_cb signal_cb, int signum, int oneshot) {
  sigset_t saved_sigmask;
  int err;
  uv_signal_t* first_handle;

  assert((handle->flags & (UV_HANDLE_CLOSING | UV_HANDLE_CLOSED)) == 0);

  /* If the user supplies signum == 0, then return an error already. If the
   * signum is otherwise invalid then uv__signal_register will find out
   * eventually.
   */
  if (signum == 0) {
    return -1;
  }

  /* Short circuit: if the signal watcher is already watching {signum} don't
   * go through the process of deregistering and registering the handler.
   * Additionally, this avoids pending signals getting lost in the small
   * time frame that handle->signum == 0.
   */
  if (signum == handle->signum) {
    handle->signal_cb = signal_cb;
    return 0;
  }

  /* If the signal handler was already active, stop it first. */
  if (handle->signum != 0) {
    uv__signal_stop(handle);
  }

  uv__signal_block_and_lock(&saved_sigmask);

  /* If at this point there are no active signal watchers for this signum (in
   * any of the loops), it's time to try and register a handler for it here.
   * Also in case there's only one-shot handlers and a regular handler comes in.
   */
  first_handle = uv__signal_first_handle(signum);
  if (first_handle == NULL || (!oneshot && (first_handle->flags & UV_SIGNAL_ONE_SHOT))) {
    err = uv__signal_register_handler(signum, oneshot);
    if (err) {
      /* Registering the signal handler failed. Must be an invalid signal. */
      uv__signal_unlock_and_unblock(&saved_sigmask);
      return err;
    }
  }

  handle->signum = signum;
  if (oneshot) {
    handle->flags |= UV_SIGNAL_ONE_SHOT;
  }

  uv__signal_tree_s_RB_INSERT(&uv__signal_tree, handle);

  uv__signal_unlock_and_unblock(&saved_sigmask);

  handle->signal_cb = signal_cb;
  if ((handle->flags & UV_HANDLE_ACTIVE) != 0) {
    return 0;           
  }              
  handle->flags |= UV_HANDLE_ACTIVE;                                           
  if ((handle->flags & UV_HANDLE_REF) != 0) {
    handle->loop->active_handles++;
  }

  return 0;
}


static void uv__signal_event(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  uv__signal_msg_t* msg;
  uv_signal_t* handle;
  char buf[sizeof(uv__signal_msg_t) * 32];
  size_t bytes, end, i;
  int r;

  bytes = 0;
  end = 0;

  do {
    r = read(loop->signal_pipefd[0], buf + bytes, sizeof(buf) - bytes);

    if (r == -1 && errno == EINTR) {
      continue;
    }

    if (r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      /* If there are bytes in the buffer already (which really is extremely
       * unlikely if possible at all) we can't exit the function here. We'll
       * spin until more bytes are read instead.
       */
      if (bytes > 0) {
        continue;
      }

      /* Otherwise, there was nothing there. */
      return;
    }

    /* Other errors really should never happen. */
    if (r == -1) {
      abort();
    }

    bytes += r;

    /* `end` is rounded down to a multiple of sizeof(uv__signal_msg_t). */
    end = (bytes / sizeof(uv__signal_msg_t)) * sizeof(uv__signal_msg_t);

    for (i = 0; i < end; i += sizeof(uv__signal_msg_t)) {
      msg = (uv__signal_msg_t*) (buf + i);
      handle = msg->handle;

      if (msg->signum == handle->signum) {
        assert(!(handle->flags & UV_HANDLE_CLOSING));
        handle->signal_cb(handle, handle->signum);
      }

      handle->dispatched_signals++;

      if (handle->flags & UV_SIGNAL_ONE_SHOT) {
        uv__signal_stop(handle);
      }
    }

    bytes -= end;

    /* If there are any "partial" messages left, move them to the start of the
     * the buffer, and spin. This should not happen.
     */
    if (bytes) {
      memmove(buf, buf + end, bytes);
      continue;
    }
  } while (end == sizeof buf);
}


static int uv__signal_compare(uv_signal_t* w1, uv_signal_t* w2) {
  int f1;
  int f2;
  /* Compare signums first so all watchers with the same signnum end up
   * adjacent.
   */
  if (w1->signum < w2->signum) return -1;
  if (w1->signum > w2->signum) return 1;

  /* Handlers without UV_SIGNAL_ONE_SHOT set will come first, so if the first
   * handler returned is a one-shot handler, the rest will be too.
   */
  f1 = w1->flags & UV_SIGNAL_ONE_SHOT;
  f2 = w2->flags & UV_SIGNAL_ONE_SHOT;
  if (f1 < f2) return -1;
  if (f1 > f2) return 1;

  /* Sort by loop pointer, so we can easily look up the first item after
   * { .signum = x, .loop = NULL }.
   */
  if (w1->loop < w2->loop) return -1;
  if (w1->loop > w2->loop) return 1;

  if (w1 < w2) return -1;
  if (w1 > w2) return 1;

  return 0;
}


int uv_signal_stop(uv_signal_t* handle) {
  assert(((handle->flags & (UV_HANDLE_CLOSING | UV_HANDLE_CLOSED)) == 0));
  uv__signal_stop(handle);
  return 0;
}


static void uv__signal_stop(uv_signal_t* handle) {
  uv_signal_t* removed_handle;
  sigset_t saved_sigmask;
  uv_signal_t* first_handle;
  int rem_oneshot;
  int first_oneshot;
  int ret;

  /* If the watcher wasn't started, this is a no-op. */
  if (handle->signum == 0) {
    return;
  }

  uv__signal_block_and_lock(&saved_sigmask);

  removed_handle = uv__signal_tree_s_RB_REMOVE(&uv__signal_tree, handle);
  assert(removed_handle == handle);
  (void) removed_handle;

  /* Check if there are other active signal watchers observing this signal. If
   * not, unregister the signal handler.
   */
  first_handle = uv__signal_first_handle(handle->signum);
  if (first_handle == NULL) {
    uv__signal_unregister_handler(handle->signum);
  } else {
    rem_oneshot = handle->flags & UV_SIGNAL_ONE_SHOT;
    first_oneshot = first_handle->flags & UV_SIGNAL_ONE_SHOT;
    if (first_oneshot && !rem_oneshot) {
      ret = uv__signal_register_handler(handle->signum, 1);
      assert(ret == 0);
      (void)ret;
    }
  }

  uv__signal_unlock_and_unblock(&saved_sigmask);

  handle->signum = 0;
  if ((handle->flags & UV_HANDLE_ACTIVE) == 0) {
    return;
  }
  handle->flags &= ~UV_HANDLE_ACTIVE;
  if ((handle->flags & UV_HANDLE_REF) != 0) {
    handle->loop->active_handles--;
  }
}
