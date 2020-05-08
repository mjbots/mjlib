// Copyright 2020 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/telemetry/file_reader.h"

#include <fstream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/temporary_file.h"
#include "mjlib/base/system_error.h"

using namespace mjlib;

namespace {
class TemporaryContents : public base::TemporaryFile {
 public:
  template <typename Data>
  TemporaryContents(const Data& data) {
    std::ofstream of(native());
    base::system_error::throw_if(!of.is_open());
    of.write(reinterpret_cast<const char*>(&*std::begin(data)),
             std::end(data) - std::begin(data));
  }
};

using DUT = telemetry::FileReader;

}  // namespace

BOOST_AUTO_TEST_CASE(EmptyFileReaderTest) {
  std::vector<uint8_t> empty_log{
    0x54, 0x4c, 0x4f, 0x47, 0x30, 0x30, 0x30, 0x33, };
  TemporaryContents contents{empty_log};

  DUT dut{contents.native()};
  BOOST_TEST(dut.records().empty());
  int count = 0;
  for (const auto& item : dut.items()) {
    (void)item;
    count++;
  }
  BOOST_TEST(count == 0);
}

BOOST_AUTO_TEST_CASE(SchemaOnly) {
  std::vector<uint8_t> log_data{
    0x54, 0x4c, 0x4f, 0x47, 0x30, 0x30, 0x30, 0x33,
        0x01, 0x09,  // BlockType=Schema, size
          0x02,  // identifier=2
          0x00,  // flags=0
          0x04, 0x74, 0x65, 0x73, 0x74,  // name="test"
          0x03, 0x04,  // fixedint(4)
  };
  TemporaryContents contents{log_data};

  {
    DUT dut{contents.native()};
    const auto records = dut.records();
    BOOST_TEST_REQUIRE(records.size() == 1);
    const auto& r = *records[0];
    BOOST_TEST(r.identifier == 2);
    BOOST_TEST(r.name == "test");
    BOOST_TEST(r.raw_schema == std::string("\x03\x04"));
  }
  {
    DUT dut{contents.native()};
    int count = 0;
    for (const auto& item : dut.items()) {
      (void)item;
      count++;
    }
    BOOST_TEST(count == 0);
    BOOST_TEST(dut.records().size() == 1);
  }
}

BOOST_AUTO_TEST_CASE(BasicLog) {
  std::vector<uint8_t> log_data{
    0x54, 0x4c, 0x4f, 0x47, 0x30, 0x30, 0x30, 0x33,
        0x01, 0x09,  // BlockType=Schema, size
          0x02,  // identifier=2
          0x00,  // flags=0
          0x04, 0x74, 0x65, 0x73, 0x74,  // name="test"
          0x03, 0x04,  // fixedint(4)
        0x02, 0x06,  // BlockType=Data, size
          0x02, 0x00,  // identifier=2, flags=0
          0x05, 0x04, 0x03, 0x02,  // 0x02030405
        0x02, 0x06,  // BlockType=Data, size
          0x02, 0x00,  // identifier=2, flags=0
          0x06, 0x05, 0x04, 0x03,  // 0x03040506
  };
  TemporaryContents contents{log_data};

  {
    DUT dut{contents.native()};
    std::vector<DUT::Item> items;
    for (const auto& item : dut.items()) { items.push_back(item); }
    BOOST_TEST(items.size() == 2);
    BOOST_TEST(items[0].record->name == "test");
    BOOST_TEST(items[0].record->identifier == 2);
    BOOST_TEST(items[0].record == items[1].record);
    BOOST_TEST(items[0].index == 19);
    BOOST_TEST(items[0].data == std::string("\x05\x04\x03\x02"));
    BOOST_TEST(items[1].index == 27);
    BOOST_TEST(items[1].data == std::string("\x06\x05\x04\x03"));
  }
  {
    DUT dut{contents.native()};
    const auto records = dut.records();
    BOOST_TEST_REQUIRE(records.size() == 1);
    BOOST_TEST(records[0]->name == "test");
  }
}

BOOST_AUTO_TEST_CASE(MultipleRecords) {
  std::vector<uint8_t> log_data{
    0x54, 0x4c, 0x4f, 0x47, 0x30, 0x30, 0x30, 0x33,
        0x01, 0x09,  // BlockType=Schema, size
          0x02,  // identifier=2
          0x00,  // flags=0
          0x04, 0x74, 0x65, 0x73, 0x74,  // name="test"
          0x03, 0x04,  // fixedint(4)
        0x02, 0x0e,  // BlockType=Data, size
          0x02, 0x02,  // identifier=2, flags=timestamp
          0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
          0x05, 0x04, 0x03, 0x02,  // 0x02030405
        0x01, 0x09,  // BlockType=Schema, size
          0x01,  // identifier=1
          0x00,  // flags=0
          0x04, 0x74, 0x65, 0x73, 0x32,  // name="tes2"
          0x04, 0x02,  // fixeduint(2)
        0x02, 0x0c,  // BlockType=Data, size
          0x01, 0x02,  // identifier=1, flags=timestamp
          0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00,
          0x06, 0x05,  // 0x0506
        0x02, 0x0e,  // BlockType=Data, size
          0x02, 0x02,  // identifier=2, flags=timestamp
          0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00,
          0x06, 0x05, 0x04, 0x03,  // 0x03040506
        0x02, 0x0c,  // BlockType=Data, size
          0x01, 0x02,  // identifier=1, flags=timestamp
          0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00,
          0x07, 0x05,  // 0x0506
        0x02, 0x0c,  // BlockType=Data, size
          0x01, 0x02,  // identifier=1, flags=timestamp
          0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00,
          0x08, 0x05,  // 0x0506
        };

  TemporaryContents contents{log_data};
  DUT dut{contents.native()};

  std::vector<DUT::Item> items;
  for (const auto& item : dut.items()) { items.push_back(item); }
  BOOST_TEST(items.size() == 5);
  const auto records = dut.records();
  BOOST_TEST(records.size() == 2);
  BOOST_TEST(records[0]->name == "test");
  BOOST_TEST(records[0]->identifier == 2);
  BOOST_TEST(records[0]->schema->root()->name == "test");
  BOOST_TEST((records[0]->schema->root()->type ==
              telemetry::Format::Type::kFixedInt));
  BOOST_TEST(records[1]->name == "tes2");
  BOOST_TEST(records[1]->identifier == 1);
  BOOST_TEST(items[0].data == std::string("\x05\x04\x03\x02"));
  BOOST_TEST(base::ConvertPtimeToEpochMicroseconds(items[0].timestamp) ==
             0x0000000010000000);
  BOOST_TEST(items[1].data == std::string("\x06\x05"));
  BOOST_TEST(base::ConvertPtimeToEpochMicroseconds(items[1].timestamp) ==
             0x0000000011000000);
}