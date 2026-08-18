#if defined(__linux__)

#include "mouse_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <libudev.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

struct axidev_io_linux_mouse_platform {
  struct libinput *libinput;
  struct udev *udev;
};

static int axidev_io_mouse_open_restricted(const char *path, int flags,
                                           void *user_data) {
  (void)user_data;
  {
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
  }
}

static void axidev_io_mouse_close_restricted(int fd, void *user_data) {
  (void)user_data;
  close(fd);
}

static const struct libinput_interface g_mouse_libinput_interface = {
    axidev_io_mouse_open_restricted, axidev_io_mouse_close_restricted};

static axidev_io_mouse_button_t
axidev_io_linux_button_flag(uint32_t button) {
  switch (button) {
  case BTN_LEFT:
    return AXIDEV_IO_MOUSE_BUTTON_LEFT;
  case BTN_RIGHT:
    return AXIDEV_IO_MOUSE_BUTTON_RIGHT;
  case BTN_MIDDLE:
    return AXIDEV_IO_MOUSE_BUTTON_MIDDLE;
  case BTN_BACK:
  case BTN_SIDE:
    return AXIDEV_IO_MOUSE_BUTTON_BACK;
  case BTN_FORWARD:
  case BTN_EXTRA:
    return AXIDEV_IO_MOUSE_BUTTON_FORWARD;
  default:
    return AXIDEV_IO_MOUSE_BUTTON_NONE;
  }
}

static void axidev_io_linux_mouse_close(
    struct axidev_io_linux_mouse_platform *platform) {
  if (platform == NULL) {
    return;
  }
  if (platform->libinput != NULL) {
    libinput_unref(platform->libinput);
    platform->libinput = NULL;
  }
  if (platform->udev != NULL) {
    udev_unref(platform->udev);
    platform->udev = NULL;
  }
}

static axidev_io_result axidev_io_linux_mouse_open(
    struct axidev_io_linux_mouse_platform *platform) {
  if (platform == NULL) {
    return AXIDEV_IO_RESULT_INVALID_ARGUMENT;
  }
  platform->udev = udev_new();
  if (platform->udev == NULL) {
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }
  platform->libinput =
      libinput_udev_create_context(&g_mouse_libinput_interface, NULL,
                                   platform->udev);
  if (platform->libinput == NULL ||
      libinput_udev_assign_seat(platform->libinput, "seat0") < 0) {
    axidev_io_linux_mouse_close(platform);
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }
  return AXIDEV_IO_RESULT_OK;
}

