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

#ifndef REALM_BSON_HPP
#define REALM_BSON_HPP

#include "util/bson/indexed_map.hpp"
#include "util/bson/regular_expression.hpp"
#include "util/bson/min_key.hpp"
#include "util/bson/max_key.hpp"
#include "util/bson/null.hpp"

#include <realm/binary_data.hpp>
#include <realm/timestamp.hpp>
#include <realm/decimal128.hpp>
#include <realm/object_id.hpp>
#include <ostream>
#include <variant>

namespace realm {
namespace bson {

namespace _impl {
/// A variant of the allowed Bson types.
template<class T>
using Var = std::variant<
    Null,
    int32_t,
    int64_t,
    bool,
    float,
    double,
    time_t,
    std::string,
    std::vector<char>,
    Timestamp,
    Decimal128,
    ObjectId,
    RegularExpression,
    MinKey,
    MaxKey,
    IndexedMap<T>,
    std::vector<T>
>;

// tie the knot
template <template<class> class K>
struct Fix : K<Fix<K>>
{
   using K<Fix>::K;
};
} // namespace _impl

using Bson = _impl::Fix<_impl::Var>;
using BsonDocument = IndexedMap<Bson>;
using BsonArray = std::vector<Bson>;

std::ostream& operator<<(std::ostream& out, const Bson& b);

Bson parse(const std::string& json);

} // namespace bson
} // namespace realm
            
#endif // REALM_BSON_HPP
