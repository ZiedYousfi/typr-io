#include <axidev-io/c_api.h>

#include "test_assert.h"

#if defined(_WIN32)
#include "keyboard/sender/sender_internal.h"
#endif

#if defined(__linux__)
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

#include <stb/stb_ds.h>

#include "internal/context.h"
#include "keyboard/common/keymap_internal.h"
#include "keyboard/common/linux_keysym_internal.h"
#endif

static void noop_listener_cb(uint32_t codepoint,
                             axidev_io_keyboard_key_with_modifier_t key_mod,
                             bool pressed, void *user_data) {
  (void)codepoint;
  (void)key_mod;
  (void)pressed;
  (void)user_data;
}

static void noop_mouse_listener_cb(const axidev_io_mouse_state_t *state,
                                   void *user_data) {
  (void)state;
  (void)user_data;
}

static void test_conversion_helpers(void) {
  char *text;
  axidev_io_keyboard_key_with_modifier_t parsed;

  AXIDEV_IO_LOG_DEBUG("test conversion helper compile check");
  TEST_CHECK(axidev_io_library_version() != NULL);
  TEST_CHECK_EQ_INT(axidev_io_keyboard_string_to_key("A"), AXIDEV_IO_KEY_A);

  text = axidev_io_keyboard_key_to_string(AXIDEV_IO_KEY_A);
  TEST_CHECK_STR(text, "A");
  axidev_io_free_string(text);

  TEST_CHECK(
      axidev_io_keyboard_string_to_key_with_modifier("Shift+A", &parsed));
  TEST_CHECK_EQ_INT(parsed.key, AXIDEV_IO_KEY_A);
  TEST_CHECK((parsed.mods & AXIDEV_IO_MOD_SHIFT) != 0);
}

#if defined(__linux__)
static void test_linux_fr_digit_key_resolution(void) {
  struct xkb_context *xkb_context;
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
  struct xkb_rule_names names;
  axidev_io_linux_keymap linux_keymap;
  ptrdiff_t index;
  int32_t keycode = -1;
  axidev_io_keyboard_modifier_t mods = AXIDEV_IO_MOD_NONE;
  axidev_io_keyboard_key_t resolved_key = AXIDEV_IO_KEY_UNKNOWN;

  axidev_io_context_ensure_runtime();

  memset(&names, 0, sizeof(names));
  memset(&linux_keymap, 0, sizeof(linux_keymap));
  names.layout = "fr";

  xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  TEST_CHECK(xkb_context != NULL);
  if (xkb_context == NULL) {
    return;
  }

  xkb_keymap = xkb_keymap_new_from_names(xkb_context, &names,
                                         XKB_KEYMAP_COMPILE_NO_FLAGS);
  TEST_CHECK(xkb_keymap != NULL);
  if (xkb_keymap == NULL) {
    xkb_context_unref(xkb_context);
    return;
  }

  xkb_state = xkb_state_new(xkb_keymap);
  TEST_CHECK(xkb_state != NULL);
  if (xkb_state == NULL) {
    xkb_keymap_unref(xkb_keymap);
    xkb_context_unref(xkb_context);
    return;
  }

  axidev_io_linux_keymap_init(&linux_keymap, xkb_keymap, xkb_state);

  index = hmgeti(linux_keymap.char_to_keycode, (uint32_t)'1');
  TEST_CHECK(index >= 0);
  if (index >= 0) {
    TEST_CHECK_EQ_INT(linux_keymap.char_to_keycode[index].value.keycode, KEY_1);
    TEST_CHECK((linux_keymap.char_to_keycode[index].value.required_mods &
                AXIDEV_IO_MOD_SHIFT) != 0);
    TEST_CHECK_EQ_INT(linux_keymap.char_to_keycode[index].value.produced_key,
                      AXIDEV_IO_KEY_NUM1);
  }

  TEST_CHECK_EQ_INT(axidev_io_linux_resolve_key_from_evdev_and_mods(
                        &linux_keymap, KEY_1, AXIDEV_IO_MOD_SHIFT),
                    AXIDEV_IO_KEY_NUM1);

  axidev_io_keyboard_keymap_free();
  for (index = 0; index < hmlen(linux_keymap.char_to_keycode); ++index) {
    hmput(axidev_io_keymap_impl_get()->char_to_mapping,
          linux_keymap.char_to_keycode[index].key,
          linux_keymap.char_to_keycode[index].value);
  }
  for (index = 0; index < hmlen(linux_keymap.evdev_to_key); ++index) {
    hmput(axidev_io_keymap_impl_get()->code_to_key,
          linux_keymap.evdev_to_key[index].key,
          linux_keymap.evdev_to_key[index].value);
  }
  for (index = 0; index < hmlen(linux_keymap.code_and_mods_to_key); ++index) {
    hmput(axidev_io_keymap_impl_get()->code_and_mods_to_key,
          linux_keymap.code_and_mods_to_key[index].key,
          linux_keymap.code_and_mods_to_key[index].value);
  }
  for (index = 0; index < hmlen(linux_keymap.key_to_evdev); ++index) {
    hmput(axidev_io_keymap_impl_get()->key_to_code,
          linux_keymap.key_to_evdev[index].key,
          linux_keymap.key_to_evdev[index].value);
  }
  axidev_io_keymap_public_context()->initialized = true;

  TEST_CHECK_EQ_INT(axidev_io_keymap_resolve_key_request(
                        (axidev_io_keyboard_key_with_modifier_t){
                            AXIDEV_IO_KEY_NUM1, AXIDEV_IO_MOD_NONE},
                        &keycode, &mods, &resolved_key),
                    AXIDEV_IO_RESULT_OK);
  TEST_CHECK_EQ_INT(keycode, KEY_1);
  TEST_CHECK((mods & AXIDEV_IO_MOD_SHIFT) != 0);
  TEST_CHECK_EQ_INT(resolved_key, AXIDEV_IO_KEY_NUM1);

  axidev_io_keyboard_keymap_free();
  axidev_io_linux_keymap_free(&linux_keymap);
  xkb_state_unref(xkb_state);
  xkb_keymap_unref(xkb_keymap);
  xkb_context_unref(xkb_context);
}
#endif

static void test_sender_lifecycle_and_errors(void) {
  char *error_text;
  axidev_io_keyboard_capabilities_t capabilities;
  bool initialized;

  TEST_CHECK(!axidev_io_keyboard_tap((axidev_io_keyboard_key_with_modifier_t){
      AXIDEV_IO_KEY_A, AXIDEV_IO_MOD_NONE}));
  error_text = axidev_io_get_last_error();
  TEST_CHECK(error_text != NULL);
  axidev_io_free_string(error_text);

  initialized = axidev_io_keyboard_initialize();
#if defined(__linux__)
  if (!initialized) {
    error_text = axidev_io_get_last_error();
    TEST_CHECK(error_text != NULL);
    TEST_CHECK(error_text != NULL &&
               strstr(error_text, "permission_denied") != NULL);
    if (error_text != NULL) {
      axidev_io_free_string(error_text);
    }
    TEST_CHECK(!axidev_io_keyboard_is_ready());
    axidev_io_keyboard_get_capabilities(&capabilities);
    TEST_CHECK(!capabilities.can_inject_keys);
    axidev_io_keyboard_free();
    return;
  }
#endif
  TEST_CHECK(initialized);
  TEST_CHECK(axidev_io_keyboard_is_ready());
  axidev_io_keyboard_get_capabilities(&capabilities);
  TEST_CHECK(capabilities.can_inject_keys);
#if defined(_WIN32)
  TEST_CHECK(capabilities.supports_key_repeat);
  TEST_CHECK(!capabilities.can_simulate_hid);
#endif

  axidev_io_keyboard_set_key_delay(1000);
  TEST_CHECK(axidev_io_keyboard_release_all_modifiers());
  axidev_io_keyboard_flush();
  axidev_io_keyboard_free();
  TEST_CHECK(!axidev_io_keyboard_is_ready());
}

#if defined(_WIN32)
static void test_windows_repeat_state(void) {
  TEST_CHECK(axidev_io_keyboard_initialize());
  TEST_CHECK_EQ_INT((int)axidev_io_windows_sender_repeat_count_for_tests(), 0);

  TEST_CHECK(axidev_io_keyboard_key_down(
      (axidev_io_keyboard_key_with_modifier_t){AXIDEV_IO_KEY_A,
                                               AXIDEV_IO_MOD_NONE},
      false));
  TEST_CHECK_EQ_INT((int)axidev_io_windows_sender_repeat_count_for_tests(), 0);
  TEST_CHECK(axidev_io_keyboard_key_up((axidev_io_keyboard_key_with_modifier_t){
      AXIDEV_IO_KEY_A, AXIDEV_IO_MOD_NONE}));

  TEST_CHECK(axidev_io_keyboard_key_down(
      (axidev_io_keyboard_key_with_modifier_t){AXIDEV_IO_KEY_A,
                                               AXIDEV_IO_MOD_NONE},
      true));
  TEST_CHECK_EQ_INT((int)axidev_io_windows_sender_repeat_count_for_tests(), 1);
  TEST_CHECK(axidev_io_keyboard_key_down(
      (axidev_io_keyboard_key_with_modifier_t){AXIDEV_IO_KEY_A,
                                               AXIDEV_IO_MOD_NONE},
      true));
  TEST_CHECK_EQ_INT((int)axidev_io_windows_sender_repeat_count_for_tests(), 1);
  TEST_CHECK(axidev_io_keyboard_key_up((axidev_io_keyboard_key_with_modifier_t){
      AXIDEV_IO_KEY_A, AXIDEV_IO_MOD_NONE}));
  TEST_CHECK_EQ_INT((int)axidev_io_windows_sender_repeat_count_for_tests(), 0);

  TEST_CHECK(axidev_io_keyboard_key_down(
      (axidev_io_keyboard_key_with_modifier_t){AXIDEV_IO_KEY_SHIFT_LEFT,
                                               AXIDEV_IO_MOD_NONE},
      true));
  TEST_CHECK_EQ_INT((int)axidev_io_windows_sender_repeat_count_for_tests(), 0);
  TEST_CHECK(axidev_io_keyboard_key_up((axidev_io_keyboard_key_with_modifier_t){
      AXIDEV_IO_KEY_SHIFT_LEFT, AXIDEV_IO_MOD_NONE}));

  TEST_CHECK(axidev_io_keyboard_key_down(
      (axidev_io_keyboard_key_with_modifier_t){AXIDEV_IO_KEY_A,
                                               AXIDEV_IO_MOD_SHIFT},
      true));
  TEST_CHECK_EQ_INT((int)axidev_io_windows_sender_repeat_count_for_tests(), 1);
  TEST_CHECK(axidev_io_keyboard_release_all_modifiers());
  TEST_CHECK_EQ_INT((int)axidev_io_windows_sender_repeat_count_for_tests(), 0);

  TEST_CHECK(axidev_io_keyboard_key_down(
      (axidev_io_keyboard_key_with_modifier_t){AXIDEV_IO_KEY_B,
                                               AXIDEV_IO_MOD_NONE},
      true));
  TEST_CHECK_EQ_INT((int)axidev_io_windows_sender_repeat_count_for_tests(), 1);
  axidev_io_keyboard_free();
  TEST_CHECK_EQ_INT((int)axidev_io_windows_sender_repeat_count_for_tests(), 0);
}
#endif

static void test_listener_lifecycle(void) {
  char *error_text;
  bool started;

  TEST_CHECK(!axidev_io_listener_start(NULL, NULL));
  error_text = axidev_io_get_last_error();
  TEST_CHECK(error_text != NULL);
  axidev_io_free_string(error_text);

  started = axidev_io_listener_start(noop_listener_cb, NULL);
  if (started) {
    TEST_CHECK(axidev_io_listener_is_listening());
    axidev_io_listener_stop();
    TEST_CHECK(!axidev_io_listener_is_listening());
  } else {
    error_text = axidev_io_get_last_error();
    if (error_text != NULL) {
      axidev_io_free_string(error_text);
    }
  }
}

static void test_mouse_api_lifecycle(void) {
  axidev_io_mouse_state_t state;
  char *error_text;
  bool started;

  TEST_CHECK(!axidev_io_mouse_poll(NULL));
  error_text = axidev_io_get_last_error();
  TEST_CHECK(error_text != NULL);
  axidev_io_free_string(error_text);

  memset(&state, 0, sizeof(state));
  TEST_CHECK(axidev_io_mouse_poll(&state));
  TEST_CHECK(state.timestamp_ms != 0);

  TEST_CHECK(!axidev_io_mouse_listener_start(NULL, NULL));
  error_text = axidev_io_get_last_error();
  TEST_CHECK(error_text != NULL);
  axidev_io_free_string(error_text);

  started = axidev_io_mouse_listener_start(noop_mouse_listener_cb, NULL);
  if (started) {
    TEST_CHECK(axidev_io_mouse_listener_is_listening());
    axidev_io_mouse_listener_stop();
    TEST_CHECK(!axidev_io_mouse_listener_is_listening());
  } else {
    error_text = axidev_io_get_last_error();
    if (error_text != NULL) {
      axidev_io_free_string(error_text);
    }
  }
}

int main(void) {
  TEST_RUN(test_conversion_helpers);
#if defined(__linux__)
  TEST_RUN(test_linux_fr_digit_key_resolution);
#endif
  TEST_RUN(test_sender_lifecycle_and_errors);
#if defined(_WIN32)
  TEST_RUN(test_windows_repeat_state);
#endif
  TEST_RUN(test_listener_lifecycle);
  TEST_RUN(test_mouse_api_lifecycle);
  return g_axidev_test_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
