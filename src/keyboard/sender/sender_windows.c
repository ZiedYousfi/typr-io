#ifdef _WIN32

#include "sender_internal.h"

#include <Windows.h>

#include <axidev-io/c_api.h>

#include <stdlib.h>
#include <string.h>

#include "../common/key_utils_internal.h"
#include "../common/windows_keymap_internal.h"

typedef struct axidev_io_windows_repeat_entry {
  axidev_io_keyboard_key_with_modifier_t request;
  axidev_io_keyboard_key_t resolved_key;
  int32_t keycode;
  axidev_io_keyboard_modifier_t mods;
  uint64_t next_fire_at_ns;
  uint64_t interval_ns;
} axidev_io_windows_repeat_entry;

typedef struct axidev_io_windows_repeat_send {
  axidev_io_keyboard_key_t resolved_key;
  int32_t keycode;
} axidev_io_windows_repeat_send;

axidev_io_keyboard_sender_impl *axidev_io_sender_impl_get(void) {
  return (axidev_io_keyboard_sender_impl *)axidev_io_sender_storage_ptr();
}

static void axidev_io_sender_delay(void) {
  uint32_t delay_us = axidev_io_sender_public_context()->key_delay_us;
  if (delay_us != 0) {
    axidev_io_sleep_us(delay_us);
  }
}

static void axidev_io_sender_update_modifier_state(axidev_io_keyboard_key_t key,
                                                   bool down) {
  axidev_io_keyboard_sender_context *sender = axidev_io_sender_public_context();
  axidev_io_keyboard_modifier_t flag = AXIDEV_IO_MOD_NONE;

  switch (key) {
  case AXIDEV_IO_KEY_SHIFT_LEFT:
  case AXIDEV_IO_KEY_SHIFT_RIGHT:
    flag = AXIDEV_IO_MOD_SHIFT;
    break;
  case AXIDEV_IO_KEY_CTRL_LEFT:
  case AXIDEV_IO_KEY_CTRL_RIGHT:
    flag = AXIDEV_IO_MOD_CTRL;
    break;
  case AXIDEV_IO_KEY_ALT_LEFT:
  case AXIDEV_IO_KEY_ALT_RIGHT:
    flag = AXIDEV_IO_MOD_ALT;
    break;
  case AXIDEV_IO_KEY_SUPER_LEFT:
  case AXIDEV_IO_KEY_SUPER_RIGHT:
    flag = AXIDEV_IO_MOD_SUPER;
    break;
  default:
    break;
  }

  if (flag == AXIDEV_IO_MOD_NONE) {
    return;
  }

  if (down) {
    sender->active_modifiers =
        axidev_io_keyboard_add_modifier(sender->active_modifiers, flag);
  } else {
    sender->active_modifiers =
        axidev_io_keyboard_remove_modifier(sender->active_modifiers, flag);
  }
}

static axidev_io_result axidev_io_windows_send_vk(WORD vk, bool down) {
  INPUT input;

  memset(&input, 0, sizeof(input));
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = vk;
  input.ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
  input.ki.dwFlags = KEYEVENTF_SCANCODE;
  if (axidev_io_is_windows_extended_key(vk)) {
    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
  }
  if (!down) {
    input.ki.dwFlags |= KEYEVENTF_KEYUP;
  }

  if (SendInput(1, &input, sizeof(input)) == 0) {
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }
  return AXIDEV_IO_RESULT_OK;
}

static axidev_io_result axidev_io_windows_send_unicode(uint32_t codepoint) {
  INPUT inputs[4];
  size_t count = 0;

  memset(inputs, 0, sizeof(inputs));
  if (codepoint <= 0xFFFFu) {
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wScan = (WORD)codepoint;
    inputs[count].ki.dwFlags = KEYEVENTF_UNICODE;
    ++count;
    inputs[count] = inputs[count - 1];
    inputs[count].ki.dwFlags |= KEYEVENTF_KEYUP;
    ++count;
  } else if (codepoint <= 0x10FFFFu) {
    uint32_t value = codepoint - 0x10000u;
    WORD high = (WORD)(0xD800u | (value >> 10));
    WORD low = (WORD)(0xDC00u | (value & 0x3FFu));
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wScan = high;
    inputs[count].ki.dwFlags = KEYEVENTF_UNICODE;
    ++count;
    inputs[count] = inputs[count - 1];
    inputs[count].ki.dwFlags |= KEYEVENTF_KEYUP;
    ++count;
    inputs[count].type = INPUT_KEYBOARD;
    inputs[count].ki.wScan = low;
    inputs[count].ki.dwFlags = KEYEVENTF_UNICODE;
    ++count;
    inputs[count] = inputs[count - 1];
    inputs[count].ki.dwFlags |= KEYEVENTF_KEYUP;
    ++count;
  } else {
    return AXIDEV_IO_RESULT_INVALID_ARGUMENT;
  }

  if (SendInput((UINT)count, inputs, sizeof(INPUT)) == 0) {
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }
  return AXIDEV_IO_RESULT_OK;
}

