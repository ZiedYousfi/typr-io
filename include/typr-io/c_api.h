/*
 * typr-io - C API (c_api.h)
 *
 * Public C-compatible wrapper for the core typr-io functionality. This header
 * provides a minimal, stable C ABI so you can create language bindings for
 * other runtimes.
 *
 * Notes:
 *  - The C API is thin: it exposes opaque sender/listener handles and a small
 *    set of convenience helpers to operate them.
 *  - All functions use the calling convention / symbol visibility used by the
 *    C++ library. The `TYPR_IO_API` macro is applied to exported symbols. When
 *    included from C++ the header will reuse the definition from
 *    <typr-io/core.hpp>. When included from C, a simple fallback is provided.
 *
 * Threading / callbacks:
 *  - Listener callbacks may be invoked on an internal background thread. Your
 *    callback must be thread-safe and should avoid long/blocking operations.
 *
 * Memory ownership:
 *  - Functions that return strings always allocate memory which you must free
 *    with `typr_io_free_string`.
 *
 * Usage example (C):
 *   #include <typr-io/c_api.h>
 *
 *   int main(void) {
 *     typr_io_sender_t s = typr_io_sender_create();
 *     if (!s) return 1;
 *
 *     typr_io_capabilities_t caps;
 *     typr_io_sender_get_capabilities(s, &caps);
 *     if (caps.can_inject_keys) {
 *       typr_io_sender_tap(s, typr_io_string_to_key("A"));
 *     }
 *
 *     typr_io_sender_destroy(s);
 *     return 0;
 *   }
 */

#pragma once
#ifndef TYPR_IO_C_API_H
#define TYPR_IO_C_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Ensure the export macro exists. Consumers building/using the C++ library
   will normally get `TYPR_IO_API` defined by the installed headers or build
   system. When it's not defined, provide a safe no-op fallback. */
#ifndef TYPR_IO_API
#define TYPR_IO_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle types */
typedef void *typr_io_sender_t;
typedef void *typr_io_listener_t;

/* Key and modifier primitive types (match the C++ API layout) */
typedef uint16_t typr_io_key_t; /* corresponds to typr::io::Key */
typedef uint8_t
    typr_io_modifier_t; /* bitmask, corresponds to typr::io::Modifier */

/* Common modifier bit masks */
enum {
  TYPR_IO_MOD_SHIFT = 0x01,
  TYPR_IO_MOD_CTRL = 0x02,
  TYPR_IO_MOD_ALT = 0x04,
  TYPR_IO_MOD_SUPER = 0x08,
  TYPR_IO_MOD_CAPSLOCK = 0x10,
  TYPR_IO_MOD_NUMLOCK = 0x20,
};

/* Capabilities struct mirrors typr::io::Capabilities */
typedef struct typr_io_capabilities_t {
  bool can_inject_keys;
  bool can_inject_text;
  bool can_simulate_hid;
  bool supports_key_repeat;
  bool needs_accessibility_perm;
  bool needs_input_monitoring_perm;
  bool needs_uinput_access;
} typr_io_capabilities_t;

/* Listener callback signature:
 *  - codepoint: Unicode codepoint (0 if none).
 *  - key: logical key id (typr_io_key_t; Key::Unknown if unknown).
 *  - mods: current modifier bitmask (typr_io_modifier_t).
 *  - pressed: true for press, false for release.
 *  - user_data: opaque pointer supplied when starting the listener.
 */
typedef void (*typr_io_listener_cb)(uint32_t codepoint, typr_io_key_t key,
                                    typr_io_modifier_t mods, bool pressed,
                                    void *user_data);

/* ---------------- Sender (input injection) ---------------- */

/* Create/destroy a Sender. Returns NULL on allocation failure. */
TYPR_IO_API typr_io_sender_t typr_io_sender_create(void);
TYPR_IO_API void typr_io_sender_destroy(typr_io_sender_t sender);

/* Introspection */
TYPR_IO_API bool typr_io_sender_is_ready(typr_io_sender_t sender);
TYPR_IO_API uint8_t
typr_io_sender_type(typr_io_sender_t sender); /* BackendType as integer */
TYPR_IO_API void
typr_io_sender_get_capabilities(typr_io_sender_t sender,
                                typr_io_capabilities_t *out_capabilities);

/* Permissions (where applicable). Returns true if backend is ready after call.
 */
TYPR_IO_API bool typr_io_sender_request_permissions(typr_io_sender_t sender);

/* Key events */
TYPR_IO_API bool typr_io_sender_key_down(typr_io_sender_t sender,
                                         typr_io_key_t key);
TYPR_IO_API bool typr_io_sender_key_up(typr_io_sender_t sender,
                                       typr_io_key_t key);
TYPR_IO_API bool typr_io_sender_tap(typr_io_sender_t sender, typr_io_key_t key);

/* Modifier helpers */
TYPR_IO_API typr_io_modifier_t
typr_io_sender_active_modifiers(typr_io_sender_t sender);
TYPR_IO_API bool typr_io_sender_hold_modifier(typr_io_sender_t sender,
                                              typr_io_modifier_t mods);
TYPR_IO_API bool typr_io_sender_release_modifier(typr_io_sender_t sender,
                                                 typr_io_modifier_t mods);
TYPR_IO_API bool typr_io_sender_release_all_modifiers(typr_io_sender_t sender);
TYPR_IO_API bool typr_io_sender_combo(typr_io_sender_t sender,
                                      typr_io_modifier_t mods,
                                      typr_io_key_t key);

/* Text / character injection (UTF-8 string or single Unicode codepoint) */
TYPR_IO_API bool typr_io_sender_type_text_utf8(typr_io_sender_t sender,
                                               const char *utf8_text);
TYPR_IO_API bool typr_io_sender_type_character(typr_io_sender_t sender,
                                               uint32_t codepoint);

/* Misc */
TYPR_IO_API void typr_io_sender_flush(typr_io_sender_t sender);
TYPR_IO_API void typr_io_sender_set_key_delay(typr_io_sender_t sender,
                                              uint32_t delay_us);

/* ---------------- Listener (global event monitoring) ---------------- */

/* Create/destroy a Listener. Returns NULL on allocation failure. */
TYPR_IO_API typr_io_listener_t typr_io_listener_create(void);
TYPR_IO_API void typr_io_listener_destroy(typr_io_listener_t listener);

/* Start listening; callback may be invoked from an internal thread. Returns
 * true on success. The provided callback pointer and user_data are stored and
 * used until the listener is stopped or destroyed. */
TYPR_IO_API bool typr_io_listener_start(typr_io_listener_t listener,
                                        typr_io_listener_cb cb,
                                        void *user_data);

/* Stop listening (safe to call from any thread). */
TYPR_IO_API void typr_io_listener_stop(typr_io_listener_t listener);
TYPR_IO_API bool typr_io_listener_is_listening(typr_io_listener_t listener);

/* ---------------- Utilities / Conversions ---------------- */

/* Convert a Key to a heap-allocated string (caller must free with
 * typr_io_free_string). Returns NULL on allocation failure. */
TYPR_IO_API char *typr_io_key_to_string(typr_io_key_t key);

/* Convert a textual key name (case-insensitive, accepts aliases like "esc",
 * "space") to a typr_io_key_t value. Returns 0 (Key::Unknown) for
 * unknown/invalid inputs. */
TYPR_IO_API typr_io_key_t typr_io_string_to_key(const char *name);

/* Library version string (pointer to internal data; do not free). */
TYPR_IO_API const char *typr_io_library_version(void);

/* Error reporting: functions that encounter internal errors will set a process-
 * wide "last error" string. Retrieve it (heap-allocated, free with
 * typr_io_free_string) or clear it. Returns NULL if there is no last error. */
TYPR_IO_API char *typr_io_get_last_error(void);
TYPR_IO_API void typr_io_clear_last_error(void);

/* Free strings returned by the C API (always safe to call with NULL). */
TYPR_IO_API void typr_io_free_string(char *s);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TYPR_IO_C_API_H */
