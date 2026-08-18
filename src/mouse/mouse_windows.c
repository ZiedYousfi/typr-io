#ifdef _WIN32

#include "mouse_internal.h"

#include <Windows.h>

#include <stdatomic.h>

static _Atomic(axidev_io_mouse_impl *) g_active_mouse_listener;

static axidev_io_mouse_button_t axidev_io_windows_mouse_buttons(void) {
  axidev_io_mouse_button_t buttons = AXIDEV_IO_MOUSE_BUTTON_NONE;
  if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
    buttons |= AXIDEV_IO_MOUSE_BUTTON_LEFT;
  }
  if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
    buttons |= AXIDEV_IO_MOUSE_BUTTON_RIGHT;
  }
  if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) {
    buttons |= AXIDEV_IO_MOUSE_BUTTON_MIDDLE;
  }
  if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) {
    buttons |= AXIDEV_IO_MOUSE_BUTTON_BACK;
  }
  if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) {
    buttons |= AXIDEV_IO_MOUSE_BUTTON_FORWARD;
  }
  return buttons;
}

static axidev_io_mouse_state_t axidev_io_windows_mouse_snapshot(void) {
  POINT point;
  axidev_io_mouse_state_t state;
  axidev_io_mouse_load_state(axidev_io_mouse_impl_get(), &state);
  if (GetCursorPos(&point)) {
    state.x = point.x;
    state.y = point.y;
  }
  state.buttons = axidev_io_windows_mouse_buttons();
  state.timestamp_ms = axidev_io_monotonic_time_ms();
  return state;
}

axidev_io_result axidev_io_mouse_poll_internal(
    axidev_io_mouse_state_t *out_state) {
  axidev_io_mouse_impl *impl = axidev_io_mouse_impl_get();
  axidev_io_mouse_state_t state;

  if (out_state == NULL) {
    return AXIDEV_IO_RESULT_INVALID_ARGUMENT;
  }
  if (!axidev_io_mouse_prepare_locks(impl)) {
    return AXIDEV_IO_RESULT_INTERNAL_ERROR;
  }
  state = axidev_io_windows_mouse_snapshot();
  axidev_io_mouse_store_state(impl, &state);
  *out_state = state;
  axidev_io_mouse_public_context()->initialized = true;
  axidev_io_mouse_public_context()->backend_type = AXIDEV_IO_BACKEND_WINDOWS;
  return AXIDEV_IO_RESULT_OK;
}

static void
axidev_io_windows_mouse_publish(axidev_io_mouse_impl *impl,
                                const axidev_io_mouse_state_t *state) {
  axidev_io_mouse_store_state(impl, state);
  axidev_io_mouse_invoke_callback(impl, state);
}