static axidev_io_result
axidev_io_sender_resolve_mapping(axidev_io_keyboard_key_with_modifier_t request,
                                 int32_t *out_keycode,
                                 axidev_io_keyboard_modifier_t *out_mods,
                                 axidev_io_keyboard_key_t *out_resolved_key) {
  return axidev_io_keymap_resolve_key_request(request, out_keycode, out_mods,
                                              out_resolved_key);
}

static axidev_io_result
axidev_io_sender_send_raw_key(axidev_io_keyboard_key_t key, int32_t keycode,
                              bool down) {
  axidev_io_result result = axidev_io_windows_send_vk((WORD)keycode, down);
  if (result == AXIDEV_IO_RESULT_OK) {
    axidev_io_sender_update_modifier_state(key, down);
  }
  return result;
}

static axidev_io_windows_repeat_entry *
axidev_io_windows_repeat_entries(axidev_io_keyboard_sender_impl *impl) {
  return (axidev_io_windows_repeat_entry *)impl->repeat_entries;
}

static uint64_t axidev_io_windows_monotonic_time_ns(void) {
  return axidev_io_monotonic_time_ms() * 1000000ull;
}

static axidev_io_result
axidev_io_windows_read_repeat_settings(uint64_t *out_delay_ns,
                                       uint64_t *out_interval_ns) {
  UINT delay = 0;
  UINT speed = 0;
  double repeats_per_second;

  if (out_delay_ns == NULL || out_interval_ns == NULL) {
    return AXIDEV_IO_RESULT_INVALID_ARGUMENT;
  }
  if (!SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &delay, 0)) {
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }
  if (!SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &speed, 0)) {
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }

  if (delay > 3u) {
    delay = 3u;
  }
  if (speed > 31u) {
    speed = 31u;
  }

  repeats_per_second = 2.5 + (((double)speed * 27.5) / 31.0);
  *out_delay_ns = (uint64_t)(delay + 1u) * 250000000ull;
  *out_interval_ns = (uint64_t)(1000000000.0 / repeats_per_second);
  if (*out_interval_ns == 0) {
    *out_interval_ns = 1;
  }
  return AXIDEV_IO_RESULT_OK;
}

static size_t axidev_io_windows_repeat_find_request(
    axidev_io_keyboard_sender_impl *impl,
    axidev_io_keyboard_key_with_modifier_t request) {
  axidev_io_windows_repeat_entry *entries =
      axidev_io_windows_repeat_entries(impl);
  size_t i;

  for (i = 0; i < impl->repeat_len; ++i) {
    if (entries[i].request.key == request.key &&
        entries[i].request.mods == request.mods) {
      return i;
    }
  }
  return (size_t)-1;
}

static bool
axidev_io_windows_repeat_reserve(axidev_io_keyboard_sender_impl *impl,
                                 size_t needed) {
  axidev_io_windows_repeat_entry *entries;
  size_t new_cap;

  if (needed <= impl->repeat_cap) {
    return true;
  }

  new_cap = impl->repeat_cap == 0 ? 4u : impl->repeat_cap * 2u;
  while (new_cap < needed) {
    new_cap *= 2u;
  }

  entries = (axidev_io_windows_repeat_entry *)realloc(
      impl->repeat_entries, new_cap * sizeof(*entries));
  if (entries == NULL) {
    return false;
  }

  impl->repeat_entries = entries;
  impl->repeat_cap = new_cap;
  return true;
}

static void
axidev_io_windows_repeat_remove_at(axidev_io_keyboard_sender_impl *impl,
                                   size_t index) {
  axidev_io_windows_repeat_entry *entries =
      axidev_io_windows_repeat_entries(impl);

  if (index >= impl->repeat_len) {
    return;
  }
  if (index + 1u < impl->repeat_len) {
    memmove(&entries[index], &entries[index + 1u],
            (impl->repeat_len - index - 1u) * sizeof(entries[0]));
  }
  --impl->repeat_len;
}

static uint64_t
axidev_io_windows_repeat_next_deadline(uint64_t previous_deadline_ns,
                                       uint64_t interval_ns, uint64_t now_ns) {
  uint64_t missed_intervals;

  if (interval_ns == 0 || previous_deadline_ns > UINT64_MAX - interval_ns) {
    return now_ns + 1u;
  }
  if (previous_deadline_ns + interval_ns > now_ns) {
    return previous_deadline_ns + interval_ns;
  }

  missed_intervals = ((now_ns - previous_deadline_ns) / interval_ns) + 1u;
  if (missed_intervals > (UINT64_MAX - previous_deadline_ns) / interval_ns) {
    return now_ns + interval_ns;
  }
  return previous_deadline_ns + (missed_intervals * interval_ns);
}

static DWORD axidev_io_windows_repeat_timeout_ms(uint64_t now_ns,
                                                 uint64_t deadline_ns) {
  uint64_t remaining_ns;
  uint64_t timeout_ms;

  if (deadline_ns <= now_ns) {
    return 0;
  }
  remaining_ns = deadline_ns - now_ns;
  timeout_ms = (remaining_ns + 999999ull) / 1000000ull;
  if (timeout_ms > (uint64_t)MAXDWORD) {
    return MAXDWORD;
  }
  return (DWORD)timeout_ms;
}

static void axidev_io_windows_repeat_drain_entries(
    axidev_io_keyboard_sender_impl *impl,
    axidev_io_windows_repeat_entry **out_entries, size_t *out_count) {
  axidev_io_windows_repeat_entry *entries = NULL;
  size_t count = 0;

  if (out_entries == NULL || out_count == NULL) {
    return;
  }

  if (impl->repeat_lock_initialized) {
    axidev_io_mutex_lock(&impl->repeat_lock);
    entries = axidev_io_windows_repeat_entries(impl);
    count = impl->repeat_len;
    impl->repeat_entries = NULL;
    impl->repeat_len = 0;
    impl->repeat_cap = 0;
    axidev_io_mutex_unlock(&impl->repeat_lock);
  }

  if (impl->repeat_wake_event != NULL) {
    SetEvent((HANDLE)impl->repeat_wake_event);
  }
  if (impl->repeat_idle_event != NULL) {
    WaitForSingleObject((HANDLE)impl->repeat_idle_event, INFINITE);
  }

  *out_entries = entries;
  *out_count = count;
}

static axidev_io_result axidev_io_windows_release_repeat_entries(
    axidev_io_windows_repeat_entry *entries, size_t count) {
  axidev_io_result final_result = AXIDEV_IO_RESULT_OK;
  size_t i;

  for (i = 0; i < count; ++i) {
    axidev_io_result result = axidev_io_sender_send_raw_key(
        entries[i].resolved_key, entries[i].keycode, false);
    if (result != AXIDEV_IO_RESULT_OK && final_result == AXIDEV_IO_RESULT_OK) {
      final_result = result;
    }

    result =
        axidev_io_keyboard_sender_release_modifier_internal(entries[i].mods);
    if (result != AXIDEV_IO_RESULT_OK && final_result == AXIDEV_IO_RESULT_OK) {
      final_result = result;
    }
  }

  free(entries);
  return final_result;
}

