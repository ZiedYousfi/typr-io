#include <axidev-io/c_api.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal/context.h"
#include "internal/utf.h"
#include "keyboard/common/key_utils_internal.h"
#include "keyboard/common/keymap_internal.h"
#include "keyboard/listener/listener_internal.h"
#include "keyboard/sender/sender_internal.h"
#include "mouse/mouse_internal.h"

static void axidev_io_report_result(const char *function_name,
                                    axidev_io_result result) {
  char *existing_error;

  if (result == AXIDEV_IO_RESULT_OK) {
    return;
  }

  existing_error = axidev_io_get_last_error();
  if (existing_error == NULL || existing_error[0] == '\0') {
    axidev_io_set_last_error_result(function_name, result);
    AXIDEV_IO_LOG_ERROR("%s failed: %s", function_name,
                        axidev_io_result_to_string(result));
  } else {
    AXIDEV_IO_LOG_ERROR("%s failed: %s", function_name, existing_error);
  }
  axidev_io_free_string(existing_error);
}

static const char *axidev_io_log_level_name(axidev_io_log_level_t level) {
  switch (level) {
  case AXIDEV_IO_LOG_LEVEL_DEBUG:
    return "DEBUG";
  case AXIDEV_IO_LOG_LEVEL_INFO:
    return "INFO";
  case AXIDEV_IO_LOG_LEVEL_WARN:
    return "WARN";
  case AXIDEV_IO_LOG_LEVEL_ERROR:
  default:
    return "ERROR";
  }
}

static void axidev_io_log_non_error_threshold_notice(void) {
  axidev_io_log_level_t level = axidev_io_global->log_level;

  if ((int)level >= (int)AXIDEV_IO_LOG_LEVEL_ERROR) {
    return;
  }

  axidev_io_log_message(
      level, __FILE__, __LINE__,
      "logging level is %s; set AXIDEV_IO_LOG_LEVEL_ERROR explicitly if you "
      "want only bare-minimum error output",
      axidev_io_log_level_name(level));
}

static axidev_io_result axidev_io_require_keyboard_initialized(void) {
  if (!axidev_io_global->keyboard.initialized ||
      !axidev_io_global->keyboard.sender.initialized ||
      !axidev_io_global->keyboard.keymap.initialized) {
    return AXIDEV_IO_RESULT_NOT_INITIALIZED;
  }
  return AXIDEV_IO_RESULT_OK;
}

static bool
axidev_io_try_consume_modifier_prefix(const char **cursor,
                                      axidev_io_keyboard_modifier_t *mods) {
  static const struct {
    const char *prefix;
    axidev_io_keyboard_modifier_t flag;
  } prefixes[] = {
      {"super+", AXIDEV_IO_MOD_SUPER},  {"super-", AXIDEV_IO_MOD_SUPER},
      {"cmd+", AXIDEV_IO_MOD_SUPER},    {"cmd-", AXIDEV_IO_MOD_SUPER},
      {"win+", AXIDEV_IO_MOD_SUPER},    {"win-", AXIDEV_IO_MOD_SUPER},
      {"meta+", AXIDEV_IO_MOD_SUPER},   {"meta-", AXIDEV_IO_MOD_SUPER},
      {"ctrl+", AXIDEV_IO_MOD_CTRL},    {"ctrl-", AXIDEV_IO_MOD_CTRL},
      {"control+", AXIDEV_IO_MOD_CTRL}, {"control-", AXIDEV_IO_MOD_CTRL},
      {"alt+", AXIDEV_IO_MOD_ALT},      {"alt-", AXIDEV_IO_MOD_ALT},
      {"opt+", AXIDEV_IO_MOD_ALT},      {"opt-", AXIDEV_IO_MOD_ALT},
      {"option+", AXIDEV_IO_MOD_ALT},   {"option-", AXIDEV_IO_MOD_ALT},
      {"shift+", AXIDEV_IO_MOD_SHIFT},  {"shift-", AXIDEV_IO_MOD_SHIFT}};
  size_t i;

  if (cursor == NULL || *cursor == NULL || mods == NULL) {
    return false;
  }

  for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
    size_t length = strlen(prefixes[i].prefix);
    size_t j;
    bool matched = true;
    for (j = 0; j < length; ++j) {
      char actual = (*cursor)[j];
      if (actual == '\0') {
        matched = false;
        break;
      }
      if ((char)tolower((unsigned char)actual) != prefixes[i].prefix[j]) {
        matched = false;
        break;
      }
    }
    if (matched) {
      *mods = (axidev_io_keyboard_modifier_t)(*mods | prefixes[i].flag);
      *cursor += length;
      return true;
    }
  }

  return false;
}

