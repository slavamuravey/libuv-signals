#include "uv.h"
#include "queue.h"
#include "tree.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

enum {
  UV_HANDLE_ACTIVE = 0x00000004,
  UV_HANDLE_REF    = 0x00000008
};

enum {
  UV_HANDLE_CLOSING  = 0x00000001,
  UV_HANDLE_CLOSED   = 0x00000002,
  UV_HANDLE_INTERNAL = 0x00000010,
  UV_SIGNAL_ONE_SHOT = 0x02000000
};

void uv__io_init(uv__io_t* w, uv__io_cb cb, int fd);
void uv__io_start(uv_loop_t* loop, uv__io_t* w, unsigned int events);

/* Allocator prototypes */
void uv__free(void* ptr);
void* uv__realloc(void* ptr, size_t size);
void* uv__reallocf(void* ptr, size_t size);