// Copyright 2015-2020 Josh Pieper, jjp@pobox.com.
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

#include <inttypes.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#include "mjlib/base/inplace_function.h"
#include "mjlib/base/visit_archive.h"
#include "mjlib/base/visitor.h"
#include "mjlib/base/tokenizer.h"

#include "mjlib/micro/async_stream.h"
#include "mjlib/micro/error_code.h"

namespace mjlib {
namespace micro {

namespace detail {
struct Empty {};

template <typename T>
struct MaybeToDouble {
  T operator()(T value) const { return value; }
};

template <>
struct MaybeToDouble<float> {
  double operator()(float value) const { return static_cast<double>(value); }
};

struct FormatSpecifier {
  static const char* GetFormat(bool) { return "%" PRIu8; }

  static const char* GetFormat(int8_t) { return "%" PRIi8; }
  static const char* GetFormat(int16_t) { return "%" PRIi16; }
  static const char* GetFormat(int32_t) { return "%" PRIi32; }
  static const char* GetFormat(int64_t) { return "%" PRIi64; }

  static const char* GetFormat(uint8_t) { return "%" PRIu8; }
  static const char* GetFormat(uint16_t) { return "%" PRIu16; }
  static const char* GetFormat(uint32_t) { return "%" PRIu32; }
  static const char* GetFormat(uint64_t) { return "%" PRIu64; }

  static const char* GetFormat(float) { return "%f"; }
  static const char* GetFormat(double) { return "%f"; }

  static const char* GetFormat(Empty) { return ""; }
};

struct EnumerateArchive : public mjlib::base::VisitArchive<EnumerateArchive> {
  struct Context {
    std::string_view root_prefix;
    base::string_span buffer;
    uint16_t current_field_index_to_write = 0;
    AsyncWriteStream* stream = nullptr;
    ErrorCallback callback;
    base::inplace_function<bool ()> evaluate_enumerate_archive;
  };

  EnumerateArchive(Context* context,
                   std::string_view prefix,
                   uint16_t* current_index,
                   bool* done,
                   EnumerateArchive* parent)
      : context_(context),
        prefix_(prefix),
        parent_(parent),
        current_index_(current_index),
        done_(done) {}

  template <typename NameValuePair>
  void Visit(const NameValuePair& pair) {
    if (*done_) { return; }
    VisitArchive<EnumerateArchive>::Visit(pair);
  }

  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair& pair,
                        NameMapGetter) {
    EmitValue(pair, [](auto value) { return static_cast<int32_t>(value); });
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& pair) {
    this->VisitHelper(pair, pair.value(), 0);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair, T*, long) {
    EmitValue(pair, [](auto value) { return value; });
  }

  template <typename NameValuePair, typename Mapper>
  void EmitValue(const NameValuePair& pair, Mapper mapper) {
    auto old_index = *current_index_;
    (*current_index_)++;

    if (old_index == context_->current_field_index_to_write) {
      *done_ = true;

      std::string_view data = FormatField(
          context_->buffer, std::string_view(pair.name()),
          mapper(pair.get_value()));
      AsyncWrite(*context_->stream, data, [ctx = this->context_](
                     error_code error) {
          if (error) { ctx->callback(error); return; }
          ctx->current_field_index_to_write++;

          bool done = ctx->evaluate_enumerate_archive();

          if (!done) {
            // We have finished with everything.
            ctx->callback({});
          } else {
            // This archive call should have enqueued another
            // callback, we are done.
          }
        });
    }
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair,
                   std::optional<T>*,
                   int) {
    // We only support optional scalar types.
    auto maybe_value = pair.get_value();
    if (!maybe_value) {
      Empty empty;
      Visit(base::ReferenceNameValuePair<Empty>(&empty, pair.name()));
    } else {
      auto value = *maybe_value;
      Visit(base::ReferenceNameValuePair<decltype(value)>(&value, pair.name()));
    }
  }

