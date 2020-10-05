// Copyright 2014-2019 Josh Pieper, jjp@pobox.com.
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

#include "mjlib/base/time_conversions.h"

namespace mjlib {
namespace base {
namespace {
const boost::posix_time::ptime kEpoch(
    boost::gregorian::date(1970, boost::gregorian::Jan, 1));
}

boost::posix_time::time_duration ConvertSecondsToDuration(double time_s) {
  if (time_s == -std::numeric_limits<double>::infinity()) {
    return boost::posix_time::neg_infin;
  } else if (time_s == std::numeric_limits<double>::infinity()) {
    return boost::posix_time::pos_infin;
  } else if (!std::isfinite(time_s)) {
    return boost::posix_time::time_duration(boost::posix_time::not_a_date_time);
  }

  const int64_t int_time = static_cast<int64_t>(time_s);
  const int64_t counts =
      static_cast<int64_t>(
          (time_s - static_cast<double>(int_time)) *
          boost::posix_time::time_duration::ticks_per_second());
  return boost::posix_time::seconds(static_cast<int>(time_s)) +
      boost::posix_time::time_duration(0, 0, 0, counts);
}

boost::posix_time::time_duration ConvertMicrosecondsToDuration(int64_t time_us) {
  if (time_us == std::numeric_limits<int64_t>::min()) {
    return boost::posix_time::neg_infin;
  } else if (time_us == std::numeric_limits<int64_t>::max()) {
    return boost::posix_time::pos_infin;
  } else if (time_us == (std::numeric_limits<int64_t>::min() + 1)) {
    return boost::posix_time::not_a_date_time;
  }
  const int64_t int_time = time_us / 1000000;
  const int64_t counts =
      (time_us - (int_time * 1000000)) *
      (boost::posix_time::time_duration::ticks_per_second() / 1000000);
  return boost::posix_time::seconds(int_time) +
      boost::posix_time::time_duration(0, 0, 0, counts);
}

double ConvertDurationToSeconds(boost::posix_time::time_duration time) {
  if (time.is_pos_infinity()) {
    return std::numeric_limits<double>::infinity();
  } else if (time.is_neg_infinity()) {
    return -std::numeric_limits<double>::infinity();
  } else if (time.is_special()) {
    return std::numeric_limits<double>::signaling_NaN();
  }

  return time.total_microseconds() / 1e6;
}

int64_t ConvertDurationToMicroseconds(boost::posix_time::time_duration time) {
  if (time.is_pos_infinity()) {
    return std::numeric_limits<int64_t>::max();
  } else if (time.is_neg_infinity()) {
    return std::numeric_limits<int64_t>::min();
  } else if (time.is_special()) {
    return std::numeric_limits<int64_t>::min() + 1;
  }

  return time.total_microseconds();
}


boost::posix_time::ptime ConvertEpochSecondsToPtime(double time_s) {
  if (time_s == -std::numeric_limits<double>::infinity()) {
    return boost::posix_time::neg_infin;
  } else if (time_s == std::numeric_limits<double>::infinity()) {
    return boost::posix_time::pos_infin;
  } else if (!std::isfinite(time_s)) {
    return boost::posix_time::ptime();
  }

  return ConvertEpochMicrosecondsToPtime(time_s * 1e6);
}

boost::posix_time::ptime ConvertEpochMicrosecondsToPtime(int64_t value) {
  if (value == std::numeric_limits<int64_t>::max()) {
    return boost::posix_time::pos_infin;
  } else if (value == std::numeric_limits<int64_t>::min()) {
    return boost::posix_time::neg_infin;
  } else if (value == std::numeric_limits<int64_t>::min() + 1) {
    return boost::posix_time::ptime();
  }

  return kEpoch + boost::posix_time::time_duration(
      0, 0, 0,
      value *
      (boost::posix_time::time_duration::ticks_per_second() / 1000000));
}

double ConvertPtimeToEpochSeconds(boost::posix_time::ptime time) {
  if (time.is_pos_infinity()) {
    return std::numeric_limits<double>::infinity();
  } else if (time.is_neg_infinity()) {
    return -std::numeric_limits<double>::infinity();
  } else if (time.is_special()) {
    return std::numeric_limits<double>::signaling_NaN();
  }

  return ConvertPtimeToEpochMicroseconds(time) / 1e6;
}

int64_t ConvertPtimeToEpochMicroseconds(boost::posix_time::ptime time) {
  if (time.is_pos_infinity()) {
    return std::numeric_limits<int64_t>::max();
  } else if (time.is_neg_infinity()) {
    return std::numeric_limits<int64_t>::min();
  } else if (time.is_special()) {
    return std::numeric_limits<int64_t>::min() + 1;
  }

  return (time - kEpoch).total_microseconds();
}

}
}
