#pragma once
#ifndef AXIDEV_IO_INTERNAL_CONTEXT_H
#define AXIDEV_IO_INTERNAL_CONTEXT_H

#include <axidev-io/c_api.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "result.h"
#include "thread.h"

typedef struct axidev_io_private_runtime {
  axidev_io_mutex state_lock;
  axidev_io_mutex error_lock;
} axidev_io_private_runtime;

void axidev_io_context_ensure_runtime(void);
axidev_io_private_runtime *axidev_io_private_runtime_get(void);

void axidev_io_context_lock(void);
void axidev_io_context_unlock(void);

void axidev_io_set_last_error_message(const char *message);
void axidev_io_set_last_errorf(const char *fmt, ...);
void axidev_io_set_last_errorfv(const char *fmt, va_list args);
void axidev_io_set_last_error_result(const char *function_name,
                                     axidev_io_result result);
void axidev_io_clear_last_error_internal(void);
char *axidev_io_duplicate_string(const char *text);

void axidev_io_keyboard_reset_public_sender_state(void);
void axidev_io_keyboard_reset_public_listener_state(void);
void axidev_io_keyboard_reset_public_keymap_state(void);
void axidev_io_keyboard_reset_public_state(void);
void axidev_io_mouse_reset_public_state(void);

static inline void *axidev_io_sender_storage_ptr(void) {
  return (void *)axidev_io_global->keyboard.sender.storage.bytes;
}

static inline void *axidev_io_listener_storage_ptr(void) {
  return (void *)axidev_io_global->keyboard.listener.storage.bytes;
}

static inline void *axidev_io_keymap_storage_ptr(void) {
  return (void *)axidev_io_global->keyboard.keymap.storage.bytes;
}

static inline void *axidev_io_mouse_storage_ptr(void) {
  return (void *)axidev_io_global->mouse.storage.bytes;
}

static inline axidev_io_keyboard_sender_context *
axidev_io_sender_public_context(void) {
  return &axidev_io_global->keyboard.sender;
}

static inline axidev_io_keyboard_listener_context *
axidev_io_listener_public_context(void) {
  return &axidev_io_global->keyboard.listener;
}

static inline axidev_io_keyboard_keymap_context *
axidev_io_keymap_public_context(void) {
  return &axidev_io_global->keyboard.keymap;
}

static inline axidev_io_mouse_context *axidev_io_mouse_public_context(void) {
  return &axidev_io_global->mouse;
}

static inline bool
axidev_io_keyboard_has_modifier(axidev_io_keyboard_modifier_t state,
                                axidev_io_keyboard_modifier_t flag) {
  return (state & flag) != 0;
}

static inline axidev_io_keyboard_modifier_t
axidev_io_keyboard_add_modifier(axidev_io_keyboard_modifier_t state,
                                axidev_io_keyboard_modifier_t flag) {
  return (axidev_io_keyboard_modifier_t)(state | flag);
}

static inline axidev_io_keyboard_modifier_t
axidev_io_keyboard_remove_modifier(axidev_io_keyboard_modifier_t state,
                                   axidev_io_keyboard_modifier_t flag) {
  return (axidev_io_keyboard_modifier_t)(state & (uint8_t)~flag);
}

#endif