static axidev_io_result
axidev_io_keyboard_type_text_internal(const char *text) {
  const char *cursor = text;

  while (cursor != NULL && *cursor != '\0') {
    axidev_io_keyboard_modifier_t latched_mods = AXIDEV_IO_MOD_NONE;

    while (axidev_io_try_consume_modifier_prefix(&cursor, &latched_mods)) {
    }

    while (*cursor != '\0' && *cursor != ',') {
      const char *previous = cursor;
      uint32_t codepoint = 0;
      axidev_io_result result;

      axidev_io_utf8_decode_one(&cursor, &codepoint);
      if (cursor == previous) {
        return AXIDEV_IO_RESULT_INVALID_ARGUMENT;
      }

      if (latched_mods != AXIDEV_IO_MOD_NONE) {
        axidev_io_keyboard_key_with_modifier_t key_mod;
        result = axidev_io_keymap_lookup_character(codepoint, &key_mod);
        if (result != AXIDEV_IO_RESULT_OK) {
          return result;
        }
        key_mod.mods =
            (axidev_io_keyboard_modifier_t)(key_mod.mods | latched_mods);
        result = axidev_io_keyboard_sender_tap_internal(key_mod);
      } else {
        result = axidev_io_keyboard_sender_type_character_internal(codepoint);
      }

      if (result != AXIDEV_IO_RESULT_OK) {
        return result;
      }
    }

    if (*cursor == ',') {
      ++cursor;
    }
  }

  return AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API bool axidev_io_keyboard_initialize(void) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();

  axidev_io_context_lock();
  axidev_io_keyboard_sender_free();
  axidev_io_keyboard_keymap_free();

  result = axidev_io_keyboard_keymap_initialize();
  if (result == AXIDEV_IO_RESULT_OK) {
    result = axidev_io_keyboard_sender_initialize();
  }

  if (result == AXIDEV_IO_RESULT_OK) {
    axidev_io_global->keyboard.initialized = true;
    axidev_io_log_non_error_threshold_notice();
  } else {
    axidev_io_global->keyboard.initialized = false;
    axidev_io_keyboard_sender_free();
    axidev_io_keyboard_keymap_free();
    axidev_io_report_result("axidev_io_keyboard_initialize", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API void axidev_io_keyboard_free(void) {
  axidev_io_context_ensure_runtime();
  axidev_io_context_lock();
  axidev_io_keyboard_sender_free();
  axidev_io_keyboard_keymap_free();
  axidev_io_global->keyboard.initialized = false;
  if (!axidev_io_global->keyboard.listener.initialized) {
    axidev_io_global->keyboard.backend_type = AXIDEV_IO_BACKEND_UNKNOWN;
  }
  axidev_io_context_unlock();
}

AXIDEV_IO_API bool axidev_io_keyboard_is_ready(void) {
  axidev_io_context_ensure_runtime();
  return axidev_io_global->keyboard.initialized &&
         axidev_io_global->keyboard.sender.ready;
}

AXIDEV_IO_API axidev_io_keyboard_backend_type_t axidev_io_keyboard_type(void) {
  axidev_io_context_ensure_runtime();
  return axidev_io_global->keyboard.backend_type;
}

AXIDEV_IO_API void axidev_io_keyboard_get_capabilities(
    axidev_io_keyboard_capabilities_t *out_capabilities) {
  axidev_io_context_ensure_runtime();
  if (out_capabilities == NULL) {
    axidev_io_report_result("axidev_io_keyboard_get_capabilities",
                            AXIDEV_IO_RESULT_INVALID_ARGUMENT);
    return;
  }
  *out_capabilities = axidev_io_global->keyboard.sender.capabilities;
}

AXIDEV_IO_API bool axidev_io_keyboard_request_permissions(void) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  axidev_io_context_lock();
  result = axidev_io_require_keyboard_initialized();
  if (result == AXIDEV_IO_RESULT_OK) {
    result = axidev_io_keyboard_sender_request_permissions();
  }
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_keyboard_request_permissions", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API bool
axidev_io_keyboard_key_down(axidev_io_keyboard_key_with_modifier_t key_mod,
                            bool repeat) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  axidev_io_context_lock();
  result = axidev_io_require_keyboard_initialized();
  if (result == AXIDEV_IO_RESULT_OK) {
    result = axidev_io_keyboard_sender_key_down_internal(key_mod, repeat);
  }
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_keyboard_key_down", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API bool
axidev_io_keyboard_key_up(axidev_io_keyboard_key_with_modifier_t key_mod) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  axidev_io_context_lock();
  result = axidev_io_require_keyboard_initialized();
  if (result == AXIDEV_IO_RESULT_OK) {
    result = axidev_io_keyboard_sender_key_up_internal(key_mod);
  }
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_keyboard_key_up", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API bool
axidev_io_keyboard_tap(axidev_io_keyboard_key_with_modifier_t key_mod) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  axidev_io_context_lock();
  result = axidev_io_require_keyboard_initialized();
  if (result == AXIDEV_IO_RESULT_OK) {
    result = axidev_io_keyboard_sender_tap_internal(key_mod);
  }
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_keyboard_tap", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API axidev_io_keyboard_modifier_t
axidev_io_keyboard_active_modifiers(void) {
  axidev_io_context_ensure_runtime();
  return axidev_io_global->keyboard.sender.active_modifiers;
}

AXIDEV_IO_API bool
axidev_io_keyboard_hold_modifier(axidev_io_keyboard_modifier_t mods) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  axidev_io_context_lock();
  result = axidev_io_require_keyboard_initialized();
  if (result == AXIDEV_IO_RESULT_OK) {
    result = axidev_io_keyboard_sender_hold_modifier_internal(mods);
  }
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_keyboard_hold_modifier", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API bool
axidev_io_keyboard_release_modifier(axidev_io_keyboard_modifier_t mods) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  axidev_io_context_lock();
  result = axidev_io_require_keyboard_initialized();
  if (result == AXIDEV_IO_RESULT_OK) {
    result = axidev_io_keyboard_sender_release_modifier_internal(mods);
  }
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_keyboard_release_modifier", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API bool axidev_io_keyboard_release_all_modifiers(void) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  axidev_io_context_lock();
  result = axidev_io_require_keyboard_initialized();
  if (result == AXIDEV_IO_RESULT_OK) {
    result = axidev_io_keyboard_sender_release_all_modifiers_internal();
  }
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_keyboard_release_all_modifiers", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API bool axidev_io_keyboard_type_text(const char *text) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  if (text == NULL) {
    axidev_io_report_result("axidev_io_keyboard_type_text",
                            AXIDEV_IO_RESULT_INVALID_ARGUMENT);
    return false;
  }

  axidev_io_context_lock();
  result = axidev_io_require_keyboard_initialized();
  if (result == AXIDEV_IO_RESULT_OK) {
    result = axidev_io_keyboard_type_text_internal(text);
  }
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_keyboard_type_text", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API bool axidev_io_keyboard_type_character(uint32_t codepoint) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  axidev_io_context_lock();
  result = axidev_io_require_keyboard_initialized();
  if (result == AXIDEV_IO_RESULT_OK) {
    result = axidev_io_keyboard_sender_type_character_internal(codepoint);
  }
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_keyboard_type_character", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API void axidev_io_keyboard_flush(void) {
  axidev_io_context_ensure_runtime();
  axidev_io_context_lock();
  if (axidev_io_require_keyboard_initialized() == AXIDEV_IO_RESULT_OK) {
    axidev_io_keyboard_sender_flush_internal();
  }
  axidev_io_context_unlock();
}

AXIDEV_IO_API void axidev_io_keyboard_set_key_delay(uint32_t delay_us) {
  axidev_io_context_ensure_runtime();
  axidev_io_context_lock();
  if (axidev_io_require_keyboard_initialized() == AXIDEV_IO_RESULT_OK) {
    axidev_io_keyboard_sender_set_key_delay_internal(delay_us);
  }
  axidev_io_context_unlock();
}

AXIDEV_IO_API bool axidev_io_listener_start(axidev_io_keyboard_listener_cb cb,
                                            void *user_data) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  axidev_io_context_lock();
  result = axidev_io_keyboard_listener_start_internal(cb, user_data);
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_listener_start", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API void axidev_io_listener_stop(void) {
  axidev_io_context_ensure_runtime();
  axidev_io_context_lock();
  axidev_io_keyboard_listener_stop_internal();
  axidev_io_context_unlock();
}

