// test_backend_thread.cpp
//
// Integration tests that exercise the real Sender + Listener implementations.
// These tests drive the real library (not fakes) and observe global key
// events via the Listener. Because these tests interact with the OS input
// stack and may require permissions / manual focus, they are disabled by
// default and only run when the environment variable:
//
//   TYPR_IO_RUN_INTEGRATION_TESTS=1
//
// is set by the user. Optionally, if you want the test run to pause and ask
// you to focus the terminal/window that should receive/reflect the injected
// events, set:
//
//   TYPR_IO_INTERACTIVE=1
//
// Note: these tests are explicitly intended for local manual integration
// testing. They can be flaky on CI systems, require platform permissions,
// and may need manual focus to be reliable.
#include <catch2/catch_all.hpp>

#include <typr-io/listener.hpp>
#include <typr-io/sender.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace typr::io;

namespace {

bool envFlagSet(const char *name) {
  const char *v = std::getenv(name);
  if (!v)
    return false;
  // Accept: 1, yes, true (case-insensitive-ish)
  if (v[0] == '1')
    return true;
  std::string s(v);
  for (auto &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s == "true" || s == "yes";
}

void maybePromptToFocus() {
  if (!envFlagSet("TYPR_IO_INTERACTIVE"))
    return;
  std::cout
      << "\nIntegration test wants you to focus the target UI/window.\n"
      << "Please click the application window that should receive keys (or\n"
      << "click this terminal if the test requires it).\n"
      << "The test will auto-send an Enter key shortly (no manual input "
         "required).\n"
      << std::endl;
}

void scheduleAutoEnter(Sender &s, int delayMs = 400) {
  // Only schedule auto-enter when interactive or explicitly requested via
  // TYPR_IO_AUTO_CONFIRM=1.
  if (!envFlagSet("TYPR_IO_INTERACTIVE") && !envFlagSet("TYPR_IO_AUTO_CONFIRM"))
    return;

  std::thread([delayMs, &s] {
    std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    bool ok = false;
    try {
      ok = s.tap(Key::Enter);
    } catch (const std::exception &ex) {
      std::cerr << "[integration] Auto-confirm tap threw exception: "
                << ex.what() << std::endl;
    } catch (...) {
      std::cerr << "[integration] Auto-confirm tap threw unknown exception\n";
    }
    std::cout << "[integration] Auto-confirm: tapped Enter (ok="
              << (ok ? "yes" : "no") << ")\n";
  }).detach();
}

// Small container representing an observed listener event.
struct ObservedEvent {
  Key key;
  Modifier mods;
  char32_t codepoint;
  bool pressed;
  std::chrono::steady_clock::time_point when;
};

} // namespace

TEST_CASE("integration: tap a single key (Z) is observed by a global Listener",
          "[integration][manual]") {
  if (!envFlagSet("TYPR_IO_RUN_INTEGRATION_TESTS")) {
    INFO("Integration tests are disabled; set TYPR_IO_RUN_INTEGRATION_TESTS=1 "
         "to enable");
    return;
  }

  Sender sender;
  auto caps = sender.capabilities();
  if (!caps.canInjectKeys) {
    INFO("Sender cannot inject physical keys on this platform; skipping "
         "integration test");
    return;
  }

  Listener listener;
  std::mutex mtx;
  std::condition_variable cv;
  std::vector<ObservedEvent> events;

  bool started = listener.start(
      [&](char32_t codepoint, Key key, Modifier mods, bool pressed) {
        std::lock_guard lg(mtx);
        events.push_back(ObservedEvent{key, mods, codepoint, pressed,
                                       std::chrono::steady_clock::now()});
        cv.notify_all();
      });

  if (!started) {
    WARN("Listener failed to start (permissions/platform support?). Skipping "
         "test.");
    return;
  }

  // Optionally prompt the user to focus a window/terminal so injection is more
  // reliable.
  maybePromptToFocus();
  std::cout << "[integration] Scheduling auto-confirm (Enter) and waiting "
               "briefly to allow focus...\n";
  scheduleAutoEnter(sender, 400);
  std::this_thread::sleep_for(500ms);

  {
    // Clear any spurious startup events
    std::lock_guard lg(mtx);
    events.clear();
  }

  // Attempt to tap the 'Z' key.
  INFO("Attempting to inject 'Z' key via Sender; waiting for press+release "
       "events.");
  std::cout << "[integration] Sending tap Z...\n";
  bool ok = sender.tap(Key::Z);
  std::cout << "[integration] tap Z returned ok=" << (ok ? "yes" : "no")
            << std::endl;
  REQUIRE(ok); // injection attempt should succeed (test intent is to exercise
               // the real backend)

  // Wait for a press + release for Key::Z
  auto predicate = [&] {
    std::lock_guard lg(mtx);
    bool seenPress = false, seenRelease = false;
    for (const auto &e : events) {
      if (e.key == Key::Z) {
        if (e.pressed)
          seenPress = true;
        else
          seenRelease = true;
      }
    }
    return seenPress && seenRelease;
  };

  {
    std::unique_lock lk(mtx);
    bool observed = cv.wait_for(lk, 3000ms, predicate);
    REQUIRE(observed); // we expect both press and release within timeout
  }

  // Verify ordering: find the first press and the subsequent release
  {
    std::lock_guard lg(mtx);
    int pressIdx = -1, releaseIdx = -1;
    for (size_t i = 0; i < events.size(); ++i) {
      if (events[i].key == Key::Z && events[i].pressed && pressIdx < 0)
        pressIdx = static_cast<int>(i);
      if (pressIdx >= 0 && events[i].key == Key::Z && !events[i].pressed) {
        releaseIdx = static_cast<int>(i);
        break;
      }
    }
    REQUIRE(pressIdx >= 0);
    REQUIRE(releaseIdx >= 0);
    REQUIRE(releaseIdx > pressIdx);
  }

  listener.stop();
}

TEST_CASE("integration: long keydown generates repeats when supported",
          "[integration][manual]") {
  if (!envFlagSet("TYPR_IO_RUN_INTEGRATION_TESTS")) {
    INFO("Integration tests are disabled; set TYPR_IO_RUN_INTEGRATION_TESTS=1 "
         "to enable");
    return;
  }

  Sender sender;
  auto caps = sender.capabilities();
  if (!caps.canInjectKeys) {
    INFO("Sender cannot inject physical keys on this platform; skipping "
         "integration test");
    return;
  }

  if (!caps.supportsKeyRepeat) {
    INFO("Backend does not support key repeat (no repeated key events on long "
         "press); skipping repeat test");
    return;
  }

  Listener listener;
  std::mutex mtx;
  std::condition_variable cv;
  std::vector<ObservedEvent> events;

  bool started = listener.start(
      [&](char32_t codepoint, Key key, Modifier mods, bool pressed) {
        std::lock_guard lg(mtx);
        events.push_back(ObservedEvent{key, mods, codepoint, pressed,
                                       std::chrono::steady_clock::now()});
        cv.notify_all();
      });

  if (!started) {
    WARN("Listener failed to start (permissions/platform support?). Skipping "
         "test.");
    return;
  }

  maybePromptToFocus();
  std::cout << "[integration] Scheduling auto-confirm (Enter) and waiting "
               "briefly to allow focus...\n";
  scheduleAutoEnter(sender, 400);
  std::this_thread::sleep_for(500ms);

  {
    std::lock_guard lg(mtx);
    events.clear();
  }

  // Press and hold 'Z' for a while to allow repeat events to be generated.
  INFO("Starting keyDown Z to trigger repeat; will hold then keyUp.");
  std::cout << "[integration] keyDown Z...\n";
  bool downOk = sender.keyDown(Key::Z);
  std::cout << "[integration] keyDown returned ok=" << (downOk ? "yes" : "no")
            << std::endl;
  REQUIRE(downOk);

  // Hold long enough to trigger repeats on platforms that support them.
  std::this_thread::sleep_for(700ms);

  std::cout << "[integration] keyUp Z...\n";
  bool upOk = sender.keyUp(Key::Z);
  std::cout << "[integration] keyUp returned ok=" << (upOk ? "yes" : "no")
            << std::endl;
  REQUIRE(upOk);

  // Wait for at least one release and for counting press events
  auto predicate = [&] {
    std::lock_guard lg(mtx);
    bool seenRelease = false;
    int pressCount = 0;
    for (const auto &e : events) {
      if (e.key == Key::Z) {
        if (!e.pressed)
          seenRelease = true;
        else
          ++pressCount;
      }
    }
    // We expect at least two press events (initial + repeat) and a release if
    // repeats are supported.
    return seenRelease && (pressCount >= 2);
  };

  {
    std::unique_lock lk(mtx);
    bool okObserved = cv.wait_for(lk, 3000ms, predicate);
    REQUIRE(okObserved); // requires that we saw repeated presses and a release
  }

  // Extra sanity: count presses
  {
    std::lock_guard lg(mtx);
    int pressCount = 0;
    for (const auto &e : events)
      if (e.key == Key::Z && e.pressed)
        ++pressCount;
    REQUIRE(pressCount >= 2);
  }

  listener.stop();
}

TEST_CASE("integration: sequential key taps (Z, W, Enter) are observed",
          "[integration][manual]") {
  if (!envFlagSet("TYPR_IO_RUN_INTEGRATION_TESTS")) {
    INFO("Integration tests are disabled; set TYPR_IO_RUN_INTEGRATION_TESTS=1 "
         "to enable");
    return;
  }

  Sender sender;
  auto caps = sender.capabilities();
  if (!caps.canInjectKeys) {
    INFO("Sender cannot inject physical keys on this platform; skipping "
         "integration test");
    return;
  }

  Listener listener;
  std::mutex mtx;
  std::condition_variable cv;
  std::vector<ObservedEvent> events;

  bool started = listener.start(
      [&](char32_t codepoint, Key key, Modifier mods, bool pressed) {
        std::lock_guard lg(mtx);
        events.push_back(ObservedEvent{key, mods, codepoint, pressed,
                                       std::chrono::steady_clock::now()});
        cv.notify_all();
      });

  if (!started) {
    WARN("Listener failed to start (permissions/platform support?). Skipping "
         "test.");
    return;
  }

  maybePromptToFocus();
  std::cout << "[integration] Scheduling auto-confirm (Enter) and waiting "
               "briefly to allow focus...\n";
  scheduleAutoEnter(sender, 400);
  std::this_thread::sleep_for(500ms);

  // Utility to send a tap and wait for its press+release to be observed.
  auto sendTapAndWait = [&](Key targetKey,
                            std::chrono::milliseconds timeout) -> bool {
    {
      std::lock_guard lg(mtx);
      // record current events size so we search only in the new suffix
      // (to avoid matching prior events).
    }
    std::cout << "[integration] Sending tap for " << keyToString(targetKey)
              << "...\n";
    bool ok = sender.tap(targetKey);
    std::cout << "[integration] tap returned ok=" << (ok ? "yes" : "no")
              << std::endl;
    if (!ok)
      return false;

    auto predicate = [&] {
      std::lock_guard lg(mtx);
      bool seenPress = false, seenRelease = false;
      // look for press+release for targetKey
      for (const auto &e : events) {
        if (e.key == targetKey) {
          if (e.pressed)
            seenPress = true;
          else
            seenRelease = true;
        }
      }
      return seenPress && seenRelease;
    };

    std::unique_lock lk(mtx);
    return cv.wait_for(lk, timeout, predicate);
  };

  // Send and verify taps for Z, W, and Enter in sequence
  REQUIRE(sendTapAndWait(Key::Z, 3000ms));
  REQUIRE(sendTapAndWait(Key::W, 3000ms));
  REQUIRE(sendTapAndWait(Key::Enter, 3000ms));

  listener.stop();
}
