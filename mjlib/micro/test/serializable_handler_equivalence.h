// Copyright 2026 mjbots Robotic Systems, LLC.  info@mjbots.com
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

/// @file
///
/// Test helpers which verify that mjlib::micro::SerializableHandler
/// behaves identically to the original templated implementation
/// (legacy::SerializableHandler) for a given type.

#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/buffer_stream.h"
#include "mjlib/base/fast_stream.h"

#include "mjlib/micro/event_queue.h"
#include "mjlib/micro/serializable_handler.h"
#include "mjlib/micro/serializable_handler_legacy.h"
#include "mjlib/micro/stream_pipe.h"
#include "mjlib/micro/test/reader.h"

namespace mjlib {
namespace micro {
namespace test {

/// Finds values which the original implementation could only have
/// produced through undefined behavior: bools holding something other
/// than 0 or 1 (from corrupt binary input), and enumerations outside
/// of their range of values (from Set or corrupt binary input).  The
/// compact implementation stores the underlying bytes without loading
/// them as a bool or enumeration, so these can be detected afterwards.
struct InvalidValueFinder : base::VisitArchive<InvalidValueFinder> {
  bool found = false;

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& nvp) {
    Accept(nvp.value());
  }

  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair& nvp, NameMapGetter getter) {
    using E = std::remove_cv_t<std::remove_pointer_t<
      decltype(nvp.value())>>;
    using U = std::underlying_type_t<E>;
    // Scoped enumerations always have a fixed underlying type, so any
    // value of it is valid.
    if constexpr (std::is_convertible_v<E, int>) {
      U raw = {};
      std::memcpy(&raw, nvp.value(), sizeof(raw));
      int64_t min = 0;
      int64_t max = 0;
      for (const auto& pair : getter()) {
        const auto value = static_cast<int64_t>(pair.first);
        if (value < min) { min = value; }
        if (value > max) { max = value; }
      }
      // Without a fixed underlying type, the values are those of the
      // smallest bit-field which can hold all the enumerators.
      int64_t limit = 1;
      while (limit <= max || -limit > min) { limit <<= 1; }
      const int64_t value = static_cast<int64_t>(raw);
      if (value > limit - 1 || value < (min < 0 ? -limit : 0)) {
        found = true;
      }
    }
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& nvp) {
    Check(nvp.value());
  }

 private:
  void Check(bool* value) {
    uint8_t raw = 0;
    std::memcpy(&raw, value, 1);
    if (raw > 1) { found = true; }
  }

  template <typename T, std::size_t N>
  void Check(std::array<T, N>* value) {
    for (auto& item : *value) {
      Visit(base::ReferenceNameValuePair<T>(&item, ""));
    }
  }

  template <typename T>
  void Check(std::optional<T>* value) {
    if (*value) {
      Visit(base::ReferenceNameValuePair<T>(&**value, ""));
    }
  }

  template <typename T>
  void Check(T*) {}
};

template <typename T>
bool HasInvalidValue(T* item) {
  InvalidValueFinder finder;
  finder.Accept(item);
  return finder.found;
}

struct EquivalenceOptions {
  /// Return true to skip comparing the result of Set for the given
  /// key and value.  The compact implementation parses optional
  /// scalars according to their contained type, while the legacy
  /// implementation always used strtoull.
  std::function<bool (std::string_view key, std::string_view value)> skip_set;

