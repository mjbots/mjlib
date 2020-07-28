// Copyright 2019-2020 Josh Pieper, jjp@pobox.com.
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

#include <boost/asio/any_io_executor.hpp>

#include "mjlib/base/clipp_archive.h"
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
  Selector(const boost::asio::any_io_executor& executor,
           const std::string& selector_name)
      : executor_(executor),
        selector_name_(selector_name) {}

  template <typename Derived>
  void Register(
      const std::string& name,
      const typename Derived::Options& default_options =
      typename Derived::Options()) {
    auto derived = std::make_unique<Concrete<Derived>>(default_options);
    items_.insert(std::make_pair(name, std::move(derived)));
  }

  clipp::group program_options() {
    clipp::group result;

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

    result.push_back(
        (clipp::option(selector_name_) & clipp::value("arg", name_)) %
        help_text.str()
    );

    for (auto& item_pair : items_) {
      result.merge(clipp::with_prefix(
                       (item_pair.first + "."),
                       item_pair.second->program_options()));
    }

    return result;
  }

  void set_default(const std::string& name) { name_ = name; }

  Base* selected() { return selected_.get(); }
  const Base* selected() const { return selected_.get(); }

  void AsyncStart(mjlib::io::ErrorCallback callback, Args... args) {
    const auto it = items_.find(name_);
    base::system_error::throw_if(
        it == items_.end(), "unknown type: '" + name_ + "'");

    selected_ =
        it->second->AsyncStart(executor_, std::move(callback),
                               std::forward<Args>(args)...);
  }

 private:
  class Holder {
   public:
    virtual ~Holder() {}

    virtual clipp::group program_options() = 0;
    virtual std::unique_ptr<Base> AsyncStart(
        const boost::asio::any_io_executor&,
        mjlib::io::ErrorCallback, Args...) = 0;
  };

  template <typename T>
  class Concrete : public Holder {
   public:
    Concrete(const typename T::Options& options) : options_(options) {}
    ~Concrete() override {}

    clipp::group program_options() override {
      return base::ClippArchive().Accept(&options_).release();
    }

    std::unique_ptr<Base> AsyncStart(
        const boost::asio::any_io_executor& executor,
        io::ErrorCallback callback,
        Args... args) override {
      auto result = std::make_unique<T>(
          executor, options_, std::forward<Args>(args)...);
      result->AsyncStart(std::move(callback));
      return result;
    }

    typename T::Options options_;
  };

  boost::asio::any_io_executor executor_;
  const std::string selector_name_;
  std::unique_ptr<Base> selected_;

  std::string name_;
  std::map<std::string, std::unique_ptr<Holder>> items_;
};

}
}
