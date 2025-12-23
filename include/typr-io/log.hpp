#pragma once

// typr-io - log.hpp
// Lightweight, header-only logging utility used by typr-io backends.
//
// Usage:
//   #include <typr-io/log.hpp>
//   TYPR_IO_LOG_DEBUG("something happened: %d", value);
//   TYPR_IO_LOG_INFO("ready");
//
// Configuration via environment:
//   - TYPR_IO_LOG_LEVEL (preferred): one of "debug", "info", "warn", "error"
//   - If TYPR_IO_LOG_LEVEL is not set, legacy env var TYPR_OSK_DEBUG_BACKEND is
//     respected: unset -> debug enabled (default during testing), "0" -> debug
//     off, otherwise debug enabled.
//
// The header is intentionally small and portable (works in C++ and
// Objective-C++).

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>

         namespace typr {
  namespace io {
  namespace log {

  enum class Level : int {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
  };

  inline const char *levelToString(Level l) {
    switch (l) {
    case Level::Debug:
      return "DEBUG";
    case Level::Info:
      return "INFO";
    case Level::Warn:
      return "WARN";
    case Level::Error:
      return "ERROR";
    default:
      return "UNKNOWN";
    }
  }

  // Parse runtime configuration to determine default log level. This prefers
  // TYPR_IO_LOG_LEVEL if present; otherwise falls back to legacy
  // TYPR_OSK_DEBUG_BACKEND behaviour (defaulting to Debug when unset for
  // testing).
  inline Level parseLevelFromEnv() {
    const char *lvlEnv = std::getenv("TYPR_IO_LOG_LEVEL");
    if (lvlEnv && lvlEnv[0] != '\0') {
      std::string s(lvlEnv);
      std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
      if (s == "debug" || s == "d" || s == "0")
        return Level::Debug;
      if (s == "info" || s == "i" || s == "1")
        return Level::Info;
      if (s == "warn" || s == "warning" || s == "w" || s == "2")
        return Level::Warn;
      if (s == "error" || s == "e" || s == "3")
        return Level::Error;
      // Unrecognized -> fallback to Info
      return Level::Info;
    }

    const char *legacy = std::getenv("TYPR_OSK_DEBUG_BACKEND");
    if (!legacy) {
      // Default to enabled for the time being while testing (preserve legacy
      // behaviour).
      return Level::Debug;
    }
    if (legacy[0] == '0')
      return Level::Info;
    return Level::Debug;
  }

  // Global log level (atomic to allow runtime changes).
  inline std::atomic<Level> &globalLevel() {
    static std::atomic<Level> lvl(parseLevelFromEnv());
    return lvl;
  }

  inline void setLevel(Level l) { globalLevel().store(l); }
  inline Level getLevel() { return globalLevel().load(); }

  // Whether a message at `level` should be emitted under the current level.
  // Lower enum value is more verbose (Debug = 0). Emit if message level is
  // >= current minimum level.
  inline bool isEnabled(Level level) {
    return static_cast<int>(level) >= static_cast<int>(getLevel());
  }

  // Internal: get a mutex for serialising output to stderr.
  inline std::mutex &outputMutex() {
    static std::mutex m;
    return m;
  }

  // Internal: emit a formatted message using a va_list. Thread-safe and prints
  // an ISO timestamp (local time) with millisecond precision.
  inline void vlog(Level level, const char *file, int line, const char *fmt,
                   va_list ap) {
    if (!isEnabled(level))
      return;

    // Grab time
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) %
              milliseconds(1000);
    std::time_t t = system_clock::to_time_t(now);

    // Convert to local time in a portable way
    std::tm tmbuf;
#if defined(_MSC_VER) || defined(_WIN32)
    localtime_s(&tmbuf, &t);
#else
    localtime_r(&t, &tmbuf);
#endif

    char timebuf[64];
    if (std::strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tmbuf) ==
        0) {
      // Fallback if strftime fails
      std::snprintf(timebuf, sizeof(timebuf), "%lld",
                    static_cast<long long>(t));
    }

    // Serialise output to avoid interleaving
    std::lock_guard<std::mutex> lk(outputMutex());

    // Header: timestamp.millis [LEVEL] file:line:
    std::fprintf(stderr, "[typr] %s.%03d [%s] %s:%d: ", timebuf,
                 static_cast<int>(ms.count()), levelToString(level), file,
                 line);

    // Body
    std::vfprintf(stderr, fmt, ap);

    // Newline and flush for immediacy (useful when used from tests / CI)
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
  }

  inline void log(Level level, const char *file, int line, const char *fmt,
                  ...) {
    if (!isEnabled(level))
      return;
    va_list ap;
    va_start(ap, fmt);
    vlog(level, file, line, fmt, ap);
    va_end(ap);
  }

  // Convenience helpers
  inline bool debugEnabled() { return isEnabled(Level::Debug); }

  } // namespace log
  } // namespace io
} // namespace typr

// Helper macros (include file/line automatically)
#define TYPR_IO_LOG_DEBUG(fmt, ...)                                            \
  ::typr::io::log::log(::typr::io::log::Level::Debug, __FILE__, __LINE__, fmt, \
                       ##__VA_ARGS__)
#define TYPR_IO_LOG_INFO(fmt, ...)                                             \
  ::typr::io::log::log(::typr::io::log::Level::Info, __FILE__, __LINE__, fmt,  \
                       ##__VA_ARGS__)
#define TYPR_IO_LOG_WARN(fmt, ...)                                             \
  ::typr::io::log::log(::typr::io::log::Level::Warn, __FILE__, __LINE__, fmt,  \
                       ##__VA_ARGS__)
#define TYPR_IO_LOG_ERROR(fmt, ...)                                            \
  ::typr::io::log::log(::typr::io::log::Level::Error, __FILE__, __LINE__, fmt, \
                       ##__VA_ARGS__)