static int axidev_io_windows_repeat_worker_main(void *user_data) {
  axidev_io_keyboard_sender_impl *impl =
      (axidev_io_keyboard_sender_impl *)user_data;
  axidev_io_windows_repeat_send *due = NULL;
  size_t due_cap = 0;

  for (;;) {
    axidev_io_windows_repeat_send *new_due;
    size_t due_count = 0;
    uint64_t now_ns;
    uint64_t next_deadline_ns = UINT64_MAX;
    DWORD timeout_ms;
    size_t i;

    axidev_io_mutex_lock(&impl->repeat_lock);
    if (impl->repeat_stop_worker) {
      axidev_io_mutex_unlock(&impl->repeat_lock);
      break;
    }

    if (impl->repeat_len == 0) {
      axidev_io_mutex_unlock(&impl->repeat_lock);
      WaitForSingleObject((HANDLE)impl->repeat_wake_event, INFINITE);
      continue;
    }

    if (impl->repeat_len > due_cap) {
      size_t needed = impl->repeat_len;
      axidev_io_mutex_unlock(&impl->repeat_lock);

      new_due =
          (axidev_io_windows_repeat_send *)realloc(due, needed * sizeof(*due));
      if (new_due == NULL) {
        AXIDEV_IO_LOG_ERROR("Windows repeat scheduler allocation failed");
        axidev_io_sleep_ms(1);
        continue;
      }
      due = new_due;
      due_cap = needed;
      continue;
    }

    now_ns = axidev_io_windows_monotonic_time_ns();
    for (i = 0; i < impl->repeat_len; ++i) {
      axidev_io_windows_repeat_entry *entry =
          &axidev_io_windows_repeat_entries(impl)[i];

      if (entry->next_fire_at_ns <= now_ns) {
        due[due_count].resolved_key = entry->resolved_key;
        due[due_count].keycode = entry->keycode;
        ++due_count;
        entry->next_fire_at_ns = axidev_io_windows_repeat_next_deadline(
            entry->next_fire_at_ns, entry->interval_ns, now_ns);
      }

      if (entry->next_fire_at_ns < next_deadline_ns) {
        next_deadline_ns = entry->next_fire_at_ns;
      }
    }

    now_ns = axidev_io_windows_monotonic_time_ns();
    timeout_ms = axidev_io_windows_repeat_timeout_ms(now_ns, next_deadline_ns);
    if (due_count != 0) {
      ResetEvent((HANDLE)impl->repeat_idle_event);
    }
    axidev_io_mutex_unlock(&impl->repeat_lock);

    for (i = 0; i < due_count; ++i) {
      axidev_io_result result = axidev_io_sender_send_raw_key(
          due[i].resolved_key, due[i].keycode, true);
      if (result != AXIDEV_IO_RESULT_OK) {
        AXIDEV_IO_LOG_ERROR("Windows repeat SendInput failed: %s",
                            axidev_io_result_to_string(result));
      }
    }

    if (due_count != 0) {
      SetEvent((HANDLE)impl->repeat_idle_event);
    }

    WaitForSingleObject((HANDLE)impl->repeat_wake_event, timeout_ms);
  }

  free(due);
  return 0;
}

static axidev_io_result
axidev_io_windows_repeat_start_state(axidev_io_keyboard_sender_impl *impl) {
  impl->repeat_wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
  if (impl->repeat_wake_event == NULL) {
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }
  impl->repeat_idle_event = CreateEventW(NULL, TRUE, TRUE, NULL);
  if (impl->repeat_idle_event == NULL) {
    CloseHandle((HANDLE)impl->repeat_wake_event);
    impl->repeat_wake_event = NULL;
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }
  if (!axidev_io_mutex_init(&impl->repeat_lock)) {
    CloseHandle((HANDLE)impl->repeat_idle_event);
    impl->repeat_idle_event = NULL;
    CloseHandle((HANDLE)impl->repeat_wake_event);
    impl->repeat_wake_event = NULL;
    return AXIDEV_IO_RESULT_INTERNAL_ERROR;
  }
  impl->repeat_lock_initialized = true;
  impl->repeat_stop_worker = false;
  if (!axidev_io_thread_create(&impl->repeat_worker,
                               axidev_io_windows_repeat_worker_main, impl)) {
    axidev_io_mutex_destroy(&impl->repeat_lock);
    impl->repeat_lock_initialized = false;
    CloseHandle((HANDLE)impl->repeat_idle_event);
    impl->repeat_idle_event = NULL;
    CloseHandle((HANDLE)impl->repeat_wake_event);
    impl->repeat_wake_event = NULL;
    return AXIDEV_IO_RESULT_INTERNAL_ERROR;
  }
  impl->repeat_worker_running = true;
  return AXIDEV_IO_RESULT_OK;
}

static void
axidev_io_windows_repeat_stop_state(axidev_io_keyboard_sender_impl *impl) {
  axidev_io_windows_repeat_entry *entries = NULL;
  size_t count = 0;

  if (impl->repeat_lock_initialized) {
    axidev_io_mutex_lock(&impl->repeat_lock);
    impl->repeat_stop_worker = true;
    entries = axidev_io_windows_repeat_entries(impl);
    count = impl->repeat_len;
    impl->repeat_entries = NULL;
    impl->repeat_len = 0;
    impl->repeat_cap = 0;
    axidev_io_mutex_unlock(&impl->repeat_lock);
  }

  if (impl->repeat_wake_event != NULL) {
    SetEvent((HANDLE)impl->repeat_wake_event);
  }
  if (impl->repeat_worker_running) {
    axidev_io_thread_join(&impl->repeat_worker);
    impl->repeat_worker_running = false;
  }

  axidev_io_windows_release_repeat_entries(entries, count);

  if (impl->repeat_lock_initialized) {
    axidev_io_mutex_destroy(&impl->repeat_lock);
    impl->repeat_lock_initialized = false;
  }
  if (impl->repeat_wake_event != NULL) {
    CloseHandle((HANDLE)impl->repeat_wake_event);
    impl->repeat_wake_event = NULL;
  }
  if (impl->repeat_idle_event != NULL) {
    CloseHandle((HANDLE)impl->repeat_idle_event);
    impl->repeat_idle_event = NULL;
  }
}

axidev_io_result axidev_io_keyboard_sender_initialize(void) {
  axidev_io_keyboard_sender_impl *impl = axidev_io_sender_impl_get();
  axidev_io_keyboard_sender_context *sender;
  axidev_io_result result;

  memset(impl, 0, sizeof(*impl));
  axidev_io_keyboard_reset_public_sender_state();
  sender = axidev_io_sender_public_context();

  impl->layout = GetKeyboardLayout(0);
  result = axidev_io_windows_read_repeat_settings(&sender->repeat_delay_ns,
                                                  &sender->repeat_interval_ns);
  if (result != AXIDEV_IO_RESULT_OK) {
    return result;
  }

  result = axidev_io_windows_repeat_start_state(impl);
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_windows_repeat_stop_state(impl);
    return result;
  }

  sender->initialized = true;
  sender->ready = true;
  sender->capabilities.can_inject_keys = true;
  sender->capabilities.can_inject_text = true;
  sender->capabilities.can_simulate_hid = false;
  sender->capabilities.supports_key_repeat = true;
  sender->capabilities.needs_accessibility_perm = false;
  sender->capabilities.needs_input_monitoring_perm = false;
  sender->capabilities.needs_uinput_access = false;
  axidev_io_global->keyboard.backend_type = AXIDEV_IO_BACKEND_WINDOWS;
  return AXIDEV_IO_RESULT_OK;
}

void axidev_io_keyboard_sender_free(void) {
  axidev_io_keyboard_sender_impl *impl = axidev_io_sender_impl_get();

  axidev_io_windows_repeat_stop_state(impl);
  memset(impl, 0, sizeof(*impl));
  axidev_io_keyboard_reset_public_sender_state();
}

axidev_io_result axidev_io_keyboard_sender_request_permissions(void) {
  return AXIDEV_IO_RESULT_OK;
}

axidev_io_result axidev_io_keyboard_sender_hold_modifier_internal(
    axidev_io_keyboard_modifier_t mods) {
  axidev_io_result result = AXIDEV_IO_RESULT_OK;

  if (axidev_io_keyboard_has_modifier(mods, AXIDEV_IO_MOD_SHIFT)) {
    result = axidev_io_sender_send_raw_key(AXIDEV_IO_KEY_SHIFT_LEFT, VK_LSHIFT,
                                           true);
    if (result != AXIDEV_IO_RESULT_OK) {
      return result;
    }
  }
  if (axidev_io_keyboard_has_modifier(mods, AXIDEV_IO_MOD_CTRL)) {
    result = axidev_io_sender_send_raw_key(AXIDEV_IO_KEY_CTRL_LEFT, VK_LCONTROL,
                                           true);
    if (result != AXIDEV_IO_RESULT_OK) {
      return result;
    }
  }
  if (axidev_io_keyboard_has_modifier(mods, AXIDEV_IO_MOD_ALT)) {
    result =
        axidev_io_sender_send_raw_key(AXIDEV_IO_KEY_ALT_LEFT, VK_LMENU, true);
    if (result != AXIDEV_IO_RESULT_OK) {
      return result;
    }
  }
  if (axidev_io_keyboard_has_modifier(mods, AXIDEV_IO_MOD_SUPER)) {
    result =
        axidev_io_sender_send_raw_key(AXIDEV_IO_KEY_SUPER_LEFT, VK_LWIN, true);
  }
  return result;
}

axidev_io_result axidev_io_keyboard_sender_release_modifier_internal(
    axidev_io_keyboard_modifier_t mods) {
  axidev_io_result result = AXIDEV_IO_RESULT_OK;

  if (axidev_io_keyboard_has_modifier(mods, AXIDEV_IO_MOD_SHIFT)) {
    result = axidev_io_sender_send_raw_key(AXIDEV_IO_KEY_SHIFT_LEFT, VK_LSHIFT,
                                           false);
    if (result != AXIDEV_IO_RESULT_OK) {
      return result;
    }
  }
  if (axidev_io_keyboard_has_modifier(mods, AXIDEV_IO_MOD_CTRL)) {
    result = axidev_io_sender_send_raw_key(AXIDEV_IO_KEY_CTRL_LEFT, VK_LCONTROL,
                                           false);
    if (result != AXIDEV_IO_RESULT_OK) {
      return result;
    }
  }
  if (axidev_io_keyboard_has_modifier(mods, AXIDEV_IO_MOD_ALT)) {
    result =
        axidev_io_sender_send_raw_key(AXIDEV_IO_KEY_ALT_LEFT, VK_LMENU, false);
    if (result != AXIDEV_IO_RESULT_OK) {
      return result;
    }
  }
  if (axidev_io_keyboard_has_modifier(mods, AXIDEV_IO_MOD_SUPER)) {
    result =
        axidev_io_sender_send_raw_key(AXIDEV_IO_KEY_SUPER_LEFT, VK_LWIN, false);
  }
  return result;
}

axidev_io_result
axidev_io_keyboard_sender_release_all_modifiers_internal(void) {
  axidev_io_keyboard_sender_impl *impl = axidev_io_sender_impl_get();
  axidev_io_windows_repeat_entry *entries = NULL;
  axidev_io_result repeat_result;
  axidev_io_result modifier_result;
  size_t count = 0;

  axidev_io_windows_repeat_drain_entries(impl, &entries, &count);
  repeat_result = axidev_io_windows_release_repeat_entries(entries, count);
  modifier_result = axidev_io_keyboard_sender_release_modifier_internal(
      AXIDEV_IO_MOD_SHIFT | AXIDEV_IO_MOD_CTRL | AXIDEV_IO_MOD_ALT |
      AXIDEV_IO_MOD_SUPER);
  return repeat_result != AXIDEV_IO_RESULT_OK ? repeat_result : modifier_result;
}

axidev_io_result axidev_io_keyboard_sender_key_down_internal(
    axidev_io_keyboard_key_with_modifier_t key_mod, bool repeat) {
  axidev_io_keyboard_sender_impl *impl = axidev_io_sender_impl_get();
  int32_t keycode;
  axidev_io_keyboard_modifier_t mods;
  axidev_io_keyboard_key_t resolved_key;
  axidev_io_result result;
  axidev_io_keyboard_sender_context *sender = axidev_io_sender_public_context();

  result =
      axidev_io_sender_resolve_mapping(key_mod, &keycode, &mods, &resolved_key);
  if (result != AXIDEV_IO_RESULT_OK) {
    return result;
  }

  if (!repeat) {
    result = axidev_io_keyboard_sender_hold_modifier_internal(mods);
    if (result != AXIDEV_IO_RESULT_OK) {
      return result;
    }

    return axidev_io_sender_send_raw_key(resolved_key, keycode, true);
  }

  axidev_io_mutex_lock(&impl->repeat_lock);
  if (axidev_io_windows_repeat_find_request(impl, key_mod) != (size_t)-1) {
    axidev_io_mutex_unlock(&impl->repeat_lock);
    return AXIDEV_IO_RESULT_OK;
  }

  result = axidev_io_keyboard_sender_hold_modifier_internal(mods);
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_mutex_unlock(&impl->repeat_lock);
    return result;
  }

  result = axidev_io_sender_send_raw_key(resolved_key, keycode, true);
  if (result == AXIDEV_IO_RESULT_OK &&
      !axidev_io_windows_repeat_reserve(impl, impl->repeat_len + 1u)) {
    result = AXIDEV_IO_RESULT_INTERNAL_ERROR;
  }
  if (result == AXIDEV_IO_RESULT_OK) {
    axidev_io_windows_repeat_entry *entries =
        axidev_io_windows_repeat_entries(impl);
    axidev_io_windows_repeat_entry *entry = &entries[impl->repeat_len++];

    entry->request = key_mod;
    entry->resolved_key = resolved_key;
    entry->keycode = keycode;
    entry->mods = mods;
    entry->next_fire_at_ns =
        axidev_io_windows_monotonic_time_ns() + sender->repeat_delay_ns;
    entry->interval_ns = sender->repeat_interval_ns;
    SetEvent((HANDLE)impl->repeat_wake_event);
  } else {
    axidev_io_sender_send_raw_key(resolved_key, keycode, false);
    axidev_io_keyboard_sender_release_modifier_internal(mods);
  }
  axidev_io_mutex_unlock(&impl->repeat_lock);
  return result;
}

axidev_io_result axidev_io_keyboard_sender_key_up_internal(
    axidev_io_keyboard_key_with_modifier_t key_mod) {
  axidev_io_keyboard_sender_impl *impl = axidev_io_sender_impl_get();
  int32_t keycode;
  axidev_io_keyboard_modifier_t mods;
  axidev_io_keyboard_key_t resolved_key;
  axidev_io_result result;
  size_t repeat_index;
  bool removed_repeat = false;

  result =
      axidev_io_sender_resolve_mapping(key_mod, &keycode, &mods, &resolved_key);
  if (result != AXIDEV_IO_RESULT_OK) {
    return result;
  }

  if (impl->repeat_lock_initialized) {
    axidev_io_mutex_lock(&impl->repeat_lock);
    repeat_index = axidev_io_windows_repeat_find_request(impl, key_mod);
    if (repeat_index != (size_t)-1) {
      axidev_io_windows_repeat_remove_at(impl, repeat_index);
      removed_repeat = true;
      SetEvent((HANDLE)impl->repeat_wake_event);
    }
    axidev_io_mutex_unlock(&impl->repeat_lock);
  }

  if (removed_repeat && impl->repeat_idle_event != NULL) {
    WaitForSingleObject((HANDLE)impl->repeat_idle_event, INFINITE);
  }

  result = axidev_io_sender_send_raw_key(resolved_key, keycode, false);
  if (result != AXIDEV_IO_RESULT_OK) {
    return result;
  }
  return axidev_io_keyboard_sender_release_modifier_internal(mods);
}

axidev_io_result axidev_io_keyboard_sender_tap_internal(
    axidev_io_keyboard_key_with_modifier_t key_mod) {
  int32_t keycode;
  axidev_io_keyboard_modifier_t mods;
  axidev_io_keyboard_key_t resolved_key;
  axidev_io_result result;

  result =
      axidev_io_sender_resolve_mapping(key_mod, &keycode, &mods, &resolved_key);
  if (result != AXIDEV_IO_RESULT_OK) {
    return result;
  }

  result = axidev_io_keyboard_sender_hold_modifier_internal(mods);
  if (result != AXIDEV_IO_RESULT_OK) {
    return result;
  }
  axidev_io_sender_delay();
  result = axidev_io_sender_send_raw_key(resolved_key, keycode, true);
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_keyboard_sender_release_modifier_internal(mods);
    return result;
  }
  axidev_io_sender_delay();
  result = axidev_io_sender_send_raw_key(resolved_key, keycode, false);
  axidev_io_sender_delay();
  if (axidev_io_keyboard_sender_release_modifier_internal(mods) !=
      AXIDEV_IO_RESULT_OK) {
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }
  return result;
}

axidev_io_result
axidev_io_keyboard_sender_type_character_internal(uint32_t codepoint) {
  axidev_io_keyboard_key_with_modifier_t key_mod;

  if (axidev_io_keymap_lookup_character(codepoint, &key_mod) ==
      AXIDEV_IO_RESULT_OK) {
    return axidev_io_keyboard_sender_tap_internal(key_mod);
  }

  return axidev_io_windows_send_unicode(codepoint);
}

void axidev_io_keyboard_sender_flush_internal(void) {}

void axidev_io_keyboard_sender_set_key_delay_internal(uint32_t delay_us) {
  axidev_io_sender_public_context()->key_delay_us = delay_us;
}

size_t axidev_io_windows_sender_repeat_count_for_tests(void) {
  axidev_io_keyboard_sender_impl *impl = axidev_io_sender_impl_get();
  size_t count;

  if (!impl->repeat_lock_initialized) {
    return 0;
  }

  axidev_io_mutex_lock(&impl->repeat_lock);
  count = impl->repeat_len;
  axidev_io_mutex_unlock(&impl->repeat_lock);
  return count;
}

#endif
