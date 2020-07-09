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

#include <boost/date_time/posix_time/posix_time.hpp>

#include "mjlib/base/clipp.h"

#include "mjlib/base/buffer_stream.h"

#include "mjlib/telemetry/emit_json.h"
#include "mjlib/telemetry/file_reader.h"

using FileReader = mjlib::telemetry::FileReader;

int main(int argc, char**argv) {
  std::vector<std::string> names;
  std::string log_filename;

  auto group = clipp::group(
      clipp::repeatable(
          (clipp::option("n", "name") & clipp::value("", names))
          % "names to include"),
      clipp::value("LOG", log_filename)
  );

  mjlib::base::ClippParse(argc, argv, group);

  FileReader file_reader(log_filename);

  FileReader::ItemsOptions options;
  options.records = names;

  for (const auto item : file_reader.items(options)) {
    mjlib::base::BufferReadStream stream(item.data);
    std::cout << "\"" << item.timestamp << "\" ";
    EmitJson(std::cout, item.record->schema->root(), stream);
    std::cout << "\n";
  }

  return 0;
}
