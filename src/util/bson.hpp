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

#ifndef REALM_BSON_HPP
#define REALM_BSON_HPP

#include <realm/binary_data.hpp>
#include <realm/timestamp.hpp>
#include <realm/decimal128.hpp>
#include <realm/object_id.hpp>
#include <iostream>
#include <unordered_map>
#include <sstream>
#include <json.hpp>
#include <variant>
#include <stack>

namespace realm {
namespace bson {

static constexpr bool str_equal(char const * a, char const * b) {
    return *a == *b && (*a == '\0' || str_equal(a + 1, b + 1));
}

/**
 ======= RegularExpresion =======
 */
/// Provides regular expression capabilities for pattern matching strings in queries.
/// MongoDB uses Perl compatible regular expressions (i.e. "PCRE") version 8.42 with UTF-8 support.
struct RegularExpression {
    enum class Option {
        None,
        IgnoreCase,
        Multiline,
        Dotall,
        Extended
    };

    RegularExpression(const std::string& pattern,
                      const std::string& options) :
    m_pattern(std::move(pattern)) {
        std::transform(options.begin(),
                       options.end(),
                       std::back_inserter(m_options),
                       [](const char c) { return option_char_to_option(c); });
    }

    RegularExpression(const std::string& pattern,
                      const std::vector<Option> options) :
    m_pattern(std::move(pattern)),
    m_options(options) {}

    RegularExpression& operator=(const RegularExpression& regex)
    {
        m_pattern = regex.m_pattern;
        m_options = regex.m_options;
        return *this;
    }

    std::string pattern() const
    {
        return m_pattern;
    }

    std::vector<Option> options() const
    {
        return m_options;
    }
private:
    static constexpr Option option_char_to_option(const char option)
    {
        switch (option) {
            case 'i':
                return Option::IgnoreCase;
            case 'm':
                return Option::Multiline;
            case 's':
                return Option::Dotall;
            case 'x':
                return Option::Extended;
            default:
                throw std::runtime_error("invalid regex option type");
        }
    };

    static constexpr char option_to_option_char(Option option)
    {
        switch (option) {
            case Option::IgnoreCase:
                return 'i';
            case Option::Multiline:
                return 'm';
            case Option::Dotall:
                return 's';
            case Option::Extended:
                return 'x';
            case Option::None:
                return 0;
        }
    };

    friend inline std::ostream& operator<<(std::ostream& out, const Option& o);
    std::string m_pattern;
    std::vector<Option> m_options;
};

inline std::ostream& operator<<(std::ostream& out, const RegularExpression::Option& o)
{
    out << RegularExpression::option_to_option_char(o);
    return out;
}

inline bool operator==(const RegularExpression& lhs, const RegularExpression& rhs)
{
    return lhs.pattern() == rhs.pattern() && lhs.options() == rhs.options();
}

/**
======= MinKey =======
*/
/// MinKey will always be the smallest value when comparing to other BSON types
struct MinKey {
    constexpr explicit MinKey(int)
    {
    }
};
static constexpr MinKey min_key{1};

inline bool operator==(const MinKey&, const MinKey&)
{
    return true;
}

/**
======= MaxKey =======
*/
/// MaxKey will always be the greatest value when comparing to other BSON types
struct MaxKey {
    constexpr explicit MaxKey(int)
    {
    }
};
static constexpr MaxKey max_key{1};

inline bool operator==(const MaxKey&, const MaxKey&)
{
    return true;
}

/**
======= Null =======
*/
/// Bson Null type
struct bson_null_t {
    constexpr explicit bson_null_t() {}
};

static constexpr bson_null_t bson_null = bson_null_t{};
inline bool operator==(const bson_null_t&, const bson_null_t&)
{
    return true;
}

/**
======= IndexedMap =======
*/
/// A map type that orders based on insertion order.
template <typename T>
class IndexedMap
{
public:
    class iterator: public std::iterator<std::forward_iterator_tag, // iterator_category
                                                                 T, // value_type
                                                                 T, // difference_type
                                                                 T*,// pointer
                                                                 T& // reference
    > {
        size_t m_idx = 0;
        IndexedMap* m_map;
    public:
        iterator(IndexedMap* map, size_t idx) : m_idx(idx), m_map(map) {}
        iterator& operator++()
        {
            m_idx++;
            return *this;
        }

        iterator& operator--()
        {
            m_idx--;
            return *this;
        }

        iterator operator++(int)
        {
            return ++(*this);
        }

        iterator operator--(int)
        {
            return --(*this);
        }

        bool operator==(iterator other) const
        {
            return m_idx == other.m_idx;
        }

        bool operator!=(iterator other) const
        {
            return !(*this == other);
        }

        std::pair<std::string, T> operator*()
        {
            return m_map->operator[](m_idx);
        }
    };

