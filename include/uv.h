#ifndef UV_H
#define UV_H

#include <stdint.h>
#include <pthread.h>
#include <signal.h>

#if defined(O_NONBLOCK)
# define UV_FS_O_NONBLOCK     O_NONBLOCK
#else
# define UV_FS_O_NONBLOCK     0
#endif

typedef int uv_os_fd_t;

/* uv_spawn() options. */
typedef enum {
  UV_IGNORE         = 0x00,
  UV_CREATE_PIPE    = 0x01,
  UV_INHERIT_FD     = 0x02,
  UV_INHERIT_STREAM = 0x04,

  /*
   * When UV_CREATE_PIPE is specified, UV_READABLE_PIPE and UV_WRITABLE_PIPE
   * determine the direction of flow, from the child process' perspective. Both
   * flags may be specified to create a duplex data stream.
   */
  UV_READABLE_PIPE  = 0x10,
  UV_WRITABLE_PIPE  = 0x20,

  /*
   * When UV_CREATE_PIPE is specified, specifying UV_NONBLOCK_PIPE opens the
   * handle in non-blocking mode in the child. This may cause loss of data,
   * if the child is not designed to handle to encounter this mode,
   * but can also be significantly more efficient.
   */
  UV_NONBLOCK_PIPE  = 0x40,
  UV_OVERLAPPED_PIPE = 0x40 /* old name, for compatibility */
} uv_stdio_flags;

typedef int uv_file;

#define UV_ONCE_INIT PTHREAD_ONCE_INIT

typedef pthread_once_t uv_once_t;
typedef pthread_t uv_thread_t;
typedef void (*uv_thread_cb)(void* arg);

typedef enum {
  UV_THREAD_NO_FLAGS = 0x00,
  UV_THREAD_HAS_STACK_SIZE = 0x01
} uv_thread_create_flags;

struct uv_thread_options_s {
  unsigned int flags;
  size_t stack_size;
  /* More fields may be added at any time. */
};

typedef struct uv_thread_options_s uv_thread_options_t;

int uv_thread_create(uv_thread_t* tid, uv_thread_cb entry, void* arg);
int uv_thread_create_ex(uv_thread_t* tid,
                                  const uv_thread_options_t* params,
                                  uv_thread_cb entry,
                                  void* arg);
int uv_thread_join(uv_thread_t *tid);

void uv_once(uv_once_t* guard, void (*callback)(void));

typedef void* (*uv_malloc_func)(size_t size);
typedef void* (*uv_realloc_func)(void* ptr, size_t size);
typedef void* (*uv_calloc_func)(size_t count, size_t size);
typedef void (*uv_free_func)(void* ptr);

struct uv__io_s;
struct uv_loop_s;

typedef void (*uv__io_cb)(struct uv_loop_s* loop, struct uv__io_s* w, unsigned int events);
typedef struct uv__io_s uv__io_t;

/* Internal type, do not use. */
struct uv__queue {
  struct uv__queue* next;
  struct uv__queue* prev;
};

struct uv__io_s {
  uv__io_cb cb;
  struct uv__queue pending_queue;
  struct uv__queue watcher_queue;
  unsigned int pevents; /* Pending event mask i.e. mask at next tick. */
  unsigned int events;  /* Current event mask. */
  int fd;
};

typedef enum {
  UV_RUN_DEFAULT = 0,
  UV_RUN_ONCE,
  UV_RUN_NOWAIT
} uv_run_mode;

/* Handle types. */
typedef struct uv_loop_s uv_loop_t;
typedef struct uv_handle_s uv_handle_t;
typedef struct uv_signal_s uv_signal_t;

typedef void (*uv_signal_cb)(uv_signal_t* handle, int signum);

struct uv_loop_s {
  /* Loop reference counting. */
  unsigned int active_handles;
  unsigned long flags;                                                      
  int backend_fd;
  struct uv__queue watcher_queue;
  uv__io_t** watchers;
  unsigned int nwatchers;
  unsigned int nfds;
  int signal_pipefd[2];
  uv__io_t signal_io_watcher;
};

/* The abstract base class of all handles. */
struct uv_handle_s {
  uv_loop_t* loop;                                                            
  unsigned int flags;                                                    
};

struct uv_signal_s {
  uv_loop_t* loop;
  unsigned int flags;
  
  uv_signal_cb signal_cb;
  int signum;
  /* RB_ENTRY(uv_signal_s) tree_entry; */                                     
  struct {                                                                    
    struct uv_signal_s* rbe_left;                                             
    struct uv_signal_s* rbe_right;                                            
    struct uv_signal_s* rbe_parent;                                           
    int rbe_color;                                                            
  } tree_entry;                                                               
  /* Use two counters here so we don have to fiddle with atomics. */          
  unsigned int caught_signals;                                                
  unsigned int dispatched_signals;
};

int uv_signal_init(uv_loop_t* loop, uv_signal_t* handle);
int uv_signal_start(uv_signal_t* handle, uv_signal_cb signal_cb, int signum);
int uv_signal_start_oneshot(uv_signal_t* handle, uv_signal_cb signal_cb, int signum);
int uv_signal_stop(uv_signal_t* handle);

int uv_loop_init(uv_loop_t* loop);
int uv_run(uv_loop_t*, uv_run_mode mode);

void uv_unref(uv_handle_t*);

int uv_pipe(uv_file fds[2], int read_flags, int write_flags);

#endif