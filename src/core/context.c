#include "../internal/context.h"

#include <axidev-io/c_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static axidev_io_once g_context_once = AXIDEV_IO_ONCE_INIT;
static axidev_io_global_context g_axidev_io_storage = {
    .log_level = AXIDEV_IO_LOG_LEVEL_INFO};

AXIDEV_IO_API axidev_io_global_context *axidev_io_global = &g_axidev_io_storage;

static void axidev_io_context_init_once(void) {
  axidev_io_private_runtime *runtime =
      (axidev_io_private_runtime *)axidev_io_global->private_storage.bytes;

  memset(runtime, 0, sizeof(*runtime));
  axidev_io_mutex_init(&runtime->state_lock);
  axidev_io_mutex_init(&runtime->error_lock);
  axidev_io_global->log_level = AXIDEV_IO_LOG_LEVEL_INFO;
  axidev_io_keyboard_reset_public_state();
  axidev_io_mouse_reset_public_state();
}

void axidev_io_context_ensure_runtime(void) {
  axidev_io_call_once(&g_context_once, axidev_io_context_init_once);
}

axidev_io_private_runtime *axidev_io_private_runtime_get(void) {
  axidev_io_context_ensure_runtime();
  return (axidev_io_private_runtime *)axidev_io_global->private_storage.bytes;
}

void axidev_io_context_lock(void) {
  axidev_io_private_runtime *runtime = axidev_io_private_runtime_get();
  axidev_io_mutex_lock(&runtime->state_lock);
}

void axidev_io_context_unlock(void) {
  axidev_io_private_runtime *runtime = axidev_io_private_runtime_get();
  axidev_io_mutex_unlock(&runtime->state_lock);
}

static void axidev_io_error_lock(void) {
  axidev_io_private_runtime *runtime = axidev_io_private_runtime_get();
  axidev_io_mutex_lock(&runtime->error_lock);
}

static void axidev_io_error_unlock(void) {
  axidev_io_private_runtime *runtime = axidev_io_private_runtime_get();
  axidev_io_mutex_unlock(&runtime->error_lock);
}

char *axidev_io_duplicate_string(const char *text) {
  size_t length;
  char *copy;

  if (text == NULL) {
    return NULL;
  }

  length = strlen(text);
  copy = (char *)malloc(length + 1u);
  if (copy == NULL) {
    return NULL;
  }

  memcpy(copy, text, length + 1u);
  return copy;
}

void axidev_io_clear_last_error_internal(void) {
  axidev_io_error_lock();
  free(axidev_io_global->last_error);
  axidev_io_global->last_error = NULL;
  axidev_io_error_unlock();
}

void axidev_io_set_last_error_message(const char *message) {
  char *copy = axidev_io_duplicate_string(message != NULL ? message : "");

  axidev_io_error_lock();
  free(axidev_io_global->last_error);
  axidev_io_global->last_error = copy;
  axidev_io_error_unlock();
}

void axidev_io_set_last_errorfv(const char *fmt, va_list args) {
  va_list copy_args;
  int needed;
  char *buffer;

  if (fmt == NULL) {
    axidev_io_set_last_error_message("");
    return;
  }

  va_copy(copy_args, args);
#ifdef _WIN32
  needed = _vscprintf(fmt, copy_args);
#else
  needed = vsnprintf(NULL, 0, fmt, copy_args);
#endif
  va_end(copy_args);
  if (needed < 0) {
    axidev_io_set_last_error_message("formatting error");
    return;
  }

  buffer = (char *)malloc((size_t)needed + 1u);
  if (buffer == NULL) {
    return;
  }

  vsnprintf(buffer, (size_t)needed + 1u, fmt, args);
  axidev_io_error_lock();
  free(axidev_io_global->last_error);
  axidev_io_global->last_error = buffer;
  axidev_io_error_unlock();
}

void axidev_io_set_last_errorf(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  axidev_io_set_last_errorfv(fmt, args);
  va_end(args);
}

void axidev_io_set_last_error_result(const char *function_name,
                                     axidev_io_result result) {
  if (function_name == NULL) {
    axidev_io_set_last_error_message(axidev_io_result_to_string(result));
    return;
  }
  axidev_io_set_last_errorf("%s: %s", function_name,
                            axidev_io_result_to_string(result));
}

void axidev_io_keyboard_reset_public_sender_state(void) {
  memset(&axidev_io_global->keyboard.sender, 0,
         sizeof(axidev_io_global->keyboard.sender));
  axidev_io_global->keyboard.sender.key_delay_us = 1000u;
}

void axidev_io_keyboard_reset_public_listener_state(void) {
  memset(&axidev_io_global->keyboard.listener, 0,
         sizeof(axidev_io_global->keyboard.listener));
}

void axidev_io_keyboard_reset_public_keymap_state(void) {
  memset(&axidev_io_global->keyboard.keymap, 0,
         sizeof(axidev_io_global->keyboard.keymap));
}

void axidev_io_keyboard_reset_public_state(void) {
  memset(&axidev_io_global->keyboard, 0, sizeof(axidev_io_global->keyboard));
  axidev_io_keyboard_reset_public_sender_state();
  axidev_io_keyboard_reset_public_listener_state();
  axidev_io_keyboard_reset_public_keymap_state();
  axidev_io_global->keyboard.backend_type = AXIDEV_IO_BACKEND_UNKNOWN;
}

void axidev_io_mouse_reset_public_state(void) {
  memset(&axidev_io_global->mouse, 0, sizeof(axidev_io_global->mouse));
  axidev_io_global->mouse.backend_type = AXIDEV_IO_BACKEND_UNKNOWN;
}