static void axidev_io_linux_mouse_handle_event(
    axidev_io_mouse_impl *impl, struct libinput_event *event) {
  axidev_io_mouse_state_t state;
  enum libinput_event_type type;
  bool changed = false;

  if (impl == NULL || event == NULL) {
    return;
  }

  axidev_io_mouse_load_state(impl, &state);
  state.scroll_x = 0.0;
  state.scroll_y = 0.0;
  state.timestamp_ms = axidev_io_monotonic_time_ms();

  type = libinput_event_get_type(event);
  if (type == LIBINPUT_EVENT_POINTER_MOTION) {
    struct libinput_event_pointer *pointer =
        libinput_event_get_pointer_event(event);
    state.x += (int32_t)libinput_event_pointer_get_dx(pointer);
    state.y += (int32_t)libinput_event_pointer_get_dy(pointer);
    changed = true;
  } else if (type == LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE) {
    struct libinput_event_pointer *pointer =
        libinput_event_get_pointer_event(event);
    state.x = (int32_t)libinput_event_pointer_get_absolute_x_transformed(
        pointer, 1000000);
    state.y = (int32_t)libinput_event_pointer_get_absolute_y_transformed(
        pointer, 1000000);
    changed = true;
  } else if (type == LIBINPUT_EVENT_POINTER_BUTTON) {
    struct libinput_event_pointer *pointer =
        libinput_event_get_pointer_event(event);
    axidev_io_mouse_button_t flag =
        axidev_io_linux_button_flag(libinput_event_pointer_get_button(pointer));
    if (flag != AXIDEV_IO_MOUSE_BUTTON_NONE) {
      if (libinput_event_pointer_get_button_state(pointer) ==
          LIBINPUT_BUTTON_STATE_PRESSED) {
        state.buttons |= flag;
      } else {
        state.buttons = (axidev_io_mouse_button_t)(state.buttons & ~flag);
      }
      changed = true;
    }
  } else if (type == LIBINPUT_EVENT_POINTER_AXIS) {
    struct libinput_event_pointer *pointer =
        libinput_event_get_pointer_event(event);
    if (libinput_event_pointer_has_axis(pointer,
                                        LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
      state.scroll_y = libinput_event_pointer_get_axis_value(
          pointer, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
      changed = true;
    }
    if (libinput_event_pointer_has_axis(pointer,
                                        LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
      state.scroll_x = libinput_event_pointer_get_axis_value(
          pointer, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
      changed = true;
    }
  }

  if (changed) {
    axidev_io_mouse_store_state(impl, &state);
    axidev_io_mouse_invoke_callback(impl, &state);
  }
}

axidev_io_result axidev_io_mouse_poll_internal(
    axidev_io_mouse_state_t *out_state) {
  axidev_io_mouse_impl *impl = axidev_io_mouse_impl_get();

  if (out_state == NULL) {
    return AXIDEV_IO_RESULT_INVALID_ARGUMENT;
  }
  if (!axidev_io_mouse_prepare_locks(impl)) {
    return AXIDEV_IO_RESULT_INTERNAL_ERROR;
  }
  axidev_io_mouse_load_state(impl, out_state);
  out_state->timestamp_ms = axidev_io_monotonic_time_ms();
  axidev_io_mouse_public_context()->initialized = true;
  axidev_io_mouse_public_context()->backend_type =
      AXIDEV_IO_BACKEND_LINUX_LIBINPUT;
  return AXIDEV_IO_RESULT_OK;
}

static int axidev_io_mouse_thread_main(void *user_data) {
  axidev_io_mouse_impl *impl = (axidev_io_mouse_impl *)user_data;
  struct axidev_io_linux_mouse_platform *platform = impl->platform;
  struct pollfd poll_fd;

  if (platform == NULL ||
      axidev_io_linux_mouse_open(platform) != AXIDEV_IO_RESULT_OK) {
    atomic_store(&impl->running, false);
    return 1;
  }

  poll_fd.fd = libinput_get_fd(platform->libinput);
  poll_fd.events = POLLIN;
  poll_fd.revents = 0;
  atomic_store(&impl->ready, true);

  while (atomic_load(&impl->running)) {
    int poll_result = poll(&poll_fd, 1, 100);
    if (poll_result > 0 && (poll_fd.revents & POLLIN)) {
      struct libinput_event *event;
      libinput_dispatch(platform->libinput);
      while ((event = libinput_get_event(platform->libinput)) != NULL) {
        axidev_io_linux_mouse_handle_event(impl, event);
        libinput_event_destroy(event);
      }
    }
    axidev_io_sleep_ms(1);
  }

  axidev_io_linux_mouse_close(platform);
  atomic_store(&impl->ready, false);
  return 0;
}

axidev_io_result axidev_io_mouse_listener_start_internal(
    axidev_io_mouse_listener_cb callback, void *user_data) {
  axidev_io_mouse_impl *impl = axidev_io_mouse_impl_get();

  if (callback == NULL) {
    return AXIDEV_IO_RESULT_INVALID_ARGUMENT;
  }
  if (!axidev_io_mouse_prepare_locks(impl)) {
    return AXIDEV_IO_RESULT_INTERNAL_ERROR;
  }
  if (atomic_load(&impl->running)) {
    return AXIDEV_IO_RESULT_ALREADY_INITIALIZED;
  }
  if (impl->platform == NULL) {
    impl->platform = (struct axidev_io_linux_mouse_platform *)calloc(
        1, sizeof(*impl->platform));
    if (impl->platform == NULL) {
      return AXIDEV_IO_RESULT_INTERNAL_ERROR;
    }
  }

  axidev_io_mutex_lock(&impl->callback_lock);
  impl->callback = callback;
  impl->user_data = user_data;
  axidev_io_mutex_unlock(&impl->callback_lock);

  atomic_store(&impl->running, true);
  atomic_store(&impl->ready, false);
  if (!axidev_io_thread_create(&impl->worker, axidev_io_mouse_thread_main,
                               impl)) {
    atomic_store(&impl->running, false);
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }

  for (int i = 0; i < 40; ++i) {
    if (!atomic_load(&impl->running)) {
      axidev_io_thread_join(&impl->worker);
      return AXIDEV_IO_RESULT_PLATFORM_ERROR;
    }
    if (atomic_load(&impl->ready)) {
      axidev_io_mouse_public_context()->initialized = true;
      axidev_io_mouse_public_context()->is_listening = true;
      axidev_io_mouse_public_context()->backend_type =
          AXIDEV_IO_BACKEND_LINUX_LIBINPUT;
      return AXIDEV_IO_RESULT_OK;
    }
    axidev_io_sleep_ms(5);
  }

  atomic_store(&impl->running, false);
  axidev_io_thread_join(&impl->worker);
  return AXIDEV_IO_RESULT_PLATFORM_ERROR;
}

void axidev_io_mouse_listener_stop_internal(void) {
  axidev_io_mouse_impl *impl = axidev_io_mouse_impl_get();

  if (!atomic_load(&impl->running)) {
    axidev_io_mouse_public_context()->is_listening = false;
    return;
  }

  atomic_store(&impl->running, false);
  axidev_io_thread_join(&impl->worker);
  axidev_io_mouse_public_context()->is_listening = false;
  axidev_io_mouse_public_context()->initialized = true;
}

#endif
