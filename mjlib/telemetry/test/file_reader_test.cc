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
#include "mjlib/telemetry/error.h"
#include "mjlib/telemetry/file_writer.h"

using namespace mjlib;

namespace {
class TemporaryContents : public base::TemporaryFile {
 public:
  template <size_t N>
  TemporaryContents(const char (&data)[N])
      : TemporaryContents(std::string_view{
          &data[0], sizeof(data) - 1}) {}

  TemporaryContents(const std::vector<uint8_t>& data)
      : TemporaryContents(std::string_view{
          reinterpret_cast<const char*>(&data[0]), data.size()}) {}

  TemporaryContents(std::string_view data) {
    std::ofstream of(native());
    base::system_error::throw_if(!of.is_open());
    of.write(data.data(), data.size());
  }
};

using DUT = telemetry::FileReader;

}  // namespace

BOOST_AUTO_TEST_CASE(EmptyFileReaderTest) {
  std::vector<uint8_t> empty_log{
    0x54, 0x4c, 0x4f, 0x47, 0x30, 0x30, 0x30, 0x33, 0x00, };
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
    0x54, 0x4c, 0x4f, 0x47, 0x30, 0x30, 0x30, 0x33, 0x00,
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
    0x54, 0x4c, 0x4f, 0x47, 0x30, 0x30, 0x30, 0x33, 0x00,
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
    BOOST_TEST(items[0].index == 20);
    BOOST_TEST(items[0].data == std::string("\x05\x04\x03\x02"));
    BOOST_TEST(items[1].index == 28);
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
    0x54, 0x4c, 0x4f, 0x47, 0x30, 0x30, 0x30, 0x33, 0x00,
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

BOOST_AUTO_TEST_CASE(IndexRecords) {
  TemporaryContents contents(
    "TLOG0003\x00"  // file header
    "\x01\x08"  // BlockType - Schema, size=17
    "\x01\x00"  // id=1, flags = 0
        "\x04test"  // name
          "\x0a"  // schema
      "\x02\x13"  // BlockType = Data, size=19
      "\x01\x03"  // id=1, flags= (previous_offset|timestamp)
        "\x00"  // previous offset
        "\x00\x20\x07\xcd\x74\xa0\x05\x00"  // timestamp
        "\x07""estdata"

      "\x03\x1f"  // BlockType = Index, size=31
      "\x00\x01"  // flags=0 nelements=1
        "\x01" // id
          "\x09\x00\x00\x00\x00\x00\x00\x00"  // schema location
          "\x13\x00\x00\x00\x00\x00\x00\x00"  // final record
        "\x21\x00\x00\x00"
        "TLOGIDEX"
  );

  DUT dut{contents.native()};
  const auto final_item = dut.final_item();
  BOOST_TEST(final_item == 19);
  const auto records = dut.records();
  BOOST_TEST(records.size() == 1);
  BOOST_TEST(dut.has_index());
}

namespace {
template <typename Array>
std::string MakeString(const Array& array) {
  return std::string(array, sizeof(array) - 1);
}
}  // namespace

BOOST_AUTO_TEST_CASE(ChecksumMatch) {
  std::string good_log_file = MakeString(
    "TLOG0003\x00"  // file header
    "\x01\x08"  // BlockType - Schema, size=17
    "\x01\x00"  // id=1, flags = 0
        "\x04test"  // name
          "\x0a"  // schema
      "\x02\x17"  // BlockType = Data, size=19
      "\x01\x07"  // id=1, flags= (previous_offset|timestamp|checksum)
        "\x00"  // previous offset
        "\x00\x20\x07\xcd\x74\xa0\x05\x00"  // timestamp
        "\xe0\xe5\x00\x6c"  // checksum
      "\x07""estdata"
                                         );
  {
    TemporaryContents contents(good_log_file);

    DUT dut{contents.native()};
    std::vector<DUT::Item> items;
    for (const auto& item : dut.items()) { items.push_back(item); }
    BOOST_TEST(items.size() == 1);
    BOOST_TEST(items[0].data == "\x07""estdata");
  }

  auto bad_log_file = good_log_file;
  bad_log_file.back() = 0x01;
  {
    TemporaryContents contents(bad_log_file);

    DUT dut{contents.native()};
    auto consume_all = [&]() {
      for (const auto& item : dut.items()) { (void) item; }
    };

    auto is_mismatch = [](const base::system_error& error) {
      return error.code() == telemetry::errc::kDataChecksumMismatch;
    };
    BOOST_CHECK_EXCEPTION(consume_all(), base::system_error, is_mismatch);
  }

  {
    // Disable checksum validation.
    TemporaryContents contents(bad_log_file);

    DUT dut{contents.native(), []() {
        DUT::Options options;
        options.verify_checksums = false;
        return options;
      }()};
    std::vector<DUT::Item> items;
    for (const auto& item : dut.items()) { items.push_back(item); }
    BOOST_TEST(items.size() == 1);
    BOOST_TEST(items[0].data == "\x07""estdat\x01");
  }
}

 BOOST_AUTO_TEST_CASE(DecompressionTest) {
   std::string log_file = MakeString(
    "TLOG0003\x00"  // file header
    "\x01\x08"  // BlockType - Schema, size
    "\x01\x00"  // id=1, flags = 0
        "\x04test"  // name
    "\x0a"  // schema

      "\x02\x43"  // BlockType = Data, size
      "\x01\x17"  // id=1, flags= (previous_offset|timestamp|checksum|zstd)
        "\x00"  // previous offset
        "\x00\x20\x07\xcd\x74\xa0\x05\x00"  // timestamp
        "\x1a\x6e\x76\x9e"  // crc32

        "\x80\x08\x00\x61\xfe\x01\x00\xfe"
        "\x01\x00\xfe\x01\x00\xfe\x01\x00"
        "\xfe\x01\x00\xfe\x01\x00\xfe\x01"
        "\x00\xfe\x01\x00\xfe\x01\x00\xfe"
        "\x01\x00\xfe\x01\x00\xfe\x01\x00"
        "\xfe\x01\x00\xfe\x01\x00\xfe\x01"
        "\x00\xfa\x01\x00"

      "\x03\x1f"  // BlockType = Index, size=31
      "\x00\x01"  // flags=0 nelements=1
        "\x01" // id
          "\x09\x00\x00\x00\x00\x00\x00\x00"  // schema location
          "\x13\x00\x00\x00\x00\x00\x00\x00"  // final record
        "\x21\x00\x00\x00"
        "TLOGIDEX"
  );

  {
    TemporaryContents contents(log_file);

    DUT dut{contents.native()};
    std::vector<DUT::Item> items;
    for (const auto& item : dut.items()) { items.push_back(item); }
    BOOST_TEST(items.size() == 1);
    BOOST_TEST(items[0].data == std::string(1024, 'a'));
  }
}

BOOST_AUTO_TEST_CASE(SeekTest) {
  base::TemporaryFile tempfile;

  const boost::posix_time::ptime start =
      boost::posix_time::time_from_string("2020-03-10 00:00:00");

  // For this test, we will programmatically generate a log file with
  // known properties.
  {
    telemetry::FileWriter writer{tempfile.native()};

    const auto id1 = writer.AllocateIdentifier("test1");
    const auto id2 = writer.AllocateIdentifier("test2");
    const auto id3 = writer.AllocateIdentifier("test3");
    writer.WriteSchema(id1, "\x0a");  // string
    writer.WriteSchema(id2, "\x0a");  // string
    writer.WriteSchema(id3, "\x0a");  // string

    auto timestamp = start;
    for (int i = 1; i < 10000; i++) {
      const auto ts_str =
          boost::lexical_cast<std::string>(timestamp) +
          std::string(100, ' ');
      writer.WriteData(timestamp, id1, "id1: " + ts_str);
      if ((i % 20) == 0) {
        writer.WriteData(timestamp, id2, "id2: " + ts_str);
      }
      if ((i % 200) == 0) {
        writer.WriteData(timestamp, id3, "id3: " + ts_str);
      }
      timestamp += boost::posix_time::seconds(1);
    }
  }

  DUT dut{tempfile.native()};
  BOOST_TEST(dut.has_index());
  BOOST_TEST(dut.records().size() == 3);

  {
    // There is only one record at the very first timestamp.
    const auto result = dut.Seek(start);
    BOOST_TEST_REQUIRE(result.size() == 1);
    BOOST_TEST((*result.begin()).first->name == "test1");
    // If we read that record, it should match the timestamp in
    // question.
    auto items = dut.items(
        [&]() {
          DUT::ItemsOptions options;
          options.start = result.begin()->second;
          return options;
        }());
    BOOST_TEST((items.begin() != items.end()));
    BOOST_TEST((*items.begin()).timestamp == start);
  }

  {
    // And there are none before.
    const auto result = dut.Seek(start - boost::posix_time::milliseconds(1));
    BOOST_TEST(result.size() == 0);
  }

  {
    // Past the end of the log should return the final record among
    // its items.
    const auto result = dut.Seek(start + boost::posix_time::seconds(20000));
    BOOST_TEST_REQUIRE(result.size() == 3);
    const auto last = [&]() {
      DUT::Index running = 0;
      for (const auto& pair : result) {
        running = std::max(running, pair.second);
      }
      return running;
    }();
    BOOST_TEST(last == dut.final_item());
  }

  {
    // And do something about a quarter of the way through.
    const auto query = start + boost::posix_time::seconds(2500);
    const auto result = dut.Seek(query);
    BOOST_TEST_REQUIRE(result.size() == 3);
    auto items = dut.items(
        [&]() {
          DUT::ItemsOptions options;
          options.start = result.begin()->second;
          return options;
        }());
    BOOST_TEST((items.begin() != items.end()));
    BOOST_TEST(
        std::abs(base::ConvertDurationToSeconds(
                     (*items.begin()).timestamp - query)) < 200.0);
  }
}