    using entry = std::pair<std::string, T>;

    IndexedMap() {}
    IndexedMap(std::initializer_list<entry> entries) {
        for (auto entry : entries) {
            m_keys.push_back(entry.first);
            m_map[entry.first] = entry.second;
        }
    }

    /// The size of the map
    size_t size() const
    {
        return m_map.size();
    }

    /// Find an entry by index
    entry operator[](size_t idx)
    {
        auto key = m_keys[idx];
        return { key, m_map[key] };
    }

    iterator begin()
    {
        return iterator(this, 0);
    }

    iterator end()
    {
        return iterator(this, m_map.size());
    }

    iterator front()
    {
        return this->begin();
    }

    iterator back()
    {
        return this->end()--;
    }

    /// Find or add a given key
    T& operator[](const std::string& k)
    {
        auto entry = m_map.find(k);
        if (entry == m_map.end()) {
            m_keys.push_back(k);
        }

        return m_map[k];
    }

    /// Whether or not this map is empty
    bool empty()
    {
        return m_map.empty();
    }

    /// Pop the last entry of the map
    void pop_back()
    {
        auto last_key = m_keys.back();
        m_keys.pop_back();
        m_map.erase(last_key);
    }

    std::vector<std::string> keys() const
    {
        return m_keys;
    }
private:
    friend inline bool operator==(const IndexedMap<T>& lhs, const IndexedMap<T>& rhs);
    std::unordered_map<std::string, T> m_map;
    std::vector<std::string> m_keys;
};

template <typename T>
inline bool operator==(const IndexedMap<T>& lhs, const IndexedMap<T>& rhs)
{
    return lhs.m_set == rhs.m_set && lhs.m_keys == rhs.m_keys;
}

/**
 ====== BSON =======
 */

class Bson;

/// A variant of the allowed Bson types.
class Bson : public std::variant<
    bson_null_t,
    int32_t,
    int64_t,
    bool,
    float,
    double,
    time_t,
    const char*,
    BinaryData,
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
        bson_null_t,
        int32_t,
        int64_t,
        bool,
        float,
        double,
        time_t,
        const char*,
        BinaryData,
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

/**
 ======= JSON Serialization and Streams =======
 */
namespace {
static void to_json(const Bson& bson, std::stringstream& ss)
{
    std::visit(overloaded {
        [&ss](auto) {
            ss << "null";
        },
        [&ss](bson_null_t) {
            ss << "null";
        },
        [&ss](int32_t bson) {
            ss << "{" << "\"$numberInt\"" << ":" << '"' << bson << '"' << "}";
        },
        [&ss](double bson) {
            ss << "{" << "\"$numberDouble\"" << ":" << '"';
            if (isnan(bson)) {
                ss << "NaN";
            } else if (bson == std::numeric_limits<double>::infinity()) {
                ss << "Infinity";
            } else if (bson == (-1 * std::numeric_limits<double>::infinity())) {
                ss << "-Infinity";
            } else {
                ss << bson;
            }
            ss << '"' << "}";
        },
        [&ss](int64_t bson) {
            ss << "{" << "\"$numberLong\"" << ":" << '"' << bson << '"' << "}";
        },
        [&ss](Decimal128 bson) {
            ss << "{" << "\"$numberDecimal\"" << ":" << '"';
            if (bson.is_nan()) {
                ss << "NaN";
            } else if (bson == Decimal128("Infinity")) {
                ss << "Infinity";
            } else if (bson == Decimal128("-Infinity")) {
                ss << "-Infinity";
            } else {
                ss << bson;
            }
            ss << '"' << "}";
        },
        [&ss](ObjectId bson) {
            ss << "{" << "\"$oid\"" << ":" << '"' << bson << '"' << "}";
        },
        [&ss](BsonDocument bson) {
            ss << "{";
            for (auto const& pair : bson)
            {
                ss << '"' << pair.first << "\":";
                to_json(pair.second, ss);
                ss << ",";
            }
            if (bson.size())
                ss.seekp(-1, std::ios_base::end);
            ss << "}";
        },
        [&ss](BsonArray bson) {
            ss << "[";
            for (auto const& b : bson)
            {
                to_json(b, ss);
                ss << ",";
            }
            if (bson.size())
                ss.seekp(-1, std::ios_base::end);
            ss << "]";
        },
        [&ss](BinaryData bson) {
            ss << "{\"$binary\":{\"base64\":\"" << (bson.is_null() ? "" : bson.data()) << "\",\"subType\":\"00\"}}";
        },
        [&ss](RegularExpression bson) {
            std::stringstream options;
            for (const auto &option : bson.options()) options << option;
            ss << "{\"$regularExpression\":{\"pattern\":\"" << bson.pattern()
                << "\",\"options\":\"" << options.str() << "\"}}";
        },
        [&ss](Timestamp bson) {
            ss << "{\"$timestamp\":{\"t\":" << bson.get_seconds() << ",\"i\":" << 1 << "}}";
        },
        [&ss](time_t bson) {
            ss << "{\"$date\":{\"$numberLong\":\"" << bson << "\"}}";
        },
        [&ss](MaxKey) {
            ss << "{\"$maxKey\":1}";
        },
        [&ss](MinKey) {
            ss << "{\"$minKey\":1}";
        },
        [&ss](const char* bson) {
            ss << '"' << bson << '"';
        },
        [&ss](bool bson) {
            ss << (bson ? "true" : "false");
        },
    }, bson);
}
} // anonymous namespace

inline std::string to_json(const Bson& bson)
{
    std::stringstream ss;
    to_json(bson, ss);
    return ss.str();
}

inline std::string to_json(const BsonDocument& bson)
{
    std::stringstream ss;
    to_json(bson, ss);
    return ss.str();
}


inline std::ostream& operator<<(std::ostream& out, const Bson& m)
{
    out << to_json(m);
    return out;
}

/**
 ======= Equality =======
 */

template <typename T>
inline bool operator==(const Bson& lhs, const T& rhs)
{
    if (auto value = std::get_if<T>(&lhs))
        return *value == rhs;
    return false;
}

template <>
inline bool operator==(const Bson& lhs, const std::string& rhs)
{
    if (auto value = std::get_if<const char*>(&lhs))
        return *value == rhs.data();
    return false;
}

inline bool operator==(const Bson& lhs, const char rhs[])
{
    if (auto value = std::get_if<const char*>(&lhs))
        return !strcmp(*value, (const char*)rhs);
    return false;
}

inline bool operator==(const Bson& lhs, const Bson& rhs)
{
    auto value = std::visit(overloaded {
        [lhs](auto bson) {
            return lhs == bson;
        },
        [lhs](bson_null_t bson) {
            return lhs == bson;
        },
        [lhs](int32_t bson) {
            return lhs == bson;
        },
        [lhs](double bson) {
            return lhs == bson;
        },
        [lhs](int64_t bson) {
            return lhs == bson;
        },
        [lhs](Decimal128 bson) {
            return lhs == bson;
        },
        [lhs](ObjectId bson) {
            return lhs == bson;
        },
        [lhs](BsonDocument bson) {
            return lhs == bson;
        },
        [lhs](BsonArray bson) {
            return lhs == bson;
        },
        [lhs](BinaryData bson) {
            return lhs == bson;
        },
        [lhs](RegularExpression bson) {
            return lhs == bson;
        },
        [lhs](Timestamp bson) {
            return lhs == bson;
        },
        [lhs](time_t bson) {
            return lhs == bson;
        },
        [lhs](MaxKey bson) {
            return lhs == bson;
        },
        [lhs](MinKey bson) {
            return lhs == bson;
        },
        [lhs](const char* bson) {
            return lhs == bson;
        },
        [lhs](bool bson) {
            return lhs == bson;
        },
    }, rhs);
    return value;
}

inline bool operator==(const IndexedMap<Bson>& lhs, const IndexedMap<Bson>& rhs)
{
    return lhs.m_map == rhs.m_map && lhs.keys() == rhs.keys();
}

struct BsonError : public std::runtime_error {
    BsonError(std::string message) : std::runtime_error(message)
    {
    }
};

/**
 ======= PARSER =======
 */
namespace detail {

using namespace nlohmann;

/**
 @brief Parser for extended json. Using nlohmann's SAX interface,
 translate each incoming instruction to it's extended
 json equivalent, constructing extended json from plain json.
 */
class parser : public nlohmann::json_sax<json> {
protected:
    enum class state_t {
        start_document,
        end_document,
        start_array,
        end_array,
        number_int,
        number_long,
        number_double,
        number_decimal,
        binary,
        binary_base64,
        binary_sub_type,
        date,
        timestamp,
        timestamp_t,
        timestamp_i,
        object_id,
        string,
        max_key,
        min_key,
        regular_expression,
        regular_expression_pattern,
        regular_expression_options,
        json_key,
        skip
    };

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

    static constexpr const char* state_to_string(const state_t& i) {
        switch (i) {
            case state_t::start_document:
                return "start_document";
            case state_t::end_document:
                return "end_document";
            case state_t::start_array:
                return "start_array";
            case state_t::end_array:
                return "end_array";
            case state_t::number_int:
                return "number_int";
            case state_t::number_long:
                return "number_long";
            case state_t::number_double:
                return "number_double";
            case state_t::number_decimal:
                return "number_decimal";
            case state_t::binary:
                return "binary";
            case state_t::binary_base64:
                return "binary_base64";
            case state_t::binary_sub_type:
                return "binary_sub_type";
            case state_t::date:
                return "date";
            case state_t::timestamp:
                return "timestamp";
            case state_t::timestamp_t:
                return "timestamp_t";
            case state_t::timestamp_i:
                return "timestamp_i";
            case state_t::object_id:
                return "object_id";
            case state_t::string:
                return "string";
            case state_t::max_key:
                return "max_key";
            case state_t::min_key:
                return "min_key";
            case state_t::regular_expression:
                return "regular_expression";
            case state_t::regular_expression_pattern:
                return "regular_expression_pattern";
            case state_t::regular_expression_options:
                return "regular_expression_options";
            case state_t::json_key:
                return "json_key";
            case state_t::skip:
                return "skip";
        }
    }