  template <typename NameValuePair, typename T, std::size_t N>
  void VisitHelper(const NameValuePair& pair,
                   std::array<T, N>*,
                   int) {
    auto value = pair.value();
    for (int i = 0; i < static_cast<int>(N); i++) {
      char number[6] = {};
      ::snprintf(number, sizeof(number), "%d", i);
      EnumerateArchive(
          context_, pair.name(), current_index_, done_, this).
          Visit(base::ReferenceNameValuePair<T>(
                    &(*value)[i], number));
    }
  }

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& pair) {
    EnumerateArchive(
        context_, std::string_view(pair.name()), current_index_, done_, this).
        Accept(pair.value());
  }

  template <typename Iterator>
  int FormatPrefix(Iterator* current, Iterator* end, EnumerateArchive* ea) {
    if (ea->parent_) { FormatPrefix(current, end, ea->parent_); }

    // Would we overflow?
    if (static_cast<std::ptrdiff_t>(ea->prefix_.size() + 2) >
        std::distance(*current, *end)) {
      return 1;
    }

    for (auto it = ea->prefix_.begin(); it != ea->prefix_.end(); ++it) {
      **current = *it;
      ++(*current);
    }

    **current = '.';
    ++(*current);
    return 0;
  }

  template <typename T>
  std::string_view FormatField(base::string_span buffer,
                               std::string_view name,
                               T value) {
    auto it = buffer.begin();
    auto end = buffer.end();

    if (FormatPrefix(&it, &end, this)) {
      return std::string_view();
    }
    if (static_cast<std::ptrdiff_t>(name.size() + 3) > std::distance(it, end)) {
      return std::string_view();
    }

    for (auto nit = name.begin(); nit != name.end(); ++nit) {
      *it = *nit;
      ++it;
    }

    *it = ' ';
    ++it;

    FormatValue(&it, &end, value);
    *it = '\r';
    ++it;
    *it = '\n';
    ++it;
    return std::string_view(buffer.begin(), it - buffer.begin());
  }

  template <typename Iterator, typename T>
  void FormatValue(Iterator* current, Iterator* end, T value) {
    int result = ::snprintf(
        &(**current), std::distance(*current, *end),
        FormatSpecifier::GetFormat(value),
        MaybeToDouble<typename std::remove_reference<T>::type>()(value));
    (*current) += result;
  }

 private:
  Context* const context_;
  const std::string_view prefix_;
  EnumerateArchive* const parent_;
  uint16_t* const current_index_;
  bool* const done_;
};

template <typename Derived>
struct ItemArchive : public mjlib::base::VisitArchive<Derived> {
  ItemArchive(const std::string_view& key) {
    base::Tokenizer tokenizer(key, ".");
    my_key_ = tokenizer.next();
    remaining_key_ = tokenizer.remaining();
  }

  template <typename NameValuePair>
  void Visit(const NameValuePair& pair) {
    if (done_) { return; }
    if (my_key_ != std::string_view(pair.name())) { return; }
    done_ = true;

    found_ = true;

    mjlib::base::VisitArchive<Derived>::Visit(pair);
  }

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& pair) {
    auto sub_archive = static_cast<Derived*>(this)->Make(remaining_key_);
    sub_archive.Accept(pair.value());
    found_ = sub_archive.found();
  }

  template <typename NameValuePair>
  void VisitArray(const NameValuePair& pair) {
    base::Tokenizer tokenizer(remaining_key_, ".");
    const auto index_str = tokenizer.next();
    char* str_end = nullptr;
    const char* const actual_end = index_str.data() + index_str.size();

    const std::size_t index = std::strtol(index_str.data(), &str_end, 0);
    if (index < 0 ||
        index >= pair.value()->size() ||
        str_end == nullptr ||
        str_end != actual_end) {
      found_ = false;
      return;
    }

    auto remaining = tokenizer.remaining();
    auto mapped_remaining =
        (remaining.size() ?
         std::string_view(remaining.data() - 1, remaining.size() + 1) :
         remaining);
    auto sub_archive = static_cast<Derived*>(this)->Make(mapped_remaining);
    sub_archive.Visit(
        base::ReferenceNameValuePair(&(*pair.value())[index], ""));
    found_ = sub_archive.found();
  }

  bool found() const { return found_; }

  std::string_view my_key_;
  std::string_view remaining_key_;
  bool done_ = false;
  bool found_ = false;
};

struct SetArchive : public ItemArchive<SetArchive> {
  SetArchive(const std::string_view& key,
             const std::string_view& value)
      : ItemArchive(key), value_(value) {}

  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair& pair, NameMapGetter getter) {
    pair.set_value(static_cast<decltype(getter().begin()->first)>(
                       ParseValue<int32_t>(value_)));
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& pair) {
    using Tref = decltype(pair.get_value());
    using T = typename std::remove_const<
      typename std::remove_reference<Tref>::type>::type;
    pair.set_value(ParseValue<T>(value_));
  }

  SetArchive Make(const std::string_view& key) {
    return SetArchive(key, value_);
  }

 private:
  template <typename T>
  T ParseValue(const std::string_view& value,
               std::enable_if_t<std::is_signed_v<T>, int> = 0) const {
    return std::strtoll(&*value.begin(), nullptr, 0);
  }

  template <typename T>
  T ParseValue(const std::string_view& value,
               std::enable_if_t<!std::is_signed_v<T>, int> = 0) const {
    return std::strtoull(&*value.begin(), nullptr, 0);
  }

  const std::string_view value_;
};

template <>
inline float SetArchive::ParseValue<float>(
    const std::string_view& value,
    std::enable_if_t<std::is_signed_v<float>, int>) const {
  return std::strtof(&*value.begin(), nullptr);
}

template <>
inline double SetArchive::ParseValue<double>(
    const std::string_view& value,
    std::enable_if_t<std::is_signed_v<double>, int>) const {
  return std::strtod(&*value.begin(), nullptr);
}

struct ReadArchive : public ItemArchive<ReadArchive> {
  ReadArchive(const std::string_view& key,
              const base::string_span& buffer,
              AsyncWriteStream& stream,
              ErrorCallback callback)
      : ItemArchive(key),
        buffer_(buffer),
        stream_(stream),
        callback_(callback) {
  }

  template <typename NameValuePair, typename NameMapGetter>
  void VisitEnumeration(const NameValuePair& pair,
                        NameMapGetter) {
    auto out_buffer = EmitValue(static_cast<int32_t>(pair.get_value()));

    AsyncWrite(stream_, out_buffer, callback_);
  }

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& pair) {
    this->VisitHelper(pair, pair.value(), 0);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair,
                   std::optional<T>*,
                   int) {
    auto maybe_value = pair.get_value();
    if (!maybe_value) {
      Empty empty;
      mjlib::base::VisitArchive<ReadArchive>::Visit(
          base::ReferenceNameValuePair<Empty>(&empty, pair.name()));
    } else {
      auto value = *maybe_value;
      mjlib::base::VisitArchive<ReadArchive>::Visit(
          base::ReferenceNameValuePair<decltype(value)>(&value, pair.name()));
    }
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair,
                   T*,
                   long) {
    auto out_buffer = EmitValue(pair.get_value());

    AsyncWrite(stream_, out_buffer, callback_);
  }

  ReadArchive Make(const std::string_view& key) {
    return ReadArchive(key, buffer_, stream_, callback_);
  }

  template <typename T>
  std::string_view EmitValue(T value) {
    const int out_size = ::snprintf(
        &*buffer_.begin(), buffer_.size(),
        FormatSpecifier::GetFormat(value), value);
    return std::string_view(buffer_.begin(), out_size);
  }

  const base::string_span buffer_;
  AsyncWriteStream& stream_;
  ErrorCallback callback_;
};

template <>
inline std::string_view ReadArchive::EmitValue<float>(float value) {
  const int out_size = ::snprintf(
      &*buffer_.begin(), buffer_.size(), "%f",
      static_cast<double>(value));
  return std::string_view(buffer_.begin(), out_size);
}

}

}
}
