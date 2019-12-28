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

#include "mjlib/io/selector.h"

#include <sstream>

#include <boost/asio/io_context.hpp>
#include <boost/program_options.hpp>
#include <boost/test/auto_unit_test.hpp>

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

  Class1(const boost::asio::executor&, const Options& options, Resource*)
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

  Class2(const boost::asio::executor&, const Options& options, Resource*)
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

std::string StripSpaces(const std::string& in) {
  std::stringstream result;
  for (char c : in) {
    if (c != ' ') { result.write(&c, 1); }
  }
  return result.str();
}

void SetOption(
    boost::program_options::options_description* source,
    const std::string& key,
    const std::string& value) {
  auto semantic = source->find(key, false).semantic();
  boost::any any_value;
  semantic->parse(any_value, std::vector<std::string>({value}), true);
  semantic->notify(any_value);
}

}

BOOST_AUTO_TEST_CASE(BasicSelectorTest) {
  boost::asio::io_context context;
  Selector<Base, Resource*> dut{context.get_executor(), "mode"};
  dut.Register<Class1>("class1");
  dut.Register<Class2>("class2");

  boost::program_options::options_description options;
  dut.set_default("class1");
  dut.AddToProgramOptions(&options, "test");
  std::ostringstream ostr;
  ostr << options;
  std::string expected = "--modearg*class1*/class2\n--test.class1.value1\n--test.class2.value2\n";
  BOOST_TEST(StripSpaces(ostr.str()) == expected);

  SetOption(&options, "mode", "class2");
  SetOption(&options, "test.class2.value2", "97");
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