    static constexpr state_t bson_type_for_key(const char * val) {
        if (str_equal(val, key_number_int))
            return state_t::number_int;
        if (str_equal(val, key_number_long))
            return state_t::number_long;
        if (str_equal(val, key_number_double))
            return state_t::number_double;
        else if (str_equal(val, key_number_decimal))
            return state_t::number_decimal;
        else if (str_equal(val, key_timestamp))
            return state_t::timestamp;
        else if (str_equal(val, key_date))
            return state_t::date;
        else if (str_equal(val, key_object_id))
            return state_t::object_id;
        else if (str_equal(val, key_max_key))
            return state_t::max_key;
        else if (str_equal(val, key_min_key))
            return state_t::min_key;
        else if (str_equal(val, key_regular_expression))
            return state_t::regular_expression;
        else if (str_equal(val, key_binary))
            return state_t::binary;
        else return state_t::json_key;
    }

    struct Instruction {
        state_t type;
        std::string key;
    };

    void check_state(const state_t& current_state, const state_t& expected_state)
    {
        if (current_state != expected_state)
            throw BsonError(util::format("current state '$1' is not of expected state '$2'",
                                         state_to_string(current_state),
                                         state_to_string(expected_state)));
    }

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
public:
    using number_integer_t = typename json::number_integer_t;
    using number_unsigned_t = typename json::number_unsigned_t;
    using number_float_t = typename json::number_float_t;
    using string_t = typename json::string_t;

    parser() {
        // use a vector container to hold any fragmented values
        m_marks.push(BsonArray());
    }

    /*!
    @brief a null value was read
    @return whether parsing should proceed
    */
    bool null() override {
        auto instruction = m_instructions.top();
        m_instructions.pop();
        check_state(instruction.type, state_t::json_key);
        m_marks.top().push_back({instruction.key, bson_null});
        return true;
    }

    /*!
    @brief a boolean value was read
    @param[in] val  boolean value
    @return whether parsing should proceed
    */
    bool boolean(bool val) override {
        auto instruction = m_instructions.top();
        m_instructions.pop();
        check_state(instruction.type, state_t::json_key);
        m_marks.top().push_back({instruction.key, val});
        return true;
    }

    /*!
    @brief an integer number was read
    @param[in] val  integer value
    @return whether parsing should proceed
    */
    bool number_integer(number_integer_t) override {
        throw BsonError(util::format("Invalid SAX instruction for canonical extended json: 'number_integer'"));
        return true;
    }