AXIDEV_IO_API bool axidev_io_listener_is_listening(void) {
  axidev_io_context_ensure_runtime();
  return axidev_io_global->keyboard.listener.is_listening;
}

AXIDEV_IO_API bool axidev_io_mouse_poll(axidev_io_mouse_state_t *out_state) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  if (out_state == NULL) {
    axidev_io_report_result("axidev_io_mouse_poll",
                            AXIDEV_IO_RESULT_INVALID_ARGUMENT);
    return false;
  }

  axidev_io_context_lock();
  result = axidev_io_mouse_poll_internal(out_state);
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_mouse_poll", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API bool
axidev_io_mouse_listener_start(axidev_io_mouse_listener_cb cb,
                               void *user_data) {
  axidev_io_result result;

  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  axidev_io_context_lock();
  result = axidev_io_mouse_listener_start_internal(cb, user_data);
  if (result != AXIDEV_IO_RESULT_OK) {
    axidev_io_report_result("axidev_io_mouse_listener_start", result);
  }
  axidev_io_context_unlock();
  return result == AXIDEV_IO_RESULT_OK;
}

AXIDEV_IO_API void axidev_io_mouse_listener_stop(void) {
  axidev_io_context_ensure_runtime();
  axidev_io_context_lock();
  axidev_io_mouse_listener_stop_internal();
  axidev_io_context_unlock();
}

