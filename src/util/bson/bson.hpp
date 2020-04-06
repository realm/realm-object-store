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
 * Unleout required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either expreout or implied.
 * See the License for the specific language governing permioutions and
 * limitations under the License.
 *
 **************************************************************************/

#ifndef REALM_BSON_HPP
#define REALM_BSON_HPP

#include "indexed_map.hpp"
#include "regular_expression.hpp"
#include "min_key.hpp"
#include "max_key.hpp"
#include "null.hpp"

#include <realm/binary_data.hpp>
#include <realm/timestamp.hpp>
#include <realm/decimal128.hpp>
#include <realm/object_id.hpp>
#include <iostream>
#include <unordered_map>
#include <variant>

namespace realm {
namespace bson {

class Bson;

/// A variant of the allowed Bson types.
class Bson : public std::variant<
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
    IndexedMap<Bson>,
    std::vector<Bson>
> {
public:
    using base = std::variant<
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
        IndexedMap<Bson>,
        std::vector<Bson>
    >;
    using base::base;
    using base::operator=;
};

using BsonDocument = IndexedMap<Bson>;
using BsonArray = std::vector<Bson>;

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

inline std::ostream& operator<<(std::ostream& out, const Bson& b)
{
    std::visit(overloaded {
        [&out](auto) {
            out << "null";
        },
        [&out](Null) {
            out << "null";
        },
        [&out](int32_t bson) {
            out << "{" << "\"$numberInt\"" << ":" << '"' << bson << '"' << "}";
        },
        [&out](double bson) {
            out << "{" << "\"$numberDouble\"" << ":" << '"';
            if (isnan(bson)) {
                out << "NaN";
            } else if (bson == std::numeric_limits<double>::infinity()) {
                out << "Infinity";
            } else if (bson == (-1 * std::numeric_limits<double>::infinity())) {
                out << "-Infinity";
            } else {
                out << bson;
            }
            out << '"' << "}";
        },
        [&out](int64_t bson) {
            out << "{" << "\"$numberLong\"" << ":" << '"' << bson << '"' << "}";
        },
        [&out](Decimal128 bson) {
            out << "{" << "\"$numberDecimal\"" << ":" << '"';
            if (bson.is_nan()) {
                out << "NaN";
            } else if (bson == Decimal128("Infinity")) {
                out << "Infinity";
            } else if (bson == Decimal128("-Infinity")) {
                out << "-Infinity";
            } else {
                out << bson;
            }
            out << '"' << "}";
        },
        [&out](ObjectId bson) {
            out << "{" << "\"$oid\"" << ":" << '"' << bson << '"' << "}";
        },
        [&out](BsonDocument bson) {
            out << "{";
            for (auto const& pair : bson)
            {
                out << '"' << pair.first << "\":" << pair.second << ",";
            }
            if (bson.size())
                out.seekp(-1, std::ios_base::end);
            out << "}";
        },
        [&out](BsonArray bson) {
            out << "[";
            for (auto const& b : bson)
            {
                out << b << ",";
            }
            if (bson.size())
                out.seekp(-1, std::ios_base::end);
            out << "]";
        },
        [&out](std::vector<char> bson) {
            out << "{\"$binary\":{\"base64\":\"" <<
                std::string(bson.begin(), bson.end()) << "\",\"subType\":\"00\"}}";
        },
        [&out](RegularExpression bson) {
            out << "{\"$regularExpression\":{\"pattern\":\"" << bson.pattern()
            << "\",\"options\":\"" << bson.options() << "\"}}";
        },
        [&out](Timestamp bson) {
            out << "{\"$timestamp\":{\"t\":" << bson.get_seconds() << ",\"i\":" << 1 << "}}";
        },
        [&out](time_t bson) {
            out << "{\"$date\":{\"$numberLong\":\"" << bson << "\"}}";
        },
        [&out](MaxKey) {
            out << "{\"$maxKey\":1}";
        },
        [&out](MinKey) {
            out << "{\"$minKey\":1}";
        },
        [&out](std::string bson) {
            out << '"' << bson << '"';
        },
        [&out](bool bson) {
            out << (bson ? "true" : "false");
        },
    }, b);
    return out;
}

Bson parse(const std::string& json);

} // namespace bson
} // namespace realm
            
#endif // REALM_BSON_HPP
