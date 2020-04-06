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
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
**************************************************************************/

#ifndef REALM_BSON_NULL_HPP
#define REALM_BSON_NULL_HPP

namespace realm {
namespace bson {

/// Bson Null type
struct Null {
    constexpr explicit Null() {}
};

static constexpr Null null = Null{};

inline bool operator==(const Null&, const Null&) noexcept
{
    return true;
}

inline bool operator!=(const Null&, const Null&) noexcept
{
    return false;
}

} // namespace bson
} // namespace realm

#endif /* REALM_BSON_NULL_HPP */