AXIDEV_IO_API bool axidev_io_mouse_listener_is_listening(void) {
  axidev_io_context_ensure_runtime();
  return axidev_io_global->mouse.is_listening;
}

AXIDEV_IO_API char *
axidev_io_keyboard_key_to_string(axidev_io_keyboard_key_t key) {
  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  return axidev_io_key_to_string_alloc(key);
}

AXIDEV_IO_API axidev_io_keyboard_key_t
axidev_io_keyboard_string_to_key(const char *name) {
  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  if (name == NULL) {
    axidev_io_report_result("axidev_io_keyboard_string_to_key",
                            AXIDEV_IO_RESULT_INVALID_ARGUMENT);
    return AXIDEV_IO_KEY_UNKNOWN;
  }
  return axidev_io_string_to_key_internal(name);
}

AXIDEV_IO_API char *axidev_io_keyboard_key_to_string_with_modifier(
    axidev_io_keyboard_key_with_modifier_t key_mod) {
  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  return axidev_io_key_to_string_with_modifier_alloc(key_mod.key, key_mod.mods);
}

AXIDEV_IO_API bool axidev_io_keyboard_string_to_key_with_modifier(
    const char *combo, axidev_io_keyboard_key_with_modifier_t *out_key_mod) {
  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
  if (combo == NULL || out_key_mod == NULL) {
    axidev_io_report_result("axidev_io_keyboard_string_to_key_with_modifier",
                            AXIDEV_IO_RESULT_INVALID_ARGUMENT);
    return false;
  }
  return axidev_io_string_to_key_with_modifier_internal(combo, out_key_mod);
}

AXIDEV_IO_API const char *axidev_io_library_version(void) {
  return AXIDEV_IO_VERSION;
}

AXIDEV_IO_API char *axidev_io_get_last_error(void) {
  axidev_io_private_runtime *runtime;
  char *copy;

  axidev_io_context_ensure_runtime();
  runtime = axidev_io_private_runtime_get();
  axidev_io_mutex_lock(&runtime->error_lock);
  copy = axidev_io_duplicate_string(axidev_io_global->last_error);
  axidev_io_mutex_unlock(&runtime->error_lock);
  return copy;
}

AXIDEV_IO_API void axidev_io_clear_last_error(void) {
  axidev_io_context_ensure_runtime();
  axidev_io_clear_last_error_internal();
}

AXIDEV_IO_API void axidev_io_free_string(char *s) { free(s); }