  /// Maximum number of single byte corruptions of the binary encoding
  /// to try when comparing ReadBinary.
  int max_corruptions = 2000;
};

inline std::string Hex(const std::string& data) {
  std::string result;
  char buf[4] = {};
  for (const char c : data) {
    ::snprintf(buf, sizeof(buf), "%02x ", static_cast<uint8_t>(c));
    result += buf;
  }
  return result;
}

template <typename Handler>
std::string WriteBinaryOf(Handler& handler) {
  base::FastOStringStream stream;
  const int result = handler.WriteBinary(stream);
  BOOST_TEST(result == 0);
  return Hex(stream.str());
}

/// Serialize with the compact handler, regardless of which handler
/// populated the object.  Corrupt binary input can leave a bool with
/// a value other than 0 or 1, which the legacy handler would then
/// write out verbatim.
template <typename T>
std::string StateOf(T* item) {
  SerializableHandler<T> handler(item);
  return WriteBinaryOf(handler);
}

template <typename Handler>
std::string WriteSchemaOf(Handler& handler) {
  base::FastOStringStream stream;
  handler.WriteSchema(stream);
  return Hex(stream.str());
}

template <typename Context, typename Handler>
std::string EnumerateOf(Handler& handler, std::size_t buffer_size,
                        std::string_view prefix) {
  EventQueue event_queue;
  StreamPipe stream_pipe{event_queue.MakePoster()};
  Reader reader{stream_pipe.side_b()};
  std::vector<char> buffer(buffer_size + 1);
  Context context;
  int done_count = 0;
  error_code done_ec;
  handler.Enumerate(&context,
                    base::string_span(buffer.data(), buffer_size),
                    prefix,
                    *stream_pipe.side_a(),
                    [&](const error_code& ec) {
                      done_count++;
                      done_ec = ec;
                    });
  for (int i = 0; i < 100 && done_count == 0; i++) {
    event_queue.Poll();
  }
  event_queue.Poll();
  BOOST_TEST(done_count == 1);
  BOOST_TEST(!done_ec);
  return reader.data_.str();
}

template <typename Handler>
std::pair<int, std::string> ReadOf(Handler& handler, const std::string& key,
                                   std::size_t buffer_size) {
  EventQueue event_queue;
  StreamPipe stream_pipe{event_queue.MakePoster()};
  Reader reader{stream_pipe.side_b()};
  std::vector<char> buffer(buffer_size + 1);
  int done_count = 0;
  const int result = handler.Read(
      key, base::string_span(buffer.data(), buffer_size),
      *stream_pipe.side_a(),
      [&](const error_code&) { done_count++; });
  for (int i = 0; i < 10; i++) { event_queue.Poll(); }
  BOOST_TEST(done_count == (result == 0 ? 1 : 0));
  return {result, reader.data_.str()};
}

/// Parse the keys out of the result of Enumerate.
inline std::vector<std::string> KeysFromEnumerate(
    const std::string& enumerated, std::string_view prefix) {
  std::vector<std::string> result;
  std::size_t pos = 0;
  while (pos < enumerated.size()) {
    const auto eol = enumerated.find("\r\n", pos);
    if (eol == std::string::npos) { break; }
    const auto line = enumerated.substr(pos, eol - pos);
    pos = eol + 2;
    const auto space = line.find(' ');
    auto key = line.substr(0, space);
    BOOST_TEST_REQUIRE(key.substr(0, prefix.size() + 1) ==
                       std::string(prefix) + ".");
    result.push_back(key.substr(prefix.size() + 1));
  }
  return result;
}

/// Verify that every operation of the compact handler matches the
/// legacy handler for an object with the given initial value.
///
/// @return the keys that were enumerated
template <typename T>
std::vector<std::string> CheckEquivalence(
    const T& initial, const EquivalenceOptions& options = {}) {
  using LegacyContext = legacy::detail::EnumerateArchive::Context;

  T legacy_item = initial;
  T new_item = initial;
  legacy::SerializableHandler<T> legacy_handler(&legacy_item);
  SerializableHandler<T> new_handler(&new_item);

  // Binary data and schema.
  base::FastOStringStream binary_stream;
  legacy_handler.WriteBinary(binary_stream);
  const auto binary = binary_stream.str();
  BOOST_TEST(WriteBinaryOf(new_handler) == Hex(binary));
  BOOST_TEST(WriteSchemaOf(new_handler) == WriteSchemaOf(legacy_handler));

  // Enumerate, with a variety of buffer sizes.
  const std::string_view prefix = "pfx";
  const auto enumerated =
      EnumerateOf<LegacyContext>(legacy_handler, 1000, prefix);
  BOOST_TEST(EnumerateOf<EnumerateContext>(new_handler, 1000, prefix) ==
             enumerated);
  for (std::size_t size : {60, 25, 12, 6, 5, 3, 0}) {
    BOOST_TEST_CONTEXT("enumerate buffer " << size) {
      BOOST_TEST(EnumerateOf<EnumerateContext>(new_handler, size, prefix) ==
                 EnumerateOf<LegacyContext>(legacy_handler, size, prefix));
    }
  }

  const auto keys = KeysFromEnumerate(enumerated, prefix);

  // Read every key, along with a variety of malformed ones.
  std::vector<std::string> read_keys = keys;
  for (const auto& key : keys) {
    read_keys.push_back(key + ".extra");
    read_keys.push_back(key + ".");
    const auto dot = key.rfind('.');
    if (dot != std::string::npos) {
      read_keys.push_back(key.substr(0, dot));
      read_keys.push_back(key.substr(0, dot) + ".-1");
      read_keys.push_back(key.substr(0, dot) + ".99999");
      read_keys.push_back(key.substr(0, dot) + ".0x1");
      read_keys.push_back(key.substr(0, dot) + ".1x");
      read_keys.push_back(key.substr(0, dot) + "..");
    }
  }
  for (const char* key : {"", ".", "..", "nonexistent", "a.b.c"}) {
    read_keys.push_back(key);
  }
  for (const auto& key : read_keys) {
    BOOST_TEST_CONTEXT("read key '" << key << "'") {
      for (std::size_t size : {100, 5, 1, 0}) {
        BOOST_TEST_CONTEXT("buffer " << size) {
          const auto legacy_result = ReadOf(legacy_handler, key, size);
          const auto new_result = ReadOf(new_handler, key, size);
          BOOST_TEST(new_result.first == legacy_result.first);
          BOOST_TEST(new_result.second == legacy_result.second);
        }
      }
    }
  }

  // Set every key to a variety of values.
  const char* const values[] = {
    "0", "1", "-1", "2", "0x10", "1.5", "-2.25", "abc", "", "300", "-129",
    "65536", "4294967296", "18446744073709551615", "-9223372036854775808",
    "1e10", "  7",
  };
  for (const auto& key : read_keys) {
    for (const char* value : values) {
      if (options.skip_set && options.skip_set(key, value)) { continue; }
      BOOST_TEST_CONTEXT("set key '" << key << "' = '" << value << "'") {
        T new_copy = initial;
        SerializableHandler<T> new_copy_handler(&new_copy);
        const std::string value_str = value;
        const int new_result = new_copy_handler.Set(key, value_str);
        // The original implementation has undefined behavior when
        // setting an enumeration outside of its range of values.
        if (HasInvalidValue(&new_copy)) { continue; }

        T legacy_copy = initial;
        legacy::SerializableHandler<T> legacy_copy_handler(&legacy_copy);
        const int legacy_result = legacy_copy_handler.Set(key, value_str);
        BOOST_TEST(new_result == legacy_result);
        BOOST_TEST(WriteBinaryOf(new_copy_handler) ==
                   WriteBinaryOf(legacy_copy_handler));
      }
    }
  }

  // ReadBinary, from the full encoding, every truncation of it, and
  // a variety of corruptions.
  auto check_read = [&](const std::string& data) {
    T new_copy = T();
    SerializableHandler<T> new_copy_handler(&new_copy);
    base::BufferReadStream new_stream{data};
    const int new_result = new_copy_handler.ReadBinary(new_stream);

    // The original implementation has undefined behavior on such
    // input, so it cannot be run on it.
    if (HasInvalidValue(&new_copy)) { return; }

    T legacy_copy = T();
    legacy::SerializableHandler<T> legacy_copy_handler(&legacy_copy);
    base::BufferReadStream legacy_stream{data};
    const int legacy_result = legacy_copy_handler.ReadBinary(legacy_stream);
    BOOST_TEST(new_result == legacy_result);
    BOOST_TEST(new_stream.remaining() == legacy_stream.remaining());
    BOOST_TEST(StateOf(&new_copy) == StateOf(&legacy_copy));
  };

  check_read(binary);
  for (std::size_t i = 0; i < binary.size(); i++) {
    BOOST_TEST_CONTEXT("truncated to " << i) {
      check_read(binary.substr(0, i));
    }
  }
  int corruptions = 0;
  for (std::size_t i = 0; i < binary.size(); i++) {
    for (const char c : {'\x00', '\x01', '\x02', '\x7f', '\x80', '\xff'}) {
      if (corruptions++ >= options.max_corruptions) { break; }
      auto corrupted = binary;
      corrupted[i] = c;
      BOOST_TEST_CONTEXT("corrupted at " << i << " to " << int(c)) {
        check_read(corrupted);
      }
    }
  }

  // SetDefault.
  {
    T legacy_copy = initial;
    T new_copy = initial;
    legacy::SerializableHandler<T> legacy_copy_handler(&legacy_copy);
    SerializableHandler<T> new_copy_handler(&new_copy);
    legacy_copy_handler.SetDefault();
    new_copy_handler.SetDefault();
    BOOST_TEST(WriteBinaryOf(new_copy_handler) ==
               WriteBinaryOf(legacy_copy_handler));
  }

  return keys;
}

}
}
}
