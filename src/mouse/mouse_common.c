#include "mouse_internal.h"

axidev_io_mouse_impl *axidev_io_mouse_impl_get(void) {
  return (axidev_io_mouse_impl *)axidev_io_mouse_storage_ptr();
}

bool axidev_io_mouse_prepare_locks(axidev_io_mouse_impl *impl) {
  if (impl == NULL) {
    return false;
  }
  if (!impl->callback_lock_ready) {
    if (!axidev_io_mutex_init(&impl->callback_lock)) {
      return false;
    }
    impl->callback_lock_ready = true;
  }
  if (!impl->state_lock_ready) {
    if (!axidev_io_mutex_init(&impl->state_lock)) {
      return false;
    }
    impl->state_lock_ready = true;
  }
  return true;
}

void axidev_io_mouse_store_state(axidev_io_mouse_impl *impl,
                                 const axidev_io_mouse_state_t *state) {
  if (impl == NULL || state == NULL) {
    return;
  }
  if (impl->state_lock_ready) {
    axidev_io_mutex_lock(&impl->state_lock);
  }
  axidev_io_mouse_public_context()->state = *state;
  if (impl->state_lock_ready) {
    axidev_io_mutex_unlock(&impl->state_lock);
  }
}

void axidev_io_mouse_load_state(axidev_io_mouse_impl *impl,
                                axidev_io_mouse_state_t *out_state) {
  if (out_state == NULL) {
    return;
  }
  if (impl != NULL && impl->state_lock_ready) {
    axidev_io_mutex_lock(&impl->state_lock);
  }
  *out_state = axidev_io_mouse_public_context()->state;
  if (impl != NULL && impl->state_lock_ready) {
    axidev_io_mutex_unlock(&impl->state_lock);
  }
}

void axidev_io_mouse_invoke_callback(axidev_io_mouse_impl *impl,
                                     const axidev_io_mouse_state_t *state) {
  axidev_io_mouse_listener_cb callback = NULL;
  void *user_data = NULL;

  if (impl == NULL || state == NULL) {
    return;
  }

  axidev_io_mutex_lock(&impl->callback_lock);
  callback = impl->callback;
  user_data = impl->user_data;
  axidev_io_mutex_unlock(&impl->callback_lock);

  if (callback != NULL) {
    callback(state, user_data);
  }
}
