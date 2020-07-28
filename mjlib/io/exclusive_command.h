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

#include <deque>
#include <memory>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>
#include <boost/noncopyable.hpp>

#include "mjlib/base/assert.h"

namespace mjlib {
namespace io {

/// Ensure that only one command is outstanding at a single time.
/// This is effectively a mutex in the asio callback world.
class ExclusiveCommand : boost::noncopyable {
 public:
  ExclusiveCommand(const boost::asio::any_io_executor& executor)
      : executor_(executor) {}

  class Base : boost::noncopyable {
   public:
    virtual ~Base() {}
    virtual void Invoke() = 0;
  };

  using Nonce = std::shared_ptr<Base>;

  /// Invoke @p command when the resource is idle.
  ///
  /// Invoke @p handler will be passed to @p command when it is
  /// invoked.  This handler must be called when the operation is
  /// complete.
  template <typename Command, typename Handler>
  Nonce Invoke(Command command, Handler handler) {
    auto ptr = std::make_shared<Concrete<Command, Handler>>(
        this, std::move(command), std::move(handler));
    queued_.push_back(ptr);
    MaybeStart();
    return ptr;
  };

  boost::asio::any_io_executor get_executor() { return executor_; }

  std::size_t remove(Nonce nonce) {
    auto it = std::remove(queued_.begin(), queued_.end(), nonce);
    if (it == queued_.end()) {
      return 0;
    }
    queued_.erase(it, queued_.end());
    return 1;
  }

 private:
  void ItemDone() {
    MJ_ASSERT(waiting_);
    waiting_.reset();
    MaybeStart();
  }

  void MaybeStart() {
    if (waiting_) { return; }
    if (queued_.empty()) { return; }
    waiting_ = queued_.front();
    queued_.pop_front();
    boost::asio::post(
        executor_,
        [waiting=waiting_]() { waiting->Invoke(); });
  }

  template <typename Command, typename Handler>
  class Concrete : public Base {
   public:
    Concrete(ExclusiveCommand* parent,
             Command command,
             Handler handler)
        : parent_(parent),
          command_(std::move(command)),
          handler_(std::move(handler)) {
    }

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

        MJ_ASSERT(concrete_->parent_->waiting_);
        boost::asio::post(
            concrete_->parent_->get_executor(),
            std::bind(&ExclusiveCommand::ItemDone, concrete_->parent_));
      }

     private:
      Concrete* const concrete_;
    };

    bool called_ = false;
    ExclusiveCommand* const parent_;
    Command command_;
    Handler handler_;
  };

  boost::asio::any_io_executor executor_;
  std::shared_ptr<Base> waiting_;
  std::deque<std::shared_ptr<Base>> queued_;
};
}
}
