#include <typr-io/listener.hpp>
#include <typr-io/log.hpp>
#include <typr-io/sender.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char **argv) {
  typr::io::Sender sender;
  auto caps = sender.capabilities();
  TYPR_IO_LOG_INFO("test_consumer: started argc=%d", argc);

  std::cout << "typr-io consumer\n";
  std::cout << "  canInjectKeys: " << (caps.canInjectKeys ? "yes" : "no")
            << "\n";
  std::cout << "  canInjectText: " << (caps.canInjectText ? "yes" : "no")
            << "\n";
  std::cout << "  canSimulateHID: " << (caps.canSimulateHID ? "yes" : "no")
            << "\n\n";

  if (argc <= 1) {
    std::cout
        << "Usage:\n"
        << "  --type <text>         : inject text (if supported)\n"
        << "  --tap <KEYNAME>       : tap the named key (e.g., A, Enter, F1)\n"
        << "  --listen <secs>       : listen for global key events for N "
           "seconds\n"
        << "  --request-permissions : attempt to request runtime platform "
           "permissions (e.g., macOS Accessibility)\n"
        << "  --help                : show this help\n";
    return 0;
  }

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--help") {
      // Already printed above
      continue;

    } else if (arg == "--type") {
      if (i + 1 >= argc) {
        std::cerr << "--type requires an argument\n";
        return 1;
      }
      std::string text = argv[++i];
      if (!caps.canInjectText) {
        std::cerr << "Backend cannot inject arbitrary text on this "
                     "platform/backend\n";
      } else {
        TYPR_IO_LOG_INFO("test_consumer: attempting to type text len=%zu",
                         text.size());
        std::cout << "Attempting to type: \"" << text << "\"\n";
        bool ok = sender.typeText(text);
        TYPR_IO_LOG_INFO("test_consumer: typeText result=%u",
                         static_cast<unsigned>(ok));
        std::cout << (ok ? "-> Success\n" : "-> Failed\n");
      }

    } else if (arg == "--tap") {
      if (i + 1 >= argc) {
        std::cerr << "--tap requires a key name (e.g., A, Enter, F1)\n";
        return 1;
      }
      std::string keyName = argv[++i];
      typr::io::Key k = typr::io::stringToKey(keyName);
      if (k == typr::io::Key::Unknown) {
        std::cerr << "Unknown key: " << keyName << "\n";
        continue;
      }
      if (!caps.canInjectKeys) {
        std::cerr << "Sender cannot inject physical keys on this platform\n";
        continue;
      }
      TYPR_IO_LOG_INFO("test_consumer: tapping key=%s", keyName.c_str());
      std::cout << "Tapping key: " << typr::io::keyToString(k) << "\n";
      bool ok = sender.tap(k);
      TYPR_IO_LOG_INFO("test_consumer: tap result=%u",
                       static_cast<unsigned>(ok));
      std::cout << (ok ? "-> Success\n" : "-> Failed\n");

    } else if (arg == "--request-permissions") {
      std::cout << "Requesting runtime permissions (may prompt the OS)...\n";
      bool permOk = sender.requestPermissions();
      TYPR_IO_LOG_INFO("test_consumer: requestPermissions -> %u",
                       static_cast<unsigned>(permOk));
      std::cout
          << (permOk
                  ? "-> Sender reports ready to inject\n"
                  : "-> Sender reports not ready (permission not granted?)\n");
      auto newCaps = sender.capabilities();
      TYPR_IO_LOG_DEBUG("test_consumer: newCaps canInjectKeys=%u "
                        "canInjectText=%u canSimulateHID=%u",
                        static_cast<unsigned>(newCaps.canInjectKeys),
                        static_cast<unsigned>(newCaps.canInjectText),
                        static_cast<unsigned>(newCaps.canSimulateHID));
      std::cout << "  canInjectKeys: " << (newCaps.canInjectKeys ? "yes" : "no")
                << "\n";
      std::cout << "  canInjectText: " << (newCaps.canInjectText ? "yes" : "no")
                << "\n";
      std::cout << "  canSimulateHID: "
                << (newCaps.canSimulateHID ? "yes" : "no") << "\n\n";

      std::cout << "Attempting to start a Listener to check Input Monitoring "
                   "permission...\n";
      TYPR_IO_LOG_INFO("test_consumer: attempting to start temporary listener "
                       "to check input-monitoring permission");
      {
        typr::io::Listener tmpListener;
        bool started = tmpListener.start(
            [](char32_t, typr::io::Key, typr::io::Modifier, bool) {});
        TYPR_IO_LOG_INFO("test_consumer: temporary listener started=%u",
                         static_cast<unsigned>(started));
        if (!started) {
          std::cout << "-> Listener failed to start (Input Monitoring "
                       "permission may be required on macOS).\n";
          TYPR_IO_LOG_WARN("test_consumer: temporary listener failed to start");
        } else {
          std::cout << "-> Listener started successfully.\n";
          tmpListener.stop();
          TYPR_IO_LOG_INFO("test_consumer: temporary listener stopped");
        }
      }

    } else if (arg == "--listen") {
      if (i + 1 >= argc) {
        std::cerr << "--listen requires a duration in seconds\n";
        return 1;
      }
      int seconds = 0;
      try {
        seconds = std::stoi(argv[++i]);
      } catch (...) {
        std::cerr << "Invalid number for --listen\n";
        return 1;
      }

      TYPR_IO_LOG_INFO("test_consumer: starting listener for %d seconds",
                       seconds);
      typr::io::Listener listener;
      bool started = listener.start([](char32_t codepoint, typr::io::Key key,
                                       typr::io::Modifier mods, bool pressed) {
        TYPR_IO_LOG_DEBUG(
            "test_consumer: listener event %s key=%s cp=%u mods=0x%02x",
            pressed ? "press" : "release", typr::io::keyToString(key).c_str(),
            static_cast<unsigned>(codepoint),
            static_cast<int>(static_cast<uint8_t>(mods)));
        std::cout << (pressed ? "[press]  " : "[release] ")
                  << "Key=" << typr::io::keyToString(key)
                  << " CP=" << static_cast<unsigned>(codepoint) << " Mods=0x"
                  << std::hex << static_cast<int>(static_cast<uint8_t>(mods))
                  << std::dec << "\n";
      });

      if (!started) {
        TYPR_IO_LOG_ERROR("test_consumer: listener failed to start "
                          "(permissions / platform support?)");
        std::cerr
            << "Listener failed to start (permissions / platform support?)\n";
        continue;
      }
      TYPR_IO_LOG_INFO("test_consumer: listener started");
      std::cout << "Listening for " << seconds << " second(s)...\n";
      std::this_thread::sleep_for(std::chrono::seconds(seconds));
      listener.stop();
      TYPR_IO_LOG_INFO("test_consumer: listener stopped");
      std::cout << "Stopped listening\n";

    } else {
      TYPR_IO_LOG_WARN("test_consumer: unknown argument: %s", arg.c_str());
      std::cerr << "Unknown argument: " << arg << "\n";
      return 1;
    }
  }

  TYPR_IO_LOG_INFO("test_consumer: exiting");
  return 0;
}
