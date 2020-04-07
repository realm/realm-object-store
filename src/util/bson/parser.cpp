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

#include "parser.hpp"

namespace realm {
namespace bson {
namespace detail {

static constexpr const char * key_number_int                 = "$numberInt";
static constexpr const char * key_number_long                = "$numberLong";
static constexpr const char * key_number_double              = "$numberDouble";
static constexpr const char * key_number_decimal             = "$numberDecimal";
static constexpr const char * key_timestamp                  = "$timestamp";
static constexpr const char * key_timestamp_time             = "t";
static constexpr const char * key_timestamp_increment        = "i";
static constexpr const char * key_date                       = "$date";
static constexpr const char * key_object_id                  = "$oid";
static constexpr const char * key_max_key                    = "$maxKey";
static constexpr const char * key_min_key                    = "$minKey";
static constexpr const char * key_regular_expression         = "$regularExpression";
static constexpr const char * key_regular_expression_pattern = "pattern";
static constexpr const char * key_regular_expression_options = "options";
static constexpr const char * key_binary                     = "$binary";
static constexpr const char * key_binary_base64              = "base64";
static constexpr const char * key_binary_sub_type            = "subType";

static constexpr const char* state_to_string(const Parser::State& i) {
    switch (i) {
        case Parser::State::StartDocument:
            return "start_document";
        case Parser::State::EndDocument:
            return "end_document";
        case Parser::State::StartArray:
            return "start_array";
        case Parser::State::EndArray:
            return "end_array";
        case Parser::State::NumberInt:
            return "number_int";
        case Parser::State::NumberLong:
            return "number_long";
        case Parser::State::NumberDouble:
            return "number_double";
        case Parser::State::NumberDecimal:
            return "number_decimal";
        case Parser::State::Binary:
            return "binary";
        case Parser::State::BinaryBase64:
            return "binary_base64";
        case Parser::State::BinarySubType:
            return "binary_sub_type";
        case Parser::State::Date:
            return "date";
        case Parser::State::Timestamp:
            return "timestamp";
        case Parser::State::TimestampT:
            return "timestamp_t";
        case Parser::State::TimestampI:
            return "timestamp_i";
        case Parser::State::ObjectId:
            return "object_id";
        case Parser::State::String:
            return "string";
        case Parser::State::MaxKey:
            return "max_key";
        case Parser::State::MinKey:
            return "min_key";
        case Parser::State::RegularExpression:
            return "regular_expression";
        case Parser::State::RegularExpressionPattern:
            return "regular_expression_pattern";
        case Parser::State::RegularExpressionOptions:
            return "regular_expression_options";
        case Parser::State::JsonKey:
            return "json_key";
        case Parser::State::Skip:
            return "skip";
    }
}

static std::map<std::string, Parser::State> bson_type_for_key = {
    {key_number_int, Parser::State::NumberInt},
    {key_number_long, Parser::State::NumberLong},
    {key_number_double, Parser::State::NumberDouble},
    {key_number_decimal, Parser::State::NumberDecimal},
    {key_timestamp, Parser::State::Timestamp},
    {key_date, Parser::State::Date},
    {key_object_id, Parser::State::ObjectId},
    {key_max_key, Parser::State::MaxKey},
    {key_min_key, Parser::State::MinKey},
    {key_regular_expression, Parser::State::RegularExpression},
    {key_binary, Parser::State::Binary}
};

static void check_state(const Parser::State& current_state, const Parser::State& expected_state)
{
    if (current_state != expected_state)
        throw BsonError(util::format("current state '$1' is not of expected state '$2'",
                                     state_to_string(current_state),
                                     state_to_string(expected_state)));
}

Parser::Parser() {
    // use a vector container to hold any fragmented values
    m_marks.push(BsonArray());
}

/*!
 @brief a null value was read
 @return whether parsing should proceed
 */
bool Parser::null() {
    auto instruction = m_instructions.top();
    m_instructions.pop();
    check_state(instruction.type, State::JsonKey);
    m_marks.top().push_back({instruction.key, bson::null});
    return true;
}

/*!
 @brief a boolean value was read
 @param[in] val  boolean value
 @return whether parsing should proceed
 */
bool Parser::boolean(bool val) {
    auto instruction = m_instructions.top();
    m_instructions.pop();
    check_state(instruction.type, State::JsonKey);
    m_marks.top().push_back({instruction.key, val});
    return true;
}

/*!
 @brief an integer number was read
 @param[in] val  integer value
 @return whether parsing should proceed
 */
bool Parser::number_integer(number_integer_t) {
    throw BsonError(util::format("Invalid SAX instruction for canonical extended json: 'number_integer'"));
    return true;
}

/*!
 @brief an unsigned integer number was read
 @param[in] val  unsigned integer value
 @return whether parsing should proceed
 */
bool Parser::number_unsigned(number_unsigned_t val) {
    auto instruction = m_instructions.top();
    m_instructions.pop();
    switch (instruction.type) {
        case State::MaxKey:
            m_marks.top().push_back({instruction.key, max_key});
            m_instructions.push({State::Skip});
            break;
        case State::MinKey:
            m_marks.top().push_back({instruction.key, min_key});
            m_instructions.push({State::Skip});
            break;
        case State::TimestampI:
            if (m_marks.top().back().first == instruction.key) {
                auto ts = std::get<Timestamp>(m_marks.top().back().second);
                m_marks.top().pop_back();
                m_marks.top().push_back({instruction.key, Timestamp(ts.get_seconds(), 1)});

                // pop vestigal timestamp instruction
                m_instructions.pop();
                m_instructions.push({State::Skip});
                m_instructions.push({State::Skip});
            } else {
                m_marks.top().push_back({instruction.key, Timestamp(0, 1)});
                instruction.type = State::Timestamp;
            }
            break;
        case State::TimestampT:
            if (m_marks.top().back().first == instruction.key) {
                auto ts = std::get<Timestamp>(m_marks.top().back().second);
                m_marks.top().pop_back();
                m_marks.top().push_back({instruction.key, Timestamp(val, ts.get_nanoseconds())});

                // pop vestigal teimstamp instruction
                m_instructions.pop();
                m_instructions.push({State::Skip});
                m_instructions.push({State::Skip});
            } else {
                m_marks.top().push_back({instruction.key, Timestamp(val, 0)});
                instruction.type = State::Timestamp;
            }
            break;
        default:
            throw BsonError(util::format("Invalid state: %1 for JSON instruction number_unsigned: %2",
                                         state_to_string(instruction.type),
                                         val));
    }
    return true;
}

/*!
 @brief an floating-point number was read
 @param[in] val  floating-point value
 @param[in] s    raw token value
 @return whether parsing should proceed
 */
bool Parser::number_float(number_float_t, const string_t&) {
    throw BsonError(util::format("Invalid SAX instruction for canonical extended json: 'number_float'"));
    return true;
}

/*!
 @brief a string was read
 @param[in] val  string value
 @return whether parsing should proceed
 @note It is safe to move the passed string.
 */
bool Parser::string(string_t& val) {
    // pop last instruction
    auto instruction = m_instructions.top();
    m_instructions.pop();

    switch (instruction.type) {
        case State::NumberInt:
            m_marks.top().push_back({instruction.key, atoi(val.data())});
            m_instructions.push({State::Skip});
            break;
        case State::NumberLong:
            m_marks.top().push_back({instruction.key, (int64_t)atol(val.data())});
            m_instructions.push({State::Skip});
            break;
        case State::NumberDouble:
            m_marks.top().push_back({instruction.key, std::stod(val.data())});
            m_instructions.push({State::Skip});
            break;
        case State::NumberDecimal:
            m_marks.top().push_back({instruction.key, Decimal128(val)});
            m_instructions.push({State::Skip});
            break;
        case State::ObjectId:
            m_marks.top().push_back({instruction.key, ObjectId(val.data())});
            m_instructions.push({State::Skip});
            break;
        case State::Date:
            m_marks.top().push_back({instruction.key, time_t(atol(val.data()))});
            // skip twice because this is a number long
            m_instructions.push({State::Skip});
            m_instructions.push({State::Skip});
            break;
        case State::RegularExpressionPattern:
            // if we have already pushed a regex type
            if (m_marks.top().size() && m_marks.top().back().first == instruction.key) {
                auto regex = std::get<RegularExpression>(m_marks.top().back().second);
                m_marks.top().pop_back();
                m_marks.top().push_back({instruction.key, RegularExpression(val, regex.options())});

                // pop vestigal regex instruction
                m_instructions.pop();
                m_instructions.push({State::Skip});
                m_instructions.push({State::Skip});
            } else {
                m_marks.top().push_back({instruction.key, RegularExpression(val, "")});
            }

            break;
        case State::RegularExpressionOptions:
            // if we have already pushed a regex type
            if (m_marks.top().size() && m_marks.top().back().first == instruction.key) {
                auto regex = std::get<RegularExpression>(m_marks.top().back().second);
                m_marks.top().pop_back();
                m_marks.top().push_back({instruction.key, RegularExpression(regex.pattern(), val)});
                // pop vestigal regex instruction
                m_instructions.pop();
                m_instructions.push({State::Skip});
                m_instructions.push({State::Skip});
            } else {
                m_marks.top().push_back({instruction.key, RegularExpression("", val)});
            }

            break;
        case State::BinarySubType:
            // if we have already pushed a binary type
            if (m_marks.top().size() && m_marks.top().back().first == instruction.key) {
                // we will ignore the subtype for now
                // pop vestigal binary instruction
                m_instructions.pop();
                m_instructions.push({State::Skip});
                m_instructions.push({State::Skip});
            } else {
                // we will ignore the subtype for now
                m_marks.top().push_back({instruction.key, std::vector<char>()});
            }

            break;
        case State::BinaryBase64: {
            // if we have already pushed a binary type
            if (m_marks.top().size() && m_marks.top().back().first == instruction.key) {
                m_marks.top().pop_back();
                m_marks.top().push_back({instruction.key, std::vector<char>(val.begin(), val.end())});

                // pop vestigal binary instruction
                m_instructions.pop();
                m_instructions.push({State::Skip});
                m_instructions.push({State::Skip});
            } else {
                // we will ignore the subtype for now
                m_marks.top().push_back({instruction.key, std::vector<char>(val.begin(), val.end())});
            }

            break;
        }
        case State::JsonKey: {
            m_marks.top().push_back({instruction.key, std::string(val.begin(), val.end())});
            break;
        }
        default:
            check_state(instruction.type, State::JsonKey);
            break;
    }
    return true;
}

/*!
 @brief an object key was read
 @param[in] val  object key
 @return whether parsing should proceed
 @note It is safe to move the passed string.
 */
bool Parser::key(string_t& val) {
    if (!m_instructions.empty()) {
        auto top = m_instructions.top();

        if (top.type == State::RegularExpression) {
            if (val == key_regular_expression_pattern) {
                m_instructions.push({State::RegularExpressionPattern, top.key});
            } else if (val == key_regular_expression_options) {
                m_instructions.push({State::RegularExpressionOptions, top.key});
            }
            return true;
        } else if (top.type == State::Date) {
            return true;
        } else if (top.type == State::Binary) {
            if (val == key_binary_base64) {
                m_instructions.push({State::BinaryBase64, top.key});
            } else if (val == key_binary_sub_type) {
                m_instructions.push({State::BinarySubType, top.key});
            }
            return true;
        } else if (top.type == State::Timestamp) {
            if (val == key_timestamp_time) {
                m_instructions.push({State::TimestampT, top.key});
            } else if (val == key_timestamp_increment) {
                m_instructions.push({State::TimestampI, top.key});
            }
            return true;
        }
    }

    const auto it = bson_type_for_key.find(val.data());
    const auto type = (it != bson_type_for_key.end()) ? (*it).second : Parser::State::JsonKey;

    // if the key denotes a bson type
    if (type != State::JsonKey) {
        m_marks.pop();

        if (m_instructions.size()) {
            // if the previous instruction is a key, we don't want it
            if (m_instructions.top().type == State::JsonKey) {
                m_instructions.pop();
            }
            m_instructions.top().type = type;
        } else {
            m_instructions.push({type});
        }
    } else {
        m_instructions.push({
            .key = std::move(val),
            .type = type
        });
    }
    return true;
}


/*!
 @brief the beginning of an object was read
 @param[in] elements  number of object elements or -1 if unknown
 @return whether parsing should proceed
 @note binary formats may report the number of elements
 */
bool Parser::start_object(std::size_t) {
    if (!m_instructions.empty()) {
        auto top = m_instructions.top();

        switch (top.type) {
            case State::NumberInt:
            case State::NumberLong:
            case State::NumberDouble:
            case State::NumberDecimal:
            case State::Binary:
            case State::BinaryBase64:
            case State::BinarySubType:
            case State::Date:
            case State::Timestamp:
            case State::ObjectId:
            case State::String:
            case State::MaxKey:
            case State::MinKey:
            case State::RegularExpression:
            case State::RegularExpressionPattern:
            case State::RegularExpressionOptions:
                return true;
            default:
                break;
        }
    }

    if (m_marks.size() > 1) {
        m_instructions.push({
            State::StartDocument,
            m_instructions.size() ? m_instructions.top().key : ""
        });
    }

    m_marks.push(BsonDocument());
    return true;
}

/*!
 @brief the end of an object was read
 @return whether parsing should proceed
 */
bool Parser::end_object() {
    if (m_instructions.size() && m_instructions.top().type == State::Skip) {
        m_instructions.pop();
        return true;
    }

    if (m_marks.size() > 2) {
        auto document = m_marks.top();
        m_marks.pop();
        m_marks.top().push_back({m_instructions.top().key, std::get<BsonDocument>(document)});
        // pop key and document instructions
        m_instructions.pop();
        m_instructions.pop();
    }
    return true;
};

/*!
 @brief the beginning of an array was read
 @param[in] elements  number of array elements or -1 if unknown
 @return whether parsing should proceed
 @note binary formats may report the number of elements
 */
bool Parser::start_array(std::size_t) {
    m_instructions.push(Instruction{State::StartArray, m_instructions.top().key});
    m_marks.push(BsonArray());

    return true;
};

/*!
 @brief the end of an array was read
 @return whether parsing should proceed
 */
bool Parser::end_array() {
    if (m_marks.size() > 1) {
        auto container = m_marks.top();
        m_marks.pop();
        m_marks.top().push_back({m_instructions.top().key, std::get<BsonArray>(container)});
        // pop key and document instructions
        m_instructions.pop();
        m_instructions.pop();
    }
    return true;
};

/*!
 @brief a parse error occurred
 @param[in] position    the position in the input where the error occurs
 @param[in] last_token  the last read token
 @param[in] ex          an exception object describing the error
 @return whether parsing should proceed (must return false)
 */
bool Parser::parse_error(std::size_t,
                         const std::string&,
                         const nlohmann::detail::exception& ex) {
    throw ex;
    return false;
};

Bson Parser::parse(const std::string& json)
{
    nlohmann::json::sax_parse(json, this);
    if (m_marks.size() == 2) {
        const BsonContainer top = m_marks.top();
        if (std::holds_alternative<BsonDocument>(m_marks.top())) {
            return std::get<BsonDocument>(m_marks.top());
        } else {
            return std::get<BsonArray>(m_marks.top());
        }
    }

    return m_marks.top().back().second;
}

} // namespace detail
} // namespace bson
} // namespace realm
