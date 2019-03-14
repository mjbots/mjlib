// Copyright 2015-2019 Josh Pieper, jjp@pobox.com.  All rights reserved.
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

#include <deque>
#include <memory>

#include <boost/asio/io_service.hpp>
#include <boost/noncopyable.hpp>

namespace mjlib {
namespace io {

/// Ensure that only one command is outstanding at a single time.
/// This is effectively a mutex in the asio callback world.
class ExclusiveCommand : boost::noncopyable {
 public:
  ExclusiveCommand(boost::asio::io_service& service) : service_(service) {}

  /// Invoke @p command when the resource is idle.
  ///
  /// Invoke @p handler will be passed to @p command when it is
  /// invoked.  This handler must be called when the operation is
  /// complete.
  template <typename Command, typename Handler>
  void Invoke(Command command, Handler handler) {
    auto ptr = std::make_shared<Concrete<Command, Handler>>(this, command, handler);
    queued_.push_back(ptr);
    MaybeStart();
  };

  boost::asio::io_service& get_io_service() { return service_; }

 private:
  void ItemDone() {
    BOOST_ASSERT(waiting_);
    waiting_.reset();
    MaybeStart();
  }

  void MaybeStart() {
    if (waiting_) { return; }
    if (queued_.empty()) { return; }
    waiting_ = queued_.front();
    queued_.pop_front();
    service_.post([waiting=waiting_]() { waiting->Invoke(); });
  }

  class Base : boost::noncopyable {
   public:
    virtual ~Base() {}
    virtual void Invoke() = 0;
  };

  template <typename Command, typename Handler>
  class Concrete : public Base {
   public:
    Concrete(ExclusiveCommand* parent,
             const Command& command,
             const Handler& handler)
        : parent_(parent), command_(command), handler_(handler) {}

    ~Concrete() override {}

    void Invoke() override {
      command_(HandlerWrapper(this));
    }

   private:
    class HandlerWrapper {
     public:
      HandlerWrapper(Concrete* concrete) : concrete_(concrete) {}

      template <typename... Args>
      void operator()(Args&&... args) {
        concrete_->handler_(std::forward<Args>(args)...);

        BOOST_ASSERT(concrete_->parent_->waiting_);
        concrete_->parent_->get_io_service().post(
            std::bind(&ExclusiveCommand::ItemDone, concrete_->parent_));
      }

     private:
      Concrete* const concrete_;
    };

    ExclusiveCommand* const parent_;
    Command command_;
    Handler handler_;
  };

  boost::asio::io_service& service_;
  std::shared_ptr<Base> waiting_;
  std::deque<std::shared_ptr<Base>> queued_;
};
}
}
