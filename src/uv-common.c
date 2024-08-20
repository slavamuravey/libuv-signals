#include "uv.h"
#include "uv-common.h"
#include <stdlib.h>
#include <errno.h>

typedef struct {
  uv_malloc_func local_malloc;
  uv_realloc_func local_realloc;
  uv_calloc_func local_calloc;
  uv_free_func local_free;
} uv__allocator_t;

static uv__allocator_t uv__allocator = {
  malloc,
  realloc,
  calloc,
  free,
};

void uv_unref(uv_handle_t* handle) {
  if (!(handle->flags & UV_HANDLE_REF)) {
    return;
  }                       
  
  handle->flags &= ~UV_HANDLE_REF;                                             
  
  if (handle->flags & UV_HANDLE_ACTIVE) {
    handle->loop->active_handles--;
  }
}

void uv__free(void* ptr) {
  int saved_errno;

  /* Libuv expects that free() does not clobber errno.  The system allocator
   * honors that assumption but custom allocators may not be so careful.
   */
  saved_errno = errno;
  uv__allocator.local_free(ptr);
  errno = saved_errno;
}

void* uv__realloc(void* ptr, size_t size) {
  if (size > 0) {
    return uv__allocator.local_realloc(ptr, size);
  }
  uv__free(ptr);
  return NULL;
}

void* uv__reallocf(void* ptr, size_t size) {
  void* newptr;

  newptr = uv__realloc(ptr, size);
  if (newptr == NULL) {
    if (size > 0) {
      uv__free(ptr);
    }
  }

  return newptr;
}