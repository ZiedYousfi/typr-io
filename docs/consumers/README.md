# Consumer Guide

## Include Surface

Use `include/axidev-io/c_api.h` as the normal entry header.
It also exposes the logging macros.

## Build

```sh
python build.py
python build.py example
```

On Linux, the project expects the following system packages:

* `libinput`
* `libudev`
* `xkbcommon`
* XKB keyboard data files from `xkeyboard-config` or `xkb-data`

## Linking

`axidev-io` itself may be linked statically or dynamically.

On Linux, backend dependencies are expected to be provided by the system as shared libraries.

When distributing applications, integrators are responsible for complying with the licenses of any third-party libraries they link against.

Example:

```sh
cc main.c -laxidev-io -linput -ludev -lxkbcommon -lpthread
```

## Linux Permissions

Linux usually needs two different kinds of access:

- injection needs write access to `/dev/uinput`
- listening needs read access to the relevant `/dev/input/event*` devices

For injection, load the kernel module and grant a group access to `/dev/uinput`
through `udev`:

```sh
sudo modprobe uinput
sudo groupadd -f input
sudo usermod -aG input "$USER"
```

The Linux integration-test bundle also includes
`scripts/setup_uinput_permissions.sh`, which applies the same setup steps.
Review it before running it on a target system.

Create `/etc/udev/rules.d/70-axidev-io-uinput.rules` with:

```udev
KERNEL=="uinput", MODE="0660", GROUP="input", OPTIONS+="static_node=uinput"
```

Then reload rules and re-login so the new group membership applies:

```sh
sudo udevadm control --reload-rules
sudo udevadm trigger /dev/uinput
```

After that, `axidev_io_keyboard_initialize()` should be able to open
`/dev/uinput`. On Linux, `axidev_io_keyboard_request_permissions()` only checks
whether access is already available; it does not open a desktop permission
prompt.

For listening, `libinput` opens device nodes such as `/dev/input/event*`. On
most desktop Linux systems this works when the process runs in the active local
session on `seat0`. If listener startup fails, first confirm the process is
running in a real local login session with access to the input seat.

Linux sender and listener initialization also require the XKB data tree that
`xkbcommon` compiles layouts from. On most distros that means the
`xkeyboard-config` or `xkb-data` package, which normally installs files under
`/usr/share/X11/xkb`.

If that directory is missing, incomplete, or your environment uses a custom
location, set `XKB_CONFIG_ROOT` to a valid XKB data directory before starting
the process.

When this requirement is not met, `axidev_io_keyboard_initialize()` and
`axidev_io_listener_start()` now fail early and report that the xkb keymap
could not be set instead of continuing with a broken listener startup.

Typical fix:

```sh
# Debian/Ubuntu
sudo apt-get install xkb-data

# Fedora
sudo dnf install xkeyboard-config

# Arch
sudo pacman -S xkeyboard-config
```

Avoid broad `udev` rules that make all `/dev/input/event*` nodes world-readable.
Those devices expose raw keyboard events and can capture sensitive input. If a
headless or service environment needs listener access, grant that access with a
dedicated service account or seat/session setup instead of relaxing permissions
globally.

## Basic Usage

```c
#include <axidev-io/c_api.h>

int main(void) {
  if (!axidev_io_keyboard_initialize()) {
    return 1;
  }

  axidev_io_keyboard_type_text("Hello");
  axidev_io_keyboard_tap((axidev_io_keyboard_key_with_modifier_t){
      .key = AXIDEV_IO_KEY_A,
      .mods = AXIDEV_IO_MOD_SHIFT
  });

  axidev_io_keyboard_free();
  return 0;
}
```

## Text Semantics

- `axidev_io_keyboard_type_text(const char *)` is the preferred public send
  path.
- Printable characters are resolved through the initialized keymap so the
  library sends the physical key and modifier sequence that produces the
  requested output on the active layout.
- Modifier literals such as `Ctrl+` and `Shift+` are parsed case-insensitively.
- A comma resets latched modifier literals for the next segment.

Example:

- `Ctrl+Shift+ca,E` means `Ctrl+Shift+C`, `Ctrl+Shift+A`, then uppercase `E`
  after the comma reset.

## Held Keys And Repeat

- `axidev_io_keyboard_key_down(key_mod, false)` sends one key-down transition.
- `axidev_io_keyboard_key_down(key_mod, true)` sends the key down and requests
  backend-managed repeat for that held key where supported.
- On Windows, repeat is emulated by the SendInput backend using
  `SystemParametersInfo` keyboard delay and speed settings captured during
  `axidev_io_keyboard_initialize()`. It is not native hardware typematic
  behavior and does not make the backend a HID device; `can_simulate_hid`
  remains false. In capabilities, `supports_key_repeat` means backend-emulated
  repeat is available on Windows.
- On Windows, changes to the user's keyboard repeat settings after
  initialization are not picked up automatically. To refresh those settings,
  call `axidev_io_keyboard_free()` and then `axidev_io_keyboard_initialize()`
  again before starting new repeated holds.
- On Linux/uinput, this flag does not change the existing backend behavior.
- On Windows, repeated keys are tied to the key/modifier mapping resolved by
  the original `key_down(..., true)` call. Modifier-only holds do not repeat.
  Multiple non-modifier keys may repeat simultaneously.
- `axidev_io_keyboard_release_all_modifiers()` cancels active Windows emulated
  repeats before releasing modifiers.
- Repeated synthetic Windows events may be observed by the global listener.

## Listener

- `axidev_io_listener_start()` starts the single global listener.
- Callbacks may run on an internal background thread.
- Keep listener callbacks thread-safe and short.

## Mouse

- `axidev_io_mouse_poll()` returns the latest cursor/button snapshot.
- `axidev_io_mouse_listener_start()` starts the single global mouse listener.
- Mouse callbacks receive an `axidev_io_mouse_state_t` with cursor position,
  button bitmask, scroll deltas for the current event, and a monotonic
  timestamp.
- On Windows, polling reads the current cursor and button state from Win32.
- On Linux, listening uses libinput on `seat0` and updates the tracked state
  from pointer events. libinput does not expose the desktop compositor cursor
  position directly, so polling returns the latest tracked state rather than
  querying a compositor-specific pointer API.

## Errors And Logging

- Failure details are available through `axidev_io_get_last_error()`.
- Strings returned by the library must be freed with `axidev_io_free_string()`.
- Logging can be controlled with `axidev_io_log_set_level()` or the macros from
  `c_api.h`.

## Platform Notes

- Windows uses the Win32 keyboard APIs for injection and a low-level hook for
  listening. Mouse observation uses Win32 cursor/button APIs and a low-level
  mouse hook.
- Linux injection uses `uinput`.
- Linux keyboard listening uses `libinput` plus `xkbcommon`; mouse listening
  uses `libinput`.
- macOS is not supported in this repository.
