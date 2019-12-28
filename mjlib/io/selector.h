// Copyright 2019 Josh Pieper, jjp@pobox.com.
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

#include <map>
#include <memory>
#include <sstream>
#include <string>

#include <boost/program_options.hpp>

#include "mjlib/base/program_options_archive.h"
#include "mjlib/base/system_error.h"
#include "mjlib/io/async_types.h"

namespace mjlib {
namespace io {

/// Provide a runtime configurable mechanism to select between
/// multiple derived classes, each with unique configuration
/// requirements.  Each class needs to meet certain properties.
///
/// * They must all derive from the common class Base
/// * They must have a nested Options type, which is passed as the
///   first argument to the constructor and is serializable with
///   base::ProgramOptionsArchive.
/// * All other constructor arguments must be of the same type.
/// * It must have an "AsyncStart" method of the normal form.
template <typename Base, typename... Args>
class Selector {
 public:
  Selector(const std::string& selector_name)
      : selector_name_(selector_name) {}

  template <typename Derived>
  void Register(const std::string& name) {
    auto derived = std::make_unique<Concrete<Derived>>();
    items_.insert(std::make_pair(name, std::move(derived)));
  }

  void AddToProgramOptions(
      boost::program_options::options_description* options,
      const std::string& prefix) {
    std::ostringstream help_text;
    bool first = true;
    for (auto& item_pair : items_) {
      if (!first) {
        help_text << "/";
      }
      first = false;
      if (item_pair.first == name_) {
        help_text << "*" << item_pair.first << "*";
      } else {
        help_text << item_pair.first;
      }
    }

    (*options).add_options()
        (selector_name_.c_str(), boost::program_options::value(&name_),
         help_text.str().c_str());

    for (auto& item_pair : items_) {
      item_pair.second->AddToProgramOptions(
          options, prefix + "." + item_pair.first + ".");
    }
  }

  void set_default(const std::string& name) { name_ = name; }

  Base* selected() { return selected_.get(); }
  const Base* selected() const { return selected_.get(); }

  void AsyncStart(mjlib::io::ErrorCallback callback, Args... args) {
    const auto it = items_.find(name_);
    base::system_error::throw_if(
        it == items_.end(), "unknown type: '" + name_ + "'");

    selected_ =
        it->second->AsyncStart(callback, std::forward<Args>(args)...);
  }

 private:
  class Holder {
   public:
    virtual ~Holder() {}

    virtual void AddToProgramOptions(
        boost::program_options::options_description*,
        const std::string& prefix) = 0;
    virtual std::unique_ptr<Base> AsyncStart(
        mjlib::io::ErrorCallback, Args...) = 0;
  };

  template <typename T>
  class Concrete : public Holder {
   public:
    ~Concrete() override {}

    void AddToProgramOptions(
        boost::program_options::options_description* options,
        const std::string& prefix) override {
      base::ProgramOptionsArchive(options, prefix).Accept(&options_);
    }

    std::unique_ptr<Base> AsyncStart(io::ErrorCallback callback,
                                     Args... args) override {
      auto result = std::make_unique<T>(
          options_, std::forward<Args>(args)...);
      result->AsyncStart(callback);
      return result;
    }

    typename T::Options options_;
  };

  const std::string selector_name_;
  std::unique_ptr<Base> selected_;

  std::string name_;
  std::map<std::string, std::unique_ptr<Holder>> items_;
};

}
}
