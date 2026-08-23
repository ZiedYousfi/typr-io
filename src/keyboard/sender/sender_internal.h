#pragma once
#ifndef AXIDEV_IO_KEYBOARD_SENDER_INTERNAL_H
#define AXIDEV_IO_KEYBOARD_SENDER_INTERNAL_H

#include "../../internal/thread.h"
#include "../common/keymap_internal.h"

typedef struct axidev_io_keyboard_sender_impl {
#ifdef _WIN32
  void *layout;
  void *repeat_entries;
  void *repeat_wake_event;
  void *repeat_idle_event;
  axidev_io_mutex repeat_lock;
  axidev_io_thread repeat_worker;
  bool repeat_lock_initialized;
  bool repeat_worker_running;
  bool repeat_stop_worker;
  size_t repeat_len;
  size_t repeat_cap;
#elif defined(__linux__)
  int fd;
  void *xkb_ctx;
  void *xkb_keymap;
  void *xkb_state;
#else
  int unused;
#endif
} axidev_io_keyboard_sender_impl;

_Static_assert(sizeof(axidev_io_keyboard_sender_impl) <=
                   AXIDEV_IO_KEYBOARD_SENDER_STORAGE_SIZE,
               "sender storage is too small");

axidev_io_keyboard_sender_impl *axidev_io_sender_impl_get(void);

axidev_io_result axidev_io_keyboard_sender_initialize(void);
void axidev_io_keyboard_sender_free(void);
axidev_io_result axidev_io_keyboard_sender_request_permissions(void);
axidev_io_result axidev_io_keyboard_sender_key_down_internal(
    axidev_io_keyboard_key_with_modifier_t key_mod, bool repeat);
axidev_io_result axidev_io_keyboard_sender_key_up_internal(
    axidev_io_keyboard_key_with_modifier_t key_mod);
axidev_io_result axidev_io_keyboard_sender_tap_internal(
    axidev_io_keyboard_key_with_modifier_t key_mod);
axidev_io_result axidev_io_keyboard_sender_hold_modifier_internal(
    axidev_io_keyboard_modifier_t mods);
axidev_io_result axidev_io_keyboard_sender_release_modifier_internal(
    axidev_io_keyboard_modifier_t mods);
axidev_io_result axidev_io_keyboard_sender_release_all_modifiers_internal(void);
axidev_io_result
axidev_io_keyboard_sender_type_character_internal(uint32_t codepoint);
void axidev_io_keyboard_sender_flush_internal(void);
void axidev_io_keyboard_sender_set_key_delay_internal(uint32_t delay_us);

#ifdef _WIN32
size_t axidev_io_windows_sender_repeat_count_for_tests(void);
#endif

#endif
