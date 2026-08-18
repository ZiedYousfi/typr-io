#pragma once
#ifndef AXIDEV_IO_C_API_H
#define AXIDEV_IO_C_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef AXIDEV_IO_VERSION
#define AXIDEV_IO_VERSION "0.7.4"
#define AXIDEV_IO_VERSION_MAJOR 0
#define AXIDEV_IO_VERSION_MINOR 7
#define AXIDEV_IO_VERSION_PATCH 4
#endif

#ifndef AXIDEV_IO_API
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(axidev_io_EXPORTS)
#define AXIDEV_IO_API __declspec(dllexport)
#elif defined(AXIDEV_IO_STATIC)
#define AXIDEV_IO_API
#else
#define AXIDEV_IO_API __declspec(dllimport)
#endif
#else
#if defined(__GNUC__) && (__GNUC__ >= 4)
#define AXIDEV_IO_API __attribute__((visibility("default")))
#else
#define AXIDEV_IO_API
#endif
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define AXIDEV_IO_KEYBOARD_SENDER_STORAGE_SIZE 4096u
#define AXIDEV_IO_KEYBOARD_LISTENER_STORAGE_SIZE 16384u
#define AXIDEV_IO_KEYBOARD_KEYMAP_STORAGE_SIZE 8192u
#define AXIDEV_IO_MOUSE_STORAGE_SIZE 4096u
#define AXIDEV_IO_GLOBAL_PRIVATE_STORAGE_SIZE 2048u

typedef union axidev_io_keyboard_sender_storage_t {
  max_align_t _align;
  unsigned char bytes[AXIDEV_IO_KEYBOARD_SENDER_STORAGE_SIZE];
} axidev_io_keyboard_sender_storage_t;

typedef union axidev_io_keyboard_listener_storage_t {
  max_align_t _align;
  unsigned char bytes[AXIDEV_IO_KEYBOARD_LISTENER_STORAGE_SIZE];
} axidev_io_keyboard_listener_storage_t;

typedef union axidev_io_keyboard_keymap_storage_t {
  max_align_t _align;
  unsigned char bytes[AXIDEV_IO_KEYBOARD_KEYMAP_STORAGE_SIZE];
} axidev_io_keyboard_keymap_storage_t;

typedef union axidev_io_mouse_storage_t {
  max_align_t _align;
  unsigned char bytes[AXIDEV_IO_MOUSE_STORAGE_SIZE];
} axidev_io_mouse_storage_t;

typedef union axidev_io_global_private_storage_t {
  max_align_t _align;
  unsigned char bytes[AXIDEV_IO_GLOBAL_PRIVATE_STORAGE_SIZE];
} axidev_io_global_private_storage_t;

typedef enum axidev_io_keyboard_key_t {
  AXIDEV_IO_KEY_UNKNOWN = 0,
  AXIDEV_IO_KEY_A = 1,
  AXIDEV_IO_KEY_B = 2,
  AXIDEV_IO_KEY_C = 3,
  AXIDEV_IO_KEY_D = 4,
  AXIDEV_IO_KEY_E = 5,
  AXIDEV_IO_KEY_F = 6,
  AXIDEV_IO_KEY_G = 7,
  AXIDEV_IO_KEY_H = 8,
  AXIDEV_IO_KEY_I = 9,
  AXIDEV_IO_KEY_J = 10,
  AXIDEV_IO_KEY_K = 11,
  AXIDEV_IO_KEY_L = 12,
  AXIDEV_IO_KEY_M = 13,
  AXIDEV_IO_KEY_N = 14,
  AXIDEV_IO_KEY_O = 15,
  AXIDEV_IO_KEY_P = 16,
  AXIDEV_IO_KEY_Q = 17,
  AXIDEV_IO_KEY_R = 18,
  AXIDEV_IO_KEY_S = 19,
  AXIDEV_IO_KEY_T = 20,
  AXIDEV_IO_KEY_U = 21,
  AXIDEV_IO_KEY_V = 22,
  AXIDEV_IO_KEY_W = 23,
  AXIDEV_IO_KEY_X = 24,
  AXIDEV_IO_KEY_Y = 25,
  AXIDEV_IO_KEY_Z = 26,
  AXIDEV_IO_KEY_NUM0 = 33,
  AXIDEV_IO_KEY_NUM1 = 34,
  AXIDEV_IO_KEY_NUM2 = 35,
  AXIDEV_IO_KEY_NUM3 = 36,
  AXIDEV_IO_KEY_NUM4 = 37,
  AXIDEV_IO_KEY_NUM5 = 38,
  AXIDEV_IO_KEY_NUM6 = 39,
  AXIDEV_IO_KEY_NUM7 = 40,
  AXIDEV_IO_KEY_NUM8 = 41,
  AXIDEV_IO_KEY_NUM9 = 42,
  AXIDEV_IO_KEY_F1 = 43,
  AXIDEV_IO_KEY_F2 = 44,
  AXIDEV_IO_KEY_F3 = 45,
  AXIDEV_IO_KEY_F4 = 46,
  AXIDEV_IO_KEY_F5 = 47,
  AXIDEV_IO_KEY_F6 = 48,
  AXIDEV_IO_KEY_F7 = 49,
  AXIDEV_IO_KEY_F8 = 50,
  AXIDEV_IO_KEY_F9 = 51,
  AXIDEV_IO_KEY_F10 = 52,
  AXIDEV_IO_KEY_F11 = 53,
  AXIDEV_IO_KEY_F12 = 54,
  AXIDEV_IO_KEY_F13 = 55,
  AXIDEV_IO_KEY_F14 = 56,
  AXIDEV_IO_KEY_F15 = 57,
  AXIDEV_IO_KEY_F16 = 58,
  AXIDEV_IO_KEY_F17 = 59,
  AXIDEV_IO_KEY_F18 = 60,
  AXIDEV_IO_KEY_F19 = 61,
  AXIDEV_IO_KEY_F20 = 62,
  AXIDEV_IO_KEY_ENTER = 63,
  AXIDEV_IO_KEY_ESCAPE = 64,
  AXIDEV_IO_KEY_BACKSPACE = 65,
  AXIDEV_IO_KEY_TAB = 66,
  AXIDEV_IO_KEY_SPACE = 67,
  AXIDEV_IO_KEY_LEFT = 68,
  AXIDEV_IO_KEY_RIGHT = 69,
  AXIDEV_IO_KEY_UP = 70,
  AXIDEV_IO_KEY_DOWN = 71,
  AXIDEV_IO_KEY_HOME = 72,
  AXIDEV_IO_KEY_END = 73,
  AXIDEV_IO_KEY_PAGE_UP = 74,
  AXIDEV_IO_KEY_PAGE_DOWN = 75,
  AXIDEV_IO_KEY_DELETE = 76,
  AXIDEV_IO_KEY_INSERT = 77,
  AXIDEV_IO_KEY_PRINT_SCREEN = 78,
  AXIDEV_IO_KEY_SCROLL_LOCK = 79,
  AXIDEV_IO_KEY_PAUSE = 80,
  AXIDEV_IO_KEY_NUMPAD_DIVIDE = 83,
  AXIDEV_IO_KEY_NUMPAD_MULTIPLY = 84,
  AXIDEV_IO_KEY_NUMPAD_MINUS = 85,
  AXIDEV_IO_KEY_NUMPAD_PLUS = 86,
  AXIDEV_IO_KEY_NUMPAD_ENTER = 87,
  AXIDEV_IO_KEY_NUMPAD_DECIMAL = 88,
  AXIDEV_IO_KEY_NUMPAD0 = 89,
  AXIDEV_IO_KEY_NUMPAD1 = 90,
  AXIDEV_IO_KEY_NUMPAD2 = 91,
  AXIDEV_IO_KEY_NUMPAD3 = 92,
  AXIDEV_IO_KEY_NUMPAD4 = 93,
  AXIDEV_IO_KEY_NUMPAD5 = 94,
  AXIDEV_IO_KEY_NUMPAD6 = 95,
  AXIDEV_IO_KEY_NUMPAD7 = 96,
  AXIDEV_IO_KEY_NUMPAD8 = 97,
  AXIDEV_IO_KEY_NUMPAD9 = 98,
  AXIDEV_IO_KEY_SHIFT_LEFT = 99,
  AXIDEV_IO_KEY_SHIFT_RIGHT = 100,
  AXIDEV_IO_KEY_CTRL_LEFT = 101,
  AXIDEV_IO_KEY_CTRL_RIGHT = 102,
  AXIDEV_IO_KEY_ALT_LEFT = 103,
  AXIDEV_IO_KEY_ALT_RIGHT = 104,
  AXIDEV_IO_KEY_SUPER_LEFT = 105,
  AXIDEV_IO_KEY_SUPER_RIGHT = 106,
  AXIDEV_IO_KEY_CAPS_LOCK = 107,
  AXIDEV_IO_KEY_NUM_LOCK = 108,
  AXIDEV_IO_KEY_HELP = 109,
  AXIDEV_IO_KEY_MENU = 110,
  AXIDEV_IO_KEY_POWER = 111,
  AXIDEV_IO_KEY_SLEEP = 112,
  AXIDEV_IO_KEY_WAKE = 113,
  AXIDEV_IO_KEY_MUTE = 114,
  AXIDEV_IO_KEY_VOLUME_DOWN = 115,
  AXIDEV_IO_KEY_VOLUME_UP = 116,
  AXIDEV_IO_KEY_MEDIA_PLAY_PAUSE = 117,
  AXIDEV_IO_KEY_MEDIA_STOP = 118,
  AXIDEV_IO_KEY_MEDIA_NEXT = 119,
  AXIDEV_IO_KEY_MEDIA_PREVIOUS = 120,
  AXIDEV_IO_KEY_BRIGHTNESS_DOWN = 121,
  AXIDEV_IO_KEY_BRIGHTNESS_UP = 122,
  AXIDEV_IO_KEY_EJECT = 123,
  AXIDEV_IO_KEY_GRAVE = 124,
  AXIDEV_IO_KEY_MINUS = 125,
  AXIDEV_IO_KEY_EQUAL = 126,
  AXIDEV_IO_KEY_LEFT_BRACKET = 127,
  AXIDEV_IO_KEY_RIGHT_BRACKET = 128,
  AXIDEV_IO_KEY_BACKSLASH = 129,
  AXIDEV_IO_KEY_SEMICOLON = 130,
  AXIDEV_IO_KEY_APOSTROPHE = 131,
  AXIDEV_IO_KEY_COMMA = 132,
  AXIDEV_IO_KEY_PERIOD = 133,
  AXIDEV_IO_KEY_SLASH = 134,
  AXIDEV_IO_KEY_AT = 135,
  AXIDEV_IO_KEY_HASHTAG = 136,
  AXIDEV_IO_KEY_EXCLAMATION = 137,
  AXIDEV_IO_KEY_DOLLAR = 138,
  AXIDEV_IO_KEY_PERCENT = 139,
  AXIDEV_IO_KEY_CARET = 140,
  AXIDEV_IO_KEY_AMPERSAND = 141,
  AXIDEV_IO_KEY_ASTERISK = 142,
  AXIDEV_IO_KEY_LEFT_PAREN = 143,
  AXIDEV_IO_KEY_RIGHT_PAREN = 144,
  AXIDEV_IO_KEY_UNDERSCORE = 145,
  AXIDEV_IO_KEY_PLUS = 146,
  AXIDEV_IO_KEY_COLON = 147,
  AXIDEV_IO_KEY_QUOTE = 148,
  AXIDEV_IO_KEY_QUESTION_MARK = 149,
  AXIDEV_IO_KEY_BAR = 150,
  AXIDEV_IO_KEY_LESS_THAN = 151,
  AXIDEV_IO_KEY_GREATER_THAN = 152,
  AXIDEV_IO_KEY_ASCII_NUL = 160,
  AXIDEV_IO_KEY_ASCII_SOH = 161,
  AXIDEV_IO_KEY_ASCII_STX = 162,
  AXIDEV_IO_KEY_ASCII_ETX = 163,
  AXIDEV_IO_KEY_ASCII_EOT = 164,
  AXIDEV_IO_KEY_ASCII_ENQ = 165,
  AXIDEV_IO_KEY_ASCII_ACK = 166,
  AXIDEV_IO_KEY_ASCII_BELL = 167,
  AXIDEV_IO_KEY_ASCII_BACKSPACE = AXIDEV_IO_KEY_BACKSPACE,
  AXIDEV_IO_KEY_ASCII_TAB = AXIDEV_IO_KEY_TAB,
  AXIDEV_IO_KEY_ASCII_LF = AXIDEV_IO_KEY_ENTER,
  AXIDEV_IO_KEY_ASCII_VT = 171,
  AXIDEV_IO_KEY_ASCII_FF = 172,
  AXIDEV_IO_KEY_ASCII_CR = AXIDEV_IO_KEY_ENTER,
  AXIDEV_IO_KEY_ASCII_SO = 174,
  AXIDEV_IO_KEY_ASCII_SI = 175,
  AXIDEV_IO_KEY_ASCII_DLE = 176,
  AXIDEV_IO_KEY_ASCII_DC1 = 177,
  AXIDEV_IO_KEY_ASCII_DC2 = 178,
  AXIDEV_IO_KEY_ASCII_DC3 = 179,
  AXIDEV_IO_KEY_ASCII_DC4 = 180,
  AXIDEV_IO_KEY_ASCII_NAK = 181,
  AXIDEV_IO_KEY_ASCII_SYN = 182,
  AXIDEV_IO_KEY_ASCII_ETB = 183,
  AXIDEV_IO_KEY_ASCII_CAN = 184,
  AXIDEV_IO_KEY_ASCII_EM = 185,
  AXIDEV_IO_KEY_ASCII_SUB = 186,
  AXIDEV_IO_KEY_ASCII_ESCAPE = AXIDEV_IO_KEY_ESCAPE,
  AXIDEV_IO_KEY_ASCII_FS = 188,
  AXIDEV_IO_KEY_ASCII_GS = 189,
  AXIDEV_IO_KEY_ASCII_RS = 190,
  AXIDEV_IO_KEY_ASCII_US = 191,
  AXIDEV_IO_KEY_ASCII_DEL = AXIDEV_IO_KEY_DELETE,
  AXIDEV_IO_KEY_NUMPAD_EQUAL = 192,
  AXIDEV_IO_KEY_DEGREE = 193,
  AXIDEV_IO_KEY_STERLING = 194,
  AXIDEV_IO_KEY_MU = 195,
  AXIDEV_IO_KEY_PLUS_MINUS = 196,
  AXIDEV_IO_KEY_DEAD_CIRCUMFLEX = 197,
  AXIDEV_IO_KEY_DEAD_DIAERESIS = 198,
  AXIDEV_IO_KEY_SECTION = 199,
  AXIDEV_IO_KEY_CANCEL = 200,
  AXIDEV_IO_KEY_REDO = 201,
  AXIDEV_IO_KEY_UNDO = 202,
  AXIDEV_IO_KEY_FIND = 203,
  AXIDEV_IO_KEY_HANGUL = 204,
  AXIDEV_IO_KEY_HANGUL_HANJA = 205,
  AXIDEV_IO_KEY_KATAKANA = 206,
  AXIDEV_IO_KEY_HIRAGANA = 207,
  AXIDEV_IO_KEY_HENKAN = 208,
  AXIDEV_IO_KEY_MUHENKAN = 209,
  AXIDEV_IO_KEY_OE = 210,
  AXIDEV_IO_KEY_oe = 211,
  AXIDEV_IO_KEY_SUN_PROPS = 212,
  AXIDEV_IO_KEY_SUN_FRONT = 213,
  AXIDEV_IO_KEY_COPY = 214,
  AXIDEV_IO_KEY_OPEN = 215,
  AXIDEV_IO_KEY_PASTE = 216,
  AXIDEV_IO_KEY_CUT = 217,
  AXIDEV_IO_KEY_CALCULATOR = 218,
  AXIDEV_IO_KEY_EXPLORER = 219,
  AXIDEV_IO_KEY_PHONE = 220,
  AXIDEV_IO_KEY_WEB_CAM = 221,
  AXIDEV_IO_KEY_AUDIO_RECORD = 222,
  AXIDEV_IO_KEY_AUDIO_REWIND = 223,
  AXIDEV_IO_KEY_AUDIO_PRESET = 224,
  AXIDEV_IO_KEY_MESSENGER = 225,
  AXIDEV_IO_KEY_SEARCH = 226,
  AXIDEV_IO_KEY_GO = 227,
  AXIDEV_IO_KEY_FINANCE = 228,
  AXIDEV_IO_KEY_GAME = 229,
  AXIDEV_IO_KEY_SHOP = 230,
  AXIDEV_IO_KEY_HOME_PAGE = 231,
  AXIDEV_IO_KEY_RELOAD = 232,
  AXIDEV_IO_KEY_CLOSE = 233,
  AXIDEV_IO_KEY_SEND = 234,
  AXIDEV_IO_KEY_XFER = 235,
  AXIDEV_IO_KEY_LAUNCH_A = 236,
  AXIDEV_IO_KEY_LAUNCH_B = 237,
  AXIDEV_IO_KEY_LAUNCH1 = 238,
  AXIDEV_IO_KEY_LAUNCH2 = 239,
  AXIDEV_IO_KEY_LAUNCH3 = 240,
  AXIDEV_IO_KEY_LAUNCH4 = 241,
  AXIDEV_IO_KEY_LAUNCH5 = 242,
  AXIDEV_IO_KEY_LAUNCH6 = 243,
  AXIDEV_IO_KEY_LAUNCH7 = 244,
  AXIDEV_IO_KEY_LAUNCH8 = 245,
  AXIDEV_IO_KEY_LAUNCH9 = 246,
  AXIDEV_IO_KEY_TOUCHPAD_TOGGLE = 247,
  AXIDEV_IO_KEY_TOUCHPAD_ON = 248,
  AXIDEV_IO_KEY_TOUCHPAD_OFF = 249,
  AXIDEV_IO_KEY_KBD_LIGHT_ON_OFF = 250,
  AXIDEV_IO_KEY_KBD_BRIGHTNESS_DOWN = 251,
  AXIDEV_IO_KEY_KBD_BRIGHTNESS_UP = 252,
  AXIDEV_IO_KEY_MAIL = 253,
  AXIDEV_IO_KEY_MAIL_FORWARD = 254,
  AXIDEV_IO_KEY_SAVE = 255,
  AXIDEV_IO_KEY_DOCUMENTS = 256,
  AXIDEV_IO_KEY_BATTERY = 257,
  AXIDEV_IO_KEY_BLUETOOTH = 258,
  AXIDEV_IO_KEY_WLAN = 259,
  AXIDEV_IO_KEY_UWB = 260,
  AXIDEV_IO_KEY_NEXT_VMODE = 261,
  AXIDEV_IO_KEY_PREV_VMODE = 262,
  AXIDEV_IO_KEY_MON_BRIGHTNESS_CYCLE = 263,
  AXIDEV_IO_KEY_BRIGHTNESS_AUTO = 264,
  AXIDEV_IO_KEY_DISPLAY_OFF = 265,
  AXIDEV_IO_KEY_WWAN = 266,
  AXIDEV_IO_KEY_RF_KILL = 267
} axidev_io_keyboard_key_t;

typedef uint8_t axidev_io_keyboard_modifier_t;

enum {
  AXIDEV_IO_MOD_NONE = 0x00,
  AXIDEV_IO_MOD_SHIFT = 0x01,
  AXIDEV_IO_MOD_CTRL = 0x02,
  AXIDEV_IO_MOD_ALT = 0x04,
  AXIDEV_IO_MOD_SUPER = 0x08,
  AXIDEV_IO_MOD_CAPSLOCK = 0x10,
  AXIDEV_IO_MOD_NUMLOCK = 0x20
};

typedef enum axidev_io_keyboard_backend_type_t {
  AXIDEV_IO_BACKEND_UNKNOWN = 0,
  AXIDEV_IO_BACKEND_WINDOWS = 1,
  AXIDEV_IO_BACKEND_MACOS = 2,
  AXIDEV_IO_BACKEND_LINUX_LIBINPUT = 3,
  AXIDEV_IO_BACKEND_LINUX_UINPUT = 4
} axidev_io_keyboard_backend_type_t;

typedef struct axidev_io_keyboard_key_with_modifier_t {
  axidev_io_keyboard_key_t key;
  axidev_io_keyboard_modifier_t mods;
} axidev_io_keyboard_key_with_modifier_t;

typedef struct axidev_io_keyboard_capabilities_t {
  bool can_inject_keys;
  bool can_inject_text;
  bool can_simulate_hid;
  bool supports_key_repeat;
  bool needs_accessibility_perm;
  bool needs_input_monitoring_perm;
  bool needs_uinput_access;
} axidev_io_keyboard_capabilities_t;

typedef void (*axidev_io_keyboard_listener_cb)(
    uint32_t codepoint, axidev_io_keyboard_key_with_modifier_t key_mod,
    bool pressed, void *user_data);

typedef uint16_t axidev_io_mouse_button_t;

enum {
  AXIDEV_IO_MOUSE_BUTTON_NONE = 0x0000,
  AXIDEV_IO_MOUSE_BUTTON_LEFT = 0x0001,
  AXIDEV_IO_MOUSE_BUTTON_RIGHT = 0x0002,
  AXIDEV_IO_MOUSE_BUTTON_MIDDLE = 0x0004,
  AXIDEV_IO_MOUSE_BUTTON_BACK = 0x0008,
  AXIDEV_IO_MOUSE_BUTTON_FORWARD = 0x0010
};

typedef struct axidev_io_mouse_state_t {
  int32_t x;
  int32_t y;
  axidev_io_mouse_button_t buttons;
  double scroll_x;
  double scroll_y;
  uint64_t timestamp_ms;
} axidev_io_mouse_state_t;

typedef void (*axidev_io_mouse_listener_cb)(
    const axidev_io_mouse_state_t *state, void *user_data);

typedef struct axidev_io_keyboard_sender_context {
  bool initialized;
  bool ready;
  axidev_io_keyboard_modifier_t active_modifiers;
  uint32_t key_delay_us;
  uint64_t repeat_delay_ns;
  uint64_t repeat_interval_ns;
  axidev_io_keyboard_capabilities_t capabilities;
  axidev_io_keyboard_sender_storage_t storage;
} axidev_io_keyboard_sender_context;

typedef struct axidev_io_keyboard_listener_context {
  bool initialized;
  bool is_listening;
  axidev_io_keyboard_listener_storage_t storage;
} axidev_io_keyboard_listener_context;

typedef struct axidev_io_keyboard_keymap_context {
  bool initialized;
  axidev_io_keyboard_keymap_storage_t storage;
} axidev_io_keyboard_keymap_context;

typedef struct axidev_io_keyboard_context {
  bool initialized;
  axidev_io_keyboard_backend_type_t backend_type;
  axidev_io_keyboard_sender_context sender;
  axidev_io_keyboard_listener_context listener;
  axidev_io_keyboard_keymap_context keymap;
} axidev_io_keyboard_context;

typedef struct axidev_io_mouse_context {
  bool initialized;
  bool is_listening;
  axidev_io_keyboard_backend_type_t backend_type;
  axidev_io_mouse_state_t state;
  axidev_io_mouse_storage_t storage;
} axidev_io_mouse_context;

typedef uint8_t axidev_io_log_level_t;

enum {
  AXIDEV_IO_LOG_LEVEL_DEBUG = 0,
  AXIDEV_IO_LOG_LEVEL_INFO = 1,
  AXIDEV_IO_LOG_LEVEL_WARN = 2,
  AXIDEV_IO_LOG_LEVEL_ERROR = 3
};

typedef struct axidev_io_global_context {
  axidev_io_log_level_t log_level;
  char *last_error;
  axidev_io_keyboard_context keyboard;
  axidev_io_mouse_context mouse;
  axidev_io_global_private_storage_t private_storage;
} axidev_io_global_context;

AXIDEV_IO_API extern axidev_io_global_context *axidev_io_global;

AXIDEV_IO_API bool axidev_io_keyboard_initialize(void);
AXIDEV_IO_API void axidev_io_keyboard_free(void);
AXIDEV_IO_API bool axidev_io_keyboard_is_ready(void);
AXIDEV_IO_API axidev_io_keyboard_backend_type_t axidev_io_keyboard_type(void);
AXIDEV_IO_API void axidev_io_keyboard_get_capabilities(
    axidev_io_keyboard_capabilities_t *out_capabilities);
AXIDEV_IO_API bool axidev_io_keyboard_request_permissions(void);
AXIDEV_IO_API bool
axidev_io_keyboard_key_down(axidev_io_keyboard_key_with_modifier_t key_mod,
                            bool repeat);
AXIDEV_IO_API bool
axidev_io_keyboard_key_up(axidev_io_keyboard_key_with_modifier_t key_mod);
AXIDEV_IO_API bool
axidev_io_keyboard_tap(axidev_io_keyboard_key_with_modifier_t key_mod);
AXIDEV_IO_API axidev_io_keyboard_modifier_t
axidev_io_keyboard_active_modifiers(void);
AXIDEV_IO_API bool
axidev_io_keyboard_hold_modifier(axidev_io_keyboard_modifier_t mods);
AXIDEV_IO_API bool
axidev_io_keyboard_release_modifier(axidev_io_keyboard_modifier_t mods);
AXIDEV_IO_API bool axidev_io_keyboard_release_all_modifiers(void);
AXIDEV_IO_API bool axidev_io_keyboard_type_text(const char *text);
AXIDEV_IO_API bool axidev_io_keyboard_type_character(uint32_t codepoint);
AXIDEV_IO_API void axidev_io_keyboard_flush(void);
AXIDEV_IO_API void axidev_io_keyboard_set_key_delay(uint32_t delay_us);

AXIDEV_IO_API bool axidev_io_listener_start(axidev_io_keyboard_listener_cb cb,
                                            void *user_data);
AXIDEV_IO_API void axidev_io_listener_stop(void);
AXIDEV_IO_API bool axidev_io_listener_is_listening(void);

AXIDEV_IO_API bool axidev_io_mouse_poll(axidev_io_mouse_state_t *out_state);
AXIDEV_IO_API bool
axidev_io_mouse_listener_start(axidev_io_mouse_listener_cb cb,
                               void *user_data);
AXIDEV_IO_API void axidev_io_mouse_listener_stop(void);
AXIDEV_IO_API bool axidev_io_mouse_listener_is_listening(void);

AXIDEV_IO_API char *
axidev_io_keyboard_key_to_string(axidev_io_keyboard_key_t key);
AXIDEV_IO_API axidev_io_keyboard_key_t
axidev_io_keyboard_string_to_key(const char *name);
AXIDEV_IO_API char *axidev_io_keyboard_key_to_string_with_modifier(
    axidev_io_keyboard_key_with_modifier_t key_mod);
AXIDEV_IO_API bool axidev_io_keyboard_string_to_key_with_modifier(
    const char *combo, axidev_io_keyboard_key_with_modifier_t *out_key_mod);

AXIDEV_IO_API const char *axidev_io_library_version(void);
AXIDEV_IO_API char *axidev_io_get_last_error(void);
AXIDEV_IO_API void axidev_io_clear_last_error(void);
AXIDEV_IO_API void axidev_io_free_string(char *s);

AXIDEV_IO_API void axidev_io_log_set_level(axidev_io_log_level_t level);
AXIDEV_IO_API axidev_io_log_level_t axidev_io_log_get_level(void);
AXIDEV_IO_API bool axidev_io_log_is_enabled(axidev_io_log_level_t level);
AXIDEV_IO_API void axidev_io_log_message(axidev_io_log_level_t level,
                                         const char *file, int line,
                                         const char *fmt, ...);

#define AXIDEV_IO_LOG_DEBUG(fmt, ...)                                          \
  axidev_io_log_message(AXIDEV_IO_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt,    \
                        ##__VA_ARGS__)
#define AXIDEV_IO_LOG_INFO(fmt, ...)                                           \
  axidev_io_log_message(AXIDEV_IO_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt,     \
                        ##__VA_ARGS__)
#define AXIDEV_IO_LOG_WARN(fmt, ...)                                           \
  axidev_io_log_message(AXIDEV_IO_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt,     \
                        ##__VA_ARGS__)
#define AXIDEV_IO_LOG_ERROR(fmt, ...)                                          \
  axidev_io_log_message(AXIDEV_IO_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt,    \
                        ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
