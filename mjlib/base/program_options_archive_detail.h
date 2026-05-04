// Copyright 2023 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include <type_traits>

#include <boost/program_options.hpp>

namespace mjlib {
namespace base {

namespace detail {

template <typename NameValuePair>
class ProgramOptionsArchiveValue : public boost::program_options::value_semantic {
 public:
  ProgramOptionsArchiveValue(const NameValuePair& nvp) : nvp_(nvp) {}
  ~ProgramOptionsArchiveValue() override {}

  std::string name() const override { return ""; }
  unsigned min_tokens() const override { return 1; }
  unsigned max_tokens() const override { return 1; }
  bool is_composing() const override { return false; }
  bool is_required() const override { return false; }
  void parse(boost::any& value_store,
             const std::vector<std::string>& new_tokens,
             bool /* utf8 */) const override {
    value_store = boost::lexical_cast<
      typename std::decay<decltype(nvp_.get_value())>::type>(new_tokens.at(0));
  }

  bool apply_default(boost::any&) const override {
    return false;
  }

  void notify(const boost::any& value_store) const override {
    if (value_store.empty()) { return; }
    nvp_.set_value(boost::any_cast<decltype(nvp_.get_value())>(value_store));
  }

 private:
  NameValuePair nvp_;
};

template <typename NameValuePair, typename NameMapGetter>
class ProgramOptionsEnumArchiveValue
    : public boost::program_options::value_semantic {
 public:
  // The map is std::map<EnumT, const char*>; pull the enum type back
  // out so notify() can any_cast through it. NameValuePair has no
  // such typedef, which is why earlier code that reached for
  // typename NameValuePair::Base failed to compile.
  using EnumT = std::decay_t<decltype(
      std::declval<NameMapGetter>()().begin()->first)>;

  ProgramOptionsEnumArchiveValue(const NameValuePair& nvp,
                                 NameMapGetter mapper)
      : nvp_(nvp), mapper_(mapper) {}
  ~ProgramOptionsEnumArchiveValue() override {}

  std::string name() const override { return ""; }
  unsigned min_tokens() const override { return 1; }
  unsigned max_tokens() const override { return 1; }
  bool is_composing() const override { return false; }
  bool is_required() const override { return false; }
  void parse(boost::any& value_store,
             const std::vector<std::string>& new_tokens,
             bool /* utf8 */) const override {
    const std::string value = new_tokens.at(0);
    for (const auto& pair : mapper_()) {
      if (value == pair.second) {
        value_store = pair.first;
        return;
      }
    }

    auto format_enum_types = [&]() {
      std::string result;
      for (const auto& pair : mapper_()) {
        if (!result.empty()) { result += ","; }
        result += pair.second;
      }
      return result;
    };
    throw boost::program_options::invalid_option_value(
        "enum not in set: " + format_enum_types());
  }

  bool apply_default(boost::any&) const override {
    return false;
  }

  void notify(const boost::any& value_store) const override {
    if (value_store.empty()) { return; }
    nvp_.set_value(boost::any_cast<EnumT>(value_store));
  }

 private:
  NameValuePair nvp_;
  NameMapGetter mapper_;
};

}

}
}
