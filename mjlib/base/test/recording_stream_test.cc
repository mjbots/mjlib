// Copyright 2026 mjbots Robotic Systems, LLC.  info@mjbots.com
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

#include "mjlib/base/recording_stream.h"

#include <boost/test/auto_unit_test.hpp>

#include "mjlib/base/fast_stream.h"

using mjlib::base::FastIStringStream;
using mjlib::base::RecordingStream;

BOOST_AUTO_TEST_CASE(RecordingStreamIgnoreZeroOnFresh) {
  // Reachable from BinarySchemaParser when an element with
  // maybe_fixed_size == 0 (e.g. kNull) is read; the freshly-constructed
  // RecordingStream had its ignore_buffer_ still empty, and ignore(0)
  // dereferenced &ignore_buffer_[0] -- UB observable under UBSan and
  // libstdc++ debug mode.
  FastIStringStream istr("hello");
  RecordingStream recorder(istr);
  recorder.ignore(0);
  BOOST_TEST(recorder.str() == "");
  BOOST_TEST(recorder.gcount() == 0);
}

BOOST_AUTO_TEST_CASE(RecordingStreamIgnoreNonZero) {
  FastIStringStream istr("abcdef");
  RecordingStream recorder(istr);
  recorder.ignore(3);
  BOOST_TEST(recorder.str() == "abc");
  BOOST_TEST(recorder.gcount() == 3);
  // Subsequent ignore(0) on a recorder whose buffer is now non-empty
  // also should be a no-op and not change the recorded output.
  recorder.ignore(0);
  BOOST_TEST(recorder.str() == "abc");
  BOOST_TEST(recorder.gcount() == 3);
}

BOOST_AUTO_TEST_CASE(RecordingStreamIgnoreShorterThanInput) {
  // Buffer grows on first ignore, then a smaller second ignore reuses
  // the same backing storage -- this is the path that always worked
  // and is here as a positive control.
  FastIStringStream istr("0123456789");
  RecordingStream recorder(istr);
  recorder.ignore(6);
  recorder.ignore(2);
  BOOST_TEST(recorder.str() == "01234567");
}
