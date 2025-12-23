// test_key_utils.cpp
// Comprehensive unit tests for key/string utilities, modifiers, and
// capabilities.
//
// These tests use Catch2 (v3+) and exercise:
//  - keyToString / stringToKey round-trip behavior and uniqueness of canonical
//  names
//  - alias/synonym lookups (e.g., \"esc\" -> Escape, \"kp1\" -> Numpad1)
//  - edge cases (invalid strings, whitespace handling)
//  - Modifier bit-ops and hasModifier helper
//  - Capabilities default values
//
// To run these tests enable TYPR_IO_BUILD_TESTS=ON when configuring the
// project.

#include <catch2/catch_all.hpp>

#include <typr-io/core.hpp>
#include <typr-io/log.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>

using namespace typr::io;

static std::string toLowerCopy(const std::string &s) {
  std::string out = s;
  std::ranges::transform(out, out.begin(), [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  });
  return out;
}

TEST_CASE("keyToString / stringToKey roundtrip and uniqueness", "[key_utils]") {
  TYPR_IO_LOG_INFO("test_key_utils: roundtrip/uniqueness start");
  std::unordered_set<std::string> seen;
  int canonicalCount = 0;

  for (unsigned i = 0; i <= 255u; ++i) {
    Key k = static_cast<Key>(i);
    std::string name = keyToString(k);

    // Canonical round-trip: when the canonical name is "Unknown" the
    // mapping should resolve to Key::Unknown. For all other canonical
    // names, the round-trip should return the same Key value.
    if (name == "Unknown") {
      REQUIRE(stringToKey(name) == Key::Unknown);
    } else {
      REQUIRE(stringToKey(name) == k);
    }

    // Count & uniqueness of non-Unknown canonical names
    if (name != "Unknown") {
      ++canonicalCount;
      auto [it, inserted] = seen.emplace(name);
      REQUIRE(inserted); // canonical names must be unique

      // Lowercased canonical should map back
      std::string lower = toLowerCopy(name);
      REQUIRE(stringToKey(lower) == k);

      // Uppercased canonical should also map back
      std::string upper = name;
      std::ranges::transform(upper, upper.begin(), [](char c) {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      });
      REQUIRE(stringToKey(upper) == k);
    }
  }

  // Sanity: ensure we found a healthy number of canonical keys
  REQUIRE(canonicalCount > 40);
}

TEST_CASE("Recognizes helpful aliases / synonyms", "[key_utils][aliases]") {
  TYPR_IO_LOG_INFO("test_key_utils: aliases/synonyms start");
  REQUIRE(stringToKey("esc") == Key::Escape);
  REQUIRE(stringToKey("ESC") == Key::Escape);
  REQUIRE(stringToKey("return") == Key::Enter);
  REQUIRE(stringToKey("spacebar") == Key::Space);
  REQUIRE(stringToKey("space") == Key::Space);
  REQUIRE(stringToKey("ctrl") == Key::CtrlLeft);
  REQUIRE(stringToKey("control") == Key::CtrlLeft);
  REQUIRE(stringToKey("shift") == Key::ShiftLeft);
  REQUIRE(stringToKey("alt") == Key::AltLeft);
  REQUIRE(stringToKey("super") == Key::SuperLeft);
  REQUIRE(stringToKey("meta") == Key::SuperLeft);
  REQUIRE(stringToKey("win") == Key::SuperLeft);

  REQUIRE(stringToKey("num0") == Key::Num0);
  REQUIRE(stringToKey("num1") == Key::Num1);
  REQUIRE(stringToKey("num2") == Key::Num2);

  REQUIRE(stringToKey("dash") == Key::Minus);
  REQUIRE(stringToKey("hyphen") == Key::Minus);
  REQUIRE(stringToKey("minus") == Key::Minus);
  REQUIRE(stringToKey("-") == Key::Minus);

  REQUIRE(stringToKey("grave") == Key::Grave);
  REQUIRE(stringToKey("`") == Key::Grave);

  REQUIRE(stringToKey("backslash") == Key::Backslash);
  REQUIRE(stringToKey("\\") == Key::Backslash);

  REQUIRE(stringToKey("bracketleft") == Key::LeftBracket);
  REQUIRE(stringToKey("bracketright") == Key::RightBracket);

  REQUIRE(stringToKey("kp0") == Key::Numpad0);
  REQUIRE(stringToKey("kp1") == Key::Numpad1);
  REQUIRE(stringToKey("kp9") == Key::Numpad9);
  REQUIRE(stringToKey("numpad1") == Key::Numpad1);

  REQUIRE(stringToKey("dot") == Key::Period);
  REQUIRE(stringToKey("period") == Key::Period);
}

TEST_CASE("Handles invalid and edge-case inputs", "[key_utils][edge]") {
  TYPR_IO_LOG_INFO("test_key_utils: edge-case inputs start");
  REQUIRE(stringToKey("NotAKey") == Key::Unknown);
  REQUIRE(stringToKey("") == Key::Unknown);
  // whitespace is NOT trimmed by design
  REQUIRE(stringToKey(" Enter") == Key::Unknown);
  REQUIRE(stringToKey("Enter ") == Key::Unknown);
}

TEST_CASE("keyToString returns expected canonical values",
          "[key_utils][canonical]") {
  TYPR_IO_LOG_INFO("test_key_utils: canonical values start");
  REQUIRE(keyToString(Key::A) == "A");
  REQUIRE(keyToString(Key::Num1) == "1");
  REQUIRE(keyToString(Key::F5) == "F5");
  REQUIRE(keyToString(Key::Tab) == "Tab");
  REQUIRE(keyToString(Key::Period) == ".");
  REQUIRE(keyToString(Key::Backslash) == "\\");
  REQUIRE(keyToString(Key::Minus) == "-");
}

TEST_CASE("Modifier bit-ops and helpers", "[modifier]") {
  TYPR_IO_LOG_INFO("test_key_utils: modifier bit-ops start");
  Modifier m = Modifier::None;
  REQUIRE(!hasModifier(m, Modifier::Shift));

  m |= Modifier::Shift;
  REQUIRE(hasModifier(m, Modifier::Shift));

  m |= Modifier::Ctrl;
  REQUIRE(hasModifier(m, Modifier::Shift));
  REQUIRE(hasModifier(m, Modifier::Ctrl));
  REQUIRE(!hasModifier(m, Modifier::Alt));

  Modifier n = m & Modifier::Ctrl;
  REQUIRE(hasModifier(n, Modifier::Ctrl));
  REQUIRE((!hasModifier(n, Modifier::Shift) || (n == Modifier::Ctrl)));

  m &= Modifier::Shift; // keep only Shift
  REQUIRE(hasModifier(m, Modifier::Shift));
  REQUIRE(!hasModifier(m, Modifier::Ctrl));
}

TEST_CASE("Capabilities defaults to false", "[capabilities]") {
  TYPR_IO_LOG_INFO("test_key_utils: capabilities defaults start");
  Capabilities c;
  REQUIRE(!c.canInjectKeys);
  REQUIRE(!c.canInjectText);
  REQUIRE(!c.canSimulateHID);
  REQUIRE(!c.supportsKeyRepeat);
  REQUIRE(!c.needsAccessibilityPerm);
  REQUIRE(!c.needsInputMonitoringPerm);
  REQUIRE(!c.needsUinputAccess);
}