static LRESULT CALLBACK axidev_io_low_level_mouse_proc(int nCode,
                                                       WPARAM wParam,
                                                       LPARAM lParam) {
  axidev_io_mouse_impl *impl;
  const MSLLHOOKSTRUCT *mouse;
  axidev_io_mouse_state_t state;

  if (nCode < 0) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  impl = atomic_load(&g_active_mouse_listener);
  if (impl == NULL) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }

  mouse = (const MSLLHOOKSTRUCT *)lParam;
  axidev_io_mouse_load_state(impl, &state);
  state.x = mouse->pt.x;
  state.y = mouse->pt.y;
  state.buttons = axidev_io_windows_mouse_buttons();
  state.scroll_x = 0.0;
  state.scroll_y = 0.0;
  if (wParam == WM_MOUSEWHEEL) {
    state.scroll_y =
        (double)(SHORT)HIWORD(mouse->mouseData) / (double)WHEEL_DELTA;
  } else if (wParam == WM_MOUSEHWHEEL) {
    state.scroll_x =
        (double)(SHORT)HIWORD(mouse->mouseData) / (double)WHEEL_DELTA;
  }
  state.timestamp_ms = axidev_io_monotonic_time_ms();
  axidev_io_windows_mouse_publish(impl, &state);
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static int axidev_io_mouse_thread_main(void *user_data) {
  axidev_io_mouse_impl *impl = (axidev_io_mouse_impl *)user_data;
  MSG message;
  axidev_io_mouse_state_t state;

  impl->thread_id = GetCurrentThreadId();
  atomic_store(&g_active_mouse_listener, impl);
  state = axidev_io_windows_mouse_snapshot();
  axidev_io_mouse_store_state(impl, &state);
  impl->hook = SetWindowsHookEx(WH_MOUSE_LL, axidev_io_low_level_mouse_proc,
                                GetModuleHandle(NULL), 0);
  if (impl->hook == NULL) {
    atomic_store(&g_active_mouse_listener, NULL);
    impl->thread_id = 0;
    atomic_store(&impl->running, false);
    atomic_store(&impl->ready, false);
    return 1;
  }

  atomic_store(&impl->ready, true);
  while (GetMessage(&message, NULL, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  UnhookWindowsHookEx((HHOOK)impl->hook);
  impl->hook = NULL;
  impl->thread_id = 0;
  atomic_store(&impl->ready, false);
  atomic_store(&g_active_mouse_listener, NULL);
  return 0;
}

axidev_io_result axidev_io_mouse_listener_start_internal(
    axidev_io_mouse_listener_cb callback, void *user_data) {
  axidev_io_mouse_impl *impl = axidev_io_mouse_impl_get();

  if (callback == NULL) {
    return AXIDEV_IO_RESULT_INVALID_ARGUMENT;
  }
  if (!axidev_io_mouse_prepare_locks(impl)) {
    return AXIDEV_IO_RESULT_INTERNAL_ERROR;
  }
  if (atomic_load(&impl->running)) {
    return AXIDEV_IO_RESULT_ALREADY_INITIALIZED;
  }

  axidev_io_mutex_lock(&impl->callback_lock);
  impl->callback = callback;
  impl->user_data = user_data;
  axidev_io_mutex_unlock(&impl->callback_lock);

  atomic_store(&impl->running, true);
  atomic_store(&impl->ready, false);
  if (!axidev_io_thread_create(&impl->worker, axidev_io_mouse_thread_main,
                               impl)) {
    atomic_store(&impl->running, false);
    return AXIDEV_IO_RESULT_PLATFORM_ERROR;
  }

  for (int i = 0; i < 40; ++i) {
    if (!atomic_load(&impl->running)) {
      axidev_io_thread_join(&impl->worker);
      return AXIDEV_IO_RESULT_PLATFORM_ERROR;
    }
    if (atomic_load(&impl->ready)) {
      axidev_io_mouse_public_context()->initialized = true;
      axidev_io_mouse_public_context()->is_listening = true;
      axidev_io_mouse_public_context()->backend_type = AXIDEV_IO_BACKEND_WINDOWS;
      return AXIDEV_IO_RESULT_OK;
    }
    axidev_io_sleep_ms(5);
  }

  atomic_store(&impl->running, false);
  if (impl->thread_id != 0) {
    PostThreadMessage(impl->thread_id, WM_QUIT, 0, 0);
  }
  axidev_io_thread_join(&impl->worker);
  return AXIDEV_IO_RESULT_PLATFORM_ERROR;
}

void axidev_io_mouse_listener_stop_internal(void) {
  axidev_io_mouse_impl *impl = axidev_io_mouse_impl_get();

  if (!atomic_load(&impl->running)) {
    axidev_io_mouse_public_context()->is_listening = false;
    return;
  }

  atomic_store(&impl->running, false);
  if (impl->thread_id != 0) {
    PostThreadMessage(impl->thread_id, WM_QUIT, 0, 0);
  }
  axidev_io_thread_join(&impl->worker);
  axidev_io_mouse_public_context()->is_listening = false;
  axidev_io_mouse_public_context()->initialized = true;
}

#endif
