# axidev-io

`axidev-io` is a C library for keyboard input injection, global keyboard
listening, and mouse cursor/button observation on Windows and Linux.

## Build

```sh
python build.py
python build.py test
python build.py example
```

On Linux the build expects `libinput`, `libudev`, and `xkbcommon` through
`pkg-config`. Runtime keyboard initialization also needs the XKB data files
from `xkeyboard-config` or `xkb-data`, usually under `/usr/share/X11/xkb`.

The primary public header is `include/axidev-io/c_api.h`, including the logging
macros.

## Public API

Most consumers only need:

```c
#include <axidev-io/c_api.h>

int main(void) {
  if (!axidev_io_keyboard_initialize()) {
    return 1;
  }

  axidev_io_keyboard_type_text("Hello world");
  axidev_io_keyboard_tap((axidev_io_keyboard_key_with_modifier_t){
      .key = AXIDEV_IO_KEY_C,
      .mods = AXIDEV_IO_MOD_CTRL
  });

  axidev_io_keyboard_free();
  return 0;
}
```

The listener uses the same global-library model:

```c
static void on_key(uint32_t codepoint,
                   axidev_io_keyboard_key_with_modifier_t key_mod,
                   bool pressed,
                   void *user_data) {
  printf("Key event: codepoint=%u, key=%u, mods=%u, pressed=%d\n",
          codepoint, key_mod.key, key_mod.mods, pressed));
}

int main(void) {
  if (!axidev_io_listener_start(on_key, NULL)) {
    return 1;
  }

  axidev_io_listener_stop();
  return 0;
}
```

Mouse polling and listening use the same single global runtime:

```c
static void on_mouse(const axidev_io_mouse_state_t *state, void *user_data) {
  printf("Mouse: x=%d, y=%d, buttons=%u\n",
         state->x, state->y, state->buttons);
}

int main(void) {
  axidev_io_mouse_state_t state;
  axidev_io_mouse_poll(&state);

  if (!axidev_io_mouse_listener_start(on_mouse, NULL)) {
    return 1;
  }

  axidev_io_mouse_listener_stop();
  return 0;
}
```

## Notes

- Normal sender calls do not take sender handles, listener handles, or context
  handles.
- Shared runtime state lives in the single exported `axidev_io_global`
  pointer.
- Mouse callbacks report cursor/button state snapshots. On Linux, libinput
  does not expose the compositor cursor position directly, so the library tracks
  cursor position from observed pointer events.
- Printable text sending prefers layout-resolved key sequences; explicit
  `key_down`, `key_up`, and `tap` remain available for low-level control.
- `axidev_io_keyboard_key_down(key_mod, true)` requests backend-managed repeat
  for that held key where supported. On Windows, this repeat is emulated by the
  SendInput backend using the user's system keyboard repeat delay and speed
  settings. It is not native hardware typematic behavior and does not make the
  backend a HID device; `can_simulate_hid` remains false. On Linux/uinput, this
  flag does not change the existing backend behavior.
- Vendored dependency: `vendor/stb/stb_ds.h` with license text in
  `vendor/licenses/stb.txt`.

## Docs

- [docs/README.md](docs/README.md)
- [docs/consumers/README.md](docs/consumers/README.md)
- [docs/developers/README.md](docs/developers/README.md)

## License

See [LICENSE](LICENSE).
