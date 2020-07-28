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

#include "mjlib/io/selector.h"

#include <sstream>

#include <boost/asio/io_context.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/clipp.h"
#include "mjlib/base/collapse_whitespace.h"
#include "mjlib/base/fail.h"
#include "mjlib/base/visitor.h"

using mjlib::io::Selector;

namespace {
class Resource {};

class Base {
 public:
  virtual ~Base() {}

  virtual int type() = 0;
  virtual int value() = 0;
};

class Class1 : public Base {
 public:
  struct Options {
    int value1 = 0;

    template <typename Archive>
    void Serialize(Archive* a) {
      a->Visit(MJ_NVP(value1));
    }
  };

  Class1(const boost::asio::any_io_executor&, const Options& options, Resource*)
      : options_(options) {}
  ~Class1() override {}

  int type() override { return 1; }
  int value() override { return options_.value1; }

  void AsyncStart(mjlib::io::ErrorCallback cbk) {
    started_ = true;
    cbk(mjlib::base::error_code());
  }

  bool started_ = false;

 private:
  Options options_;
};

class Class2 : public Base {
 public:
  struct Options {
    int value2 = 0;

    template <typename Archive>
    void Serialize(Archive* a) {
      a->Visit(MJ_NVP(value2));
    }
  };

  Class2(const boost::asio::any_io_executor&, const Options& options, Resource*)
      : options_(options) {}
  ~Class2() override {}

  int type() override { return 2; }
  int value() override { return options_.value2; }

  void AsyncStart(mjlib::io::ErrorCallback cbk) {
    started_ = true;
    cbk(mjlib::base::error_code());
  }

  bool started_ = false;

 private:
  Options options_;
};
}

BOOST_AUTO_TEST_CASE(BasicSelectorTest) {
  boost::asio::io_context context;
  Selector<Base, Resource*> dut{context.get_executor(), "mode"};
  dut.Register<Class1>("class1");
  dut.Register<Class2>("class2");

  dut.set_default("class1");
  auto group = dut.program_options();
  std::ostringstream ostr;
  mjlib::base::EmitUsage(ostr, group);

  std::string expected = "Usage:\nmode <arg> *class1*/class2\nclass1.value1 <arg>\nclass2.value2 <arg>\n";
  BOOST_TEST(mjlib::base::CollapseWhitespace(ostr.str()) == expected);

  const bool parse_fail = !clipp::parse({"mode", "class2", "class2.value2", "97"}, group);
  BOOST_TEST_REQUIRE(!parse_fail);

  Resource resource;
  int done = 0;
  dut.AsyncStart([&](const mjlib::base::error_code& ec) {
      mjlib::base::FailIf(ec);
      done++;
    }, &resource);

  BOOST_TEST(done == 1);
  BOOST_TEST(dut.selected() != nullptr);

  Base* const base = dut.selected();
  Class2* const class2 = dynamic_cast<Class2*>(base);
  BOOST_TEST(class2 != nullptr);
  BOOST_TEST(class2->started_ == true);
  BOOST_TEST(class2->value() == 97);
}

namespace {
class EmptyOptions : public Base {
 public:
  struct Options {
    template <typename Archive>
    void Serialize(Archive*) {}
  };

  EmptyOptions(const boost::asio::any_io_executor&, const Options&, Resource*) {}
  ~EmptyOptions() override {}
  int type() override { return 3; }
  int value() override { return 10; }

  void AsyncStart(mjlib::io::ErrorCallback cbk) {
    cbk(mjlib::base::error_code());
  }
};
}

BOOST_AUTO_TEST_CASE(SelectorEmptyOptions) {
  // This test doesn't catch any particular failure, but it is a use
  // case I want to support.
  boost::asio::io_context context;
  Selector<Base, Resource*> dut{context.get_executor(), "mode"};
  dut.Register<Class1>("class1");
  dut.Register<EmptyOptions>("empty");
  dut.Register<Class2>("fclass2");
  std::ostringstream actual;
  clipp::group g;
  g.push_back(clipp::with_prefix("--prefix.", dut.program_options()));
  mjlib::base::EmitUsage(actual, g);
  std::string expected = R"XX(Usage:
  --prefix.mode <arg>                   class1/empty/fclass2
  --prefix.class1.value1 <arg>
  --prefix.fclass2.value2 <arg>
)XX";
  BOOST_TEST(actual.str() == expected);

  clipp::parse({"--prefix.class1.value1", "2"}, g);
}
