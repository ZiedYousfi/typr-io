#pragma once
#ifndef AXIDEV_IO_MOUSE_INTERNAL_H
#define AXIDEV_IO_MOUSE_INTERNAL_H

#include "../internal/context.h"

#include <stdatomic.h>

typedef struct axidev_io_mouse_impl {
  axidev_io_mouse_listener_cb callback;
  void *user_data;
  axidev_io_mutex callback_lock;
  axidev_io_mutex state_lock;
  bool callback_lock_ready;
  bool state_lock_ready;
  axidev_io_thread worker;
  atomic_bool running;
  atomic_bool ready;
#ifdef _WIN32
  void *hook;
  uint32_t thread_id;
#elif defined(__linux__)
  struct axidev_io_linux_mouse_platform *platform;
#endif
} axidev_io_mouse_impl;

_Static_assert(sizeof(axidev_io_mouse_impl) <= AXIDEV_IO_MOUSE_STORAGE_SIZE,
               "mouse storage is too small");

axidev_io_mouse_impl *axidev_io_mouse_impl_get(void);

axidev_io_result axidev_io_mouse_poll_internal(
    axidev_io_mouse_state_t *out_state);
axidev_io_result axidev_io_mouse_listener_start_internal(
    axidev_io_mouse_listener_cb callback, void *user_data);
void axidev_io_mouse_listener_stop_internal(void);

void axidev_io_mouse_store_state(axidev_io_mouse_impl *impl,
                                 const axidev_io_mouse_state_t *state);
void axidev_io_mouse_load_state(axidev_io_mouse_impl *impl,
                                axidev_io_mouse_state_t *out_state);
void axidev_io_mouse_invoke_callback(axidev_io_mouse_impl *impl,
                                     const axidev_io_mouse_state_t *state);
bool axidev_io_mouse_prepare_locks(axidev_io_mouse_impl *impl);

#endif
