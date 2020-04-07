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

#ifndef REALM_BSON_PARSER_HPP
#define REALM_BSON_PARSER_HPP

#include <json.hpp>
#include <variant>
#include <stack>
#include "util/bson/bson.hpp"

namespace realm {
namespace bson {
namespace detail {

using namespace nlohmann;

// Parser for extended json. Using nlohmann's SAX interface,
// translate each incoming instruction to it's extended
// json equivalent, constructing extended json from plain json.
class Parser : public nlohmann::json_sax<json> {
public:
    using number_integer_t = typename json::number_integer_t;
    using number_unsigned_t = typename json::number_unsigned_t;
    using number_float_t = typename json::number_float_t;
    using string_t = typename json::string_t;

    enum class State {
        StartDocument,
        EndDocument,
        StartArray,
        EndArray,
        NumberInt,
        NumberLong,
        NumberDouble,
        NumberDecimal,
        Binary,
        BinaryBase64,
        BinarySubType,
        Date,
        Timestamp,
        TimestampT,
        TimestampI,
        ObjectId,
        String,
        MaxKey,
        MinKey,
        RegularExpression,
        RegularExpressionPattern,
        RegularExpressionOptions,
        JsonKey,
        Skip
    };

    Parser();

    bool null() override;

    bool boolean(bool val) override;

    bool number_integer(number_integer_t) override;

    bool number_unsigned(number_unsigned_t val) override;

    bool number_float(number_float_t, const string_t&) override;

    bool string(string_t& val) override;

    bool key(string_t& val) override;

    bool start_object(std::size_t) override;

    bool end_object() override;

    bool start_array(std::size_t) override;

    bool end_array() override;

    bool parse_error(std::size_t,
                     const std::string&,
                     const nlohmann::detail::exception& ex) override;

    Bson parse(const std::string& json);

protected:
    struct Instruction {
        State type;
        std::string key;
    };

    /// Convenience class that overloads similar collection functionality
    class BsonContainer : public std::variant<BsonDocument, BsonArray> {
    public:
        using base = std::variant<BsonDocument, BsonArray>;
        using base::base;
        using base::operator=;

        void push_back(std::pair<std::string, Bson> value) {
            if (std::holds_alternative<BsonDocument>(*this)) {
                std::get<BsonDocument>(*this)[value.first] = value.second;
            } else {
                std::get<BsonArray>(*this).push_back(value.second);
            }
        }

        std::pair<std::string, Bson> back()
        {
            if (std::holds_alternative<BsonDocument>(*this)) {
                auto pair = *std::get<BsonDocument>(*this).back();

                return pair;
            } else {
                return {"", std::get<BsonArray>(*this).back()};
            }
        }

        void pop_back()
        {
            if (std::holds_alternative<BsonDocument>(*this)) {
                std::get<BsonDocument>(*this).pop_back();
            } else {
                std::get<BsonArray>(*this).pop_back();
            }
        }

        size_t size()
        {
            if (std::holds_alternative<BsonDocument>(*this)) {
                return std::get<BsonDocument>(*this).size();
            } else {
                return std::get<BsonArray>(*this).size();
            }
        }
    };

    std::stack<BsonContainer> m_marks;
    std::stack<Instruction> m_instructions;
};

struct BsonError : public std::runtime_error {
    BsonError(std::string meoutage) : std::runtime_error(meoutage)
    {
    }
};

} // namespace detail
} // namespace bson
} // namespace realm

#endif /* REALM_BSON_PARSER_HPP */
