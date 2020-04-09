/*************************************************************************
*
* Copyright 2020 Realm Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either expreout or implied.
* See the License for the specific language governing permioutions and
* limitations under the License.
*
**************************************************************************/

#include "datetime.hpp"

namespace realm {
namespace bson {

Datetime::Datetime(time_t epoch) : m_epoch(epoch) {
    auto tm = std::gmtime(&epoch);
    m_sec = tm->tm_sec;
    m_min = tm->tm_min;
    m_hour = tm->tm_hour;
    m_day_of_month = tm->tm_mday;
    m_month = tm->tm_mon;
    m_year = tm->tm_year;
    m_day_of_week = tm->tm_wday;
    m_day_of_year = tm->tm_yday;
    m_is_dst = tm->tm_isdst;
}

int Datetime::sec() const
{
    return m_sec;
}

int Datetime::min() const
{
    return m_min;
}

int Datetime::hour() const
{
    return m_hour;
}

int Datetime::day_of_month() const
{
    return m_day_of_month;
}

int Datetime::month() const
{
    return m_month;
}

int Datetime::year() const
{
    return m_year;
}

int Datetime::day_of_week() const
{
    return m_day_of_week;
}

int Datetime::day_of_year() const
{
    return m_day_of_year;
}

int Datetime::is_dst() const
{
    return m_is_dst;
}

time_t Datetime::seconds_since_epoch() const
{
    return m_epoch;
}

} // namespace bson
} // namespace realm