    /*!
    @brief an unsigned integer number was read
    @param[in] val  unsigned integer value
    @return whether parsing should proceed
    */
    bool number_unsigned(number_unsigned_t val) override {
        auto instruction = m_instructions.top();
        m_instructions.pop();
        switch (instruction.type) {
            case state_t::max_key:
                m_marks.top().push_back({instruction.key, max_key});
                m_instructions.push({state_t::skip});
                break;
            case state_t::min_key:
                m_marks.top().push_back({instruction.key, min_key});
                m_instructions.push({state_t::skip});
                break;
            case state_t::timestamp_i:
                if (m_marks.top().back().first == instruction.key) {
                    auto ts = std::get<Timestamp>(m_marks.top().back().second);
                    m_marks.top().pop_back();
                    m_marks.top().push_back({instruction.key, Timestamp(ts.get_seconds(), 1)});

                    // pop vestigal timestamp instruction
                    m_instructions.pop();
                    m_instructions.push({state_t::skip});
                    m_instructions.push({state_t::skip});
                } else {
                    m_marks.top().push_back({instruction.key, Timestamp(0, 1)});
                    instruction.type = state_t::timestamp;
                }
                break;
            case state_t::timestamp_t:
                if (m_marks.top().back().first == instruction.key) {
                    auto ts = std::get<Timestamp>(m_marks.top().back().second);
                    m_marks.top().pop_back();
                    m_marks.top().push_back({instruction.key, Timestamp(val, ts.get_nanoseconds())});

                    // pop vestigal teimstamp instruction
                    m_instructions.pop();
                    m_instructions.push({state_t::skip});
                    m_instructions.push({state_t::skip});
                } else {
                    m_marks.top().push_back({instruction.key, Timestamp(val, 0)});
                    instruction.type = state_t::timestamp;
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
    bool number_float(number_float_t, const string_t&) override {
        throw BsonError(util::format("Invalid SAX instruction for canonical extended json: 'number_float'"));
        return true;
    }

    /*!
    @brief a string was read
    @param[in] val  string value
    @return whether parsing should proceed
    @note It is safe to move the passed string.
    */
    bool string(string_t& val) override {
        // pop last instruction
        auto instruction = m_instructions.top();
        m_instructions.pop();

        switch (instruction.type) {
            case state_t::number_int:
                m_marks.top().push_back({instruction.key, atoi(val.data())});
                m_instructions.push({state_t::skip});
                break;
            case state_t::number_long:
                m_marks.top().push_back({instruction.key, (int64_t)atol(val.data())});
                m_instructions.push({state_t::skip});
                break;
            case state_t::number_double:
                m_marks.top().push_back({instruction.key, std::stod(val.data())});
                m_instructions.push({state_t::skip});
                break;
            case state_t::number_decimal:
                m_marks.top().push_back({instruction.key, Decimal128(val)});
                m_instructions.push({state_t::skip});
                break;
            case state_t::object_id:
                m_marks.top().push_back({instruction.key, ObjectId(val.data())});
                m_instructions.push({state_t::skip});
                break;
            case state_t::date:
                m_marks.top().push_back({instruction.key, time_t(atol(val.data()))});
                // skip twice because this is a number long
                m_instructions.push({state_t::skip});
                m_instructions.push({state_t::skip});
                break;
            case state_t::regular_expression_pattern:
                // if we have already pushed a regex type
                if (m_marks.top().size() && m_marks.top().back().first == instruction.key) {
                    auto regex = std::get<RegularExpression>(m_marks.top().back().second);
                    m_marks.top().pop_back();
                    m_marks.top().push_back({instruction.key, RegularExpression(val, regex.options())});

                    // pop vestigal regex instruction
                    m_instructions.pop();
                    m_instructions.push({state_t::skip});
                    m_instructions.push({state_t::skip});
                } else {
                    m_marks.top().push_back({instruction.key, RegularExpression(val, "")});
                }

                break;
            case state_t::regular_expression_options:
                // if we have already pushed a regex type
                if (m_marks.top().size() && m_marks.top().back().first == instruction.key) {
                    auto regex = std::get<RegularExpression>(m_marks.top().back().second);
                    m_marks.top().pop_back();
                    m_marks.top().push_back({instruction.key, RegularExpression(regex.pattern(), val)});
                    // pop vestigal regex instruction
                    m_instructions.pop();
                    m_instructions.push({state_t::skip});
                    m_instructions.push({state_t::skip});
                } else {
                    m_marks.top().push_back({instruction.key, RegularExpression("", val)});
                }

                break;
            case state_t::binary_sub_type:
                // if we have already pushed a binary type
                if (m_marks.top().size() && m_marks.top().back().first == instruction.key) {
                    // we will ignore the subtype for now
                    // pop vestigal binary instruction
                    m_instructions.pop();
                    m_instructions.push({state_t::skip});
                    m_instructions.push({state_t::skip});
                } else {
                    // we will ignore the subtype for now
                    m_marks.top().push_back({instruction.key, BinaryData()});
                }

                break;
            case state_t::binary_base64: {
                char* binary_val = (char *)malloc(sizeof(char) * val.size());
                BinaryData binary_data;
                if (val.size()) {
                    const char* copy = strcpy(binary_val, val.data());
                    binary_data = BinaryData(copy, strlen(copy) + 1);
                }
                // if we have already pushed a binary type
                if (m_marks.top().size() && m_marks.top().back().first == instruction.key) {
                    m_marks.top().pop_back();
                    m_marks.top().push_back({instruction.key, binary_data});

                    // pop vestigal binary instruction
                    m_instructions.pop();
                    m_instructions.push({state_t::skip});
                    m_instructions.push({state_t::skip});
                } else {
                    // we will ignore the subtype for now
                    m_marks.top().push_back({instruction.key, binary_data});
                }

                break;
            }
            case state_t::json_key: {
                char* v = (char *)malloc(sizeof(char) * val.size());
                m_marks.top().push_back({instruction.key, strcpy(v, val.data())});
                break;
            }
            default:
                check_state(instruction.type, state_t::json_key);
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
    bool key(string_t& val) override {
        if (!m_instructions.empty()) {
            auto top = m_instructions.top();

            if (top.type == state_t::regular_expression) {
                if (val == key_regular_expression_pattern) {
                    m_instructions.push({state_t::regular_expression_pattern, top.key});
                } else if (val == key_regular_expression_options) {
                    m_instructions.push({state_t::regular_expression_options, top.key});
                }
                return true;
            } else if (top.type == state_t::date) {
                return true;
            } else if (top.type == state_t::binary) {
                if (val == key_binary_base64) {
                    m_instructions.push({state_t::binary_base64, top.key});
                } else if (val == key_binary_sub_type) {
                    m_instructions.push({state_t::binary_sub_type, top.key});
                }
                return true;
            } else if (top.type == state_t::timestamp) {
                if (val == key_timestamp_time) {
                    m_instructions.push({state_t::timestamp_t, top.key});
                } else if (val == key_timestamp_increment) {
                    m_instructions.push({state_t::timestamp_i, top.key});
                }
                return true;
            }
        }

        auto type = bson_type_for_key(val.data());

        // if the key denotes a bson type
        if (type != state_t::json_key) {
            m_marks.pop();

            if (m_instructions.size()) {
                // if the previous instruction is a key, we don't want it
                if (m_instructions.top().type == state_t::json_key) {
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
    bool start_object(std::size_t) override {
        if (!m_instructions.empty()) {
            auto top = m_instructions.top();

            switch (top.type) {
                case state_t::number_int:
                case state_t::number_long:
                case state_t::number_double:
                case state_t::number_decimal:
                case state_t::binary:
                case state_t::binary_base64:
                case state_t::binary_sub_type:
                case state_t::date:
                case state_t::timestamp:
                case state_t::object_id:
                case state_t::string:
                case state_t::max_key:
                case state_t::min_key:
                case state_t::regular_expression:
                case state_t::regular_expression_pattern:
                case state_t::regular_expression_options:
                    return true;
                default:
                    break;
            }
        }

        if (m_marks.size() > 1) {
            m_instructions.push({
                state_t::start_document,
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
    bool end_object() override {
        if (m_instructions.size() && m_instructions.top().type == state_t::skip) {
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
    bool start_array(std::size_t) override {
        m_instructions.push(Instruction{state_t::start_array, m_instructions.top().key});
        m_marks.push(BsonArray());

        return true;
    };

    /*!
    @brief the end of an array was read
    @return whether parsing should proceed
    */
    bool end_array() override {
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
    bool parse_error(std::size_t,
                     const std::string&,
                     const nlohmann::detail::exception& ex) override {
        throw ex;
        return false;
    };

    Bson parse(const std::string& json)
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
};
} // namespace detail

inline Bson parse(const std::string& json) {
    return detail::parser().parse(json);
}

} // namespace bson
} // namespace realm


            
#endif // REALM_BSON_HPP
