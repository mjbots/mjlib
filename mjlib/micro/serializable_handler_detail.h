// Copyright 2015-2018 Josh Pieper, jjp@pobox.com.
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

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#include "mjlib/base/visit_archive.h"
#include "mjlib/base/visitor.h"
#include "mjlib/base/tokenizer.h"

#include "mjlib/micro/async_stream.h"

namespace mjlib {
namespace micro {

namespace detail {
struct EnumerateArchive : public mjlib::base::VisitArchive<EnumerateArchive> {
  struct Context {
    std::string_view root_prefix;
    base::string_span buffer;
    uint16_t current_field_index_to_write = 0;
    AsyncWriteStream* stream = nullptr;
    ErrorCallback callback;
    StaticFunction<bool ()> evaluate_enumerate_archive;
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

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& pair) {
    this->VisitHelper(pair, pair.value(), 0);
  }

  template <typename NameValuePair, typename T>
  void VisitHelper(const NameValuePair& pair, T*, long) {
    auto old_index = *current_index_;
    (*current_index_)++;

    if (old_index == context_->current_field_index_to_write) {
      *done_ = true;

      std::string_view data = FormatField(
          context_->buffer, std::string_view(pair.name()), pair.get_value());
      AsyncWrite(*context_->stream, data, [ctx = this->context_](
                     base::error_code error) {
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
    if (static_cast<ssize_t>(ea->prefix_.size() + 2) > std::distance(*current, *end)) { return 1; }

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
    if (static_cast<ssize_t>(name.size() + 3) > std::distance(it, end)) {
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
    int result = ::snprintf(&(**current), std::distance(*current, *end),
                            GetFormatSpecifier(value), value);
    (*current) += result;
  }

  template <typename T>
  const char* GetFormatSpecifier(T) const { return "%d"; }

  const char* GetFormatSpecifier(float) const { return "%f"; }

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
    if (found_) { return; }
    if (my_key_ != std::string_view(pair.name())) { return; }
    found_ = true;

    mjlib::base::VisitArchive<Derived>::Visit(pair);
  }

  template <typename NameValuePair>
  void VisitSerializable(const NameValuePair& pair) {
    static_cast<Derived*>(this)->Make(remaining_key_).Accept(pair.value());
  }

  template <typename NameValuePair>
  void VisitArray(const NameValuePair& pair) {
    base::Tokenizer tokenizer(remaining_key_, ".");
    const auto index_str = tokenizer.next();
    const auto index =
        std::max<ssize_t>(
            0,
            std::min<ssize_t>(pair.value()->size() - 1,
                              std::strtol(index_str.data(), nullptr, 0)));
    static_cast<Derived*>(this)->Make(tokenizer.remaining()).Visit(
        base::ReferenceNameValuePair(&(*pair.value())[index], ""));
  }

  bool found() const { return found_; }

  std::string_view my_key_;
  std::string_view remaining_key_;
  bool found_ = false;
};

struct SetArchive : public ItemArchive<SetArchive> {
  SetArchive(const std::string_view& key,
             const std::string_view& value)
      : ItemArchive(key), value_(value) {}

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
  T ParseValue(const std::string_view& value) const {
    return std::strtol(&*value.begin(), nullptr, 0);
  }

  const std::string_view value_;
};

template <>
inline float SetArchive::ParseValue<float>(const std::string_view& value) const {
  return std::strtof(&*value.begin(), nullptr);
}

template <>
inline double SetArchive::ParseValue<double>(const std::string_view& value) const {
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

  template <typename NameValuePair>
  void VisitScalar(const NameValuePair& pair) {
    auto out_buffer = EmitValue(pair.get_value());

    AsyncWrite(stream_, out_buffer, callback_);
  }

  ReadArchive Make(const std::string_view& key) {
    return ReadArchive(key, buffer_, stream_, callback_);
  }

  template <typename T>
  std::string_view EmitValue(T value) {
    const int out_size = ::snprintf(
        &*buffer_.begin(), buffer_.size(), GetFormat(value), value);
    return std::string_view(buffer_.begin(), out_size);
  }

  template <typename T>
  static const char* GetFormat(T) { return "%d"; }

  static const char* GetFormat(uint32_t) { return "%ul"; }

  const base::string_span buffer_;
  AsyncWriteStream& stream_;
  ErrorCallback callback_;
};

template <>
inline std::string_view ReadArchive::EmitValue<float>(float value) {
  const int out_size = ::snprintf(
      &*buffer_.begin(), buffer_.size(), "%f", value);
  return std::string_view(buffer_.begin(), out_size);
}
}

}
}
