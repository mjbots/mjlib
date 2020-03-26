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

#pragma once

#include <cstdint>

#include <boost/date_time/posix_time/posix_time_types.hpp>

/// @file
///
/// Translates between boost::posix_time::ptime and various more
/// standardized formats.  Special values are translated as follows.
///
///                           double                       int64
/// ptime::neg_infin          -nm<double>::infinity()      nm<int64_t>::min()
/// ptime::pos_infin          nm<double>::infinity()       nm<int64_t>::max()
/// ptime::not_a_date_time    nm<double>::signaling_NaN()  nm<int64_t>::min() + 1

namespace mjlib {
namespace base {

boost::posix_time::time_duration ConvertSecondsToDuration(double time_s);
boost::posix_time::time_duration ConvertMicrosecondsToDuration(int64_t);

double ConvertDurationToSeconds(boost::posix_time::time_duration);
int64_t ConvertDurationToMicroseconds(boost::posix_time::time_duration);


boost::posix_time::ptime ConvertEpochSecondsToPtime(double time_s);
boost::posix_time::ptime ConvertEpochMicrosecondsToPtime(int64_t);

double ConvertPtimeToEpochSeconds(boost::posix_time::ptime);
int64_t ConvertPtimeToEpochMicroseconds(boost::posix_time::ptime);

}
}
