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

#include <iostream>
namespace realm {
namespace bson {

class Bson {
public:
    Bson() noexcept
        : m_type(NONE)
    {
    }

    Bson(util::None) noexcept
        : Bson()
    {
    }

    Bson(int32_t) noexcept;
    Bson(int64_t) noexcept;
    Bson(bool) noexcept;
    Bson(double) noexcept;
    Bson(MinKey) noexcept;
    Bson(MaxKey) noexcept;


    Bson(time_t) noexcept;
    Bson(Timestamp) noexcept;
    Bson(Decimal128) noexcept;
    Bson(ObjectId) noexcept;

    Bson(const RegularExpression&) noexcept;
    Bson(const std::vector<char>&) noexcept;
    Bson(const std::string&) noexcept;
    Bson(const IndexedMap<Bson>&) noexcept;
    Bson(const std::vector<Bson>&) noexcept;
    Bson(std::string&&) noexcept;
    Bson(IndexedMap<Bson>&&) noexcept;
    Bson(std::vector<Bson>&&) noexcept;
    
    // These are shortcuts for Bson(StringData(c_str)), and are
    // needed to avoid unwanted implicit conversion of char* to bool.
    Bson(char* c_str) noexcept
        : Bson(StringData(c_str))
    {
    }
    Bson(const char* c_str) noexcept
        : Bson(StringData(c_str))
    {
    }

    ~Bson() noexcept
    {
        switch (m_type) {
            case STRING:
                delete string_val;
                string_val = NULL;
                break;
            case DOCUMENT:
                delete document_val;
                document_val = NULL;
                break;
            case ARRAY:
                delete array_val;
                array_val = NULL;
                break;
            case BINARY:
                delete binary_val;
                binary_val = NULL;
                break;
            case REGEX:
                delete regex_val;
                regex_val = NULL;
                break;
            default:
                break;
        }
    }

    Bson(Bson&& v) {
        std::cout<<"TRYING TO MOVE BSON"<<std::endl;
        m_type = v.m_type;
    }

    Bson(const Bson& v) {
        m_type = v.m_type;
        switch (v.m_type) {
            case NONE:
                break;
            case INT32:
                int32_val = v.int32_val;
                break;
            case INT64:
                int64_val = v.int64_val;
                break;
            case BOOL:
                bool_val = v.bool_val;
                break;
            case DOUBLE:
                double_val = v.double_val;
                break;
            case TIME:
                time_val = v.time_val;
                break;
            case DATE:
                date_val = v.date_val;
                break;
            case OID:
                oid_val = v.oid_val;
                break;
            case DECIMAL:
                decimal_val = v.decimal_val;
                break;
            case MAX_KEY:
                max_key_val = v.max_key_val;
                break;
            case MIN_KEY:
                min_key_val = v.min_key_val;
                break;
            case BINARY:
                binary_val = new std::vector<char>;
                *binary_val = *v.binary_val;
               break;
            case REGEX:
                regex_val = new RegularExpression;
                *regex_val = *v.regex_val;
                break;
            case STRING:
                string_val = new std::string;
                *string_val = *v.string_val;
                break;
            case DOCUMENT:
                document_val = new IndexedMap<Bson>;
                *document_val = *v.document_val;
                break;
            case ARRAY:
                array_val = new std::vector<Bson>;
                *array_val = *v.array_val;
                break;
        }
    }

    Bson& operator=(Bson&& v) {
        m_type = v.m_type;
        switch (v.m_type) {
            case NONE:
                m_type = NONE;
                break;
            case INT32:
                int32_val = v.int32_val;
                break;
            case INT64:
                int64_val = v.int64_val;
                break;
            case BOOL:
                bool_val = v.bool_val;
                break;
            case DOUBLE:
                double_val = v.double_val;
                break;
            case TIME:
                time_val = v.time_val;
                break;
            case DATE:
                date_val = v.date_val;
                break;
            case OID:
                oid_val = v.oid_val;
                break;
            case DECIMAL:
                decimal_val = v.decimal_val;
                break;

            case MAX_KEY:
                max_key_val = v.max_key_val;
                break;
            case MIN_KEY:
                min_key_val = v.min_key_val;
                break;
            case REGEX:
                if (regex_val) delete regex_val;
                regex_val = v.regex_val;
                v.regex_val = NULL;
                break;
            case BINARY:
                if (binary_val) delete binary_val;
                binary_val = v.binary_val;
                v.binary_val = NULL;
                break;
            case STRING:
                if (string_val) delete string_val;
                string_val = v.string_val;
                v.string_val = NULL;
                break;
            case DOCUMENT:
                if (document_val) delete document_val;
                document_val = v.document_val;
                v.document_val = NULL;
                break;
            case ARRAY:
                if (array_val) delete array_val;
                array_val = v.array_val;
                v.array_val = NULL;
                break;
        }

        return *this;
    }

    Bson& operator=(const Bson& v) {
        m_type = v.m_type;
        switch (v.m_type) {
            case NONE:
                m_type = NONE;
                break;
            case INT32:
                int32_val = v.int32_val;
                break;
            case INT64:
                int64_val = v.int64_val;
                break;
            case BOOL:
                bool_val = v.bool_val;
                break;
            case DOUBLE:
                double_val = v.double_val;
                break;
            case TIME:
                time_val = v.time_val;
                break;
            case DATE:
                date_val = v.date_val;
                break;
            case OID:
                oid_val = v.oid_val;
                break;
            case DECIMAL:
                decimal_val = v.decimal_val;
                break;
            case MAX_KEY:
                max_key_val = v.max_key_val;
                break;
            case MIN_KEY:
                min_key_val = v.min_key_val;
                break;
            case BINARY:
                binary_val = new std::vector<char>;
                *binary_val = *v.binary_val;
                break;
            case REGEX:
                regex_val = new RegularExpression;
                *regex_val = *v.regex_val;
                break;
            case STRING:
                string_val = new std::string;
                *string_val = *v.string_val;
                break;
            case DOCUMENT:
                document_val = new IndexedMap<Bson>;
                *document_val = *v.document_val;
                break;
            case ARRAY:
                array_val = new std::vector<Bson>;
                *array_val = *v.array_val;
                break;
        }

        return *this;
    }

    explicit operator int32_t() const
    {
        REALM_ASSERT(m_type == Bson::INT32);
        return int32_val;
    }

    explicit operator int64_t() const
    {
        REALM_ASSERT(m_type == Bson::INT64);
        return int64_val;
    }

    explicit operator bool() const
    {
        REALM_ASSERT(m_type == Bson::BOOL);
        return bool_val;
    }

    explicit operator double() const
    {
        REALM_ASSERT(m_type == Bson::DOUBLE);
        return double_val;
    }

    explicit operator const std::string&() const
    {
        REALM_ASSERT(m_type == Bson::STRING);
        return *string_val;
    }

    explicit operator const std::vector<char>&() const
    {
        REALM_ASSERT(m_type == Bson::BINARY);
        return *binary_val;
    }

    explicit operator time_t() const
    {
        REALM_ASSERT(m_type == Bson::TIME);
        return time_val;
    }

    explicit operator Timestamp() const
    {
        REALM_ASSERT(m_type == Bson::DATE);
        return date_val;
    }

    explicit operator ObjectId() const
    {
        REALM_ASSERT(m_type == Bson::OID);
        return oid_val;
    }

    explicit operator Decimal128() const
    {
        REALM_ASSERT(m_type == Bson::DECIMAL);
        return decimal_val;
    }

    explicit operator const RegularExpression&() const
    {
        REALM_ASSERT(m_type == Bson::REGEX);
        return *regex_val;
    }

    explicit operator MinKey() const
    {
        REALM_ASSERT(m_type == Bson::MIN_KEY);
        return min_key_val;
    }

    explicit operator MaxKey() const
    {
        REALM_ASSERT(m_type == Bson::MAX_KEY);
        return max_key_val;
    }

    explicit operator const IndexedMap<Bson>&&() const
    {
        REALM_ASSERT(m_type == Bson::DOCUMENT);
        return std::move(*document_val);
    }

    explicit operator const std::vector<Bson>&&() const
    {
        REALM_ASSERT(m_type == Bson::ARRAY);
        return std::move(*array_val);
    }

    size_t get_type() const noexcept
    {
        return m_type;
    }

    bool is_null() const;

    bool operator==(const Bson& other) const
    {
        if (m_type != other.m_type) {
            return false;
        }

        switch (m_type) {
            case NONE:
                return true;
            case INT32:
                return int32_val == other.int32_val;
            case INT64:
                return int64_val == other.int64_val;
            case BOOL:
                return bool_val == other.bool_val;
            case DOUBLE:
                return double_val == other.double_val;
            case TIME:
                return time_val == other.time_val;
            case DATE:
                return date_val == other.date_val;
            case OID:
                return oid_val == other.oid_val;
            case DECIMAL:
                return decimal_val == other.decimal_val;
            case MAX_KEY:
                return max_key_val == other.max_key_val;
            case MIN_KEY:
                return min_key_val == other.min_key_val;
            case STRING:
                return *string_val == *other.string_val;
            case REGEX:
                return *regex_val == *other.regex_val;
            case BINARY:
                return *binary_val == *other.binary_val;
            case DOCUMENT:
                return *document_val == *other.document_val;
            case ARRAY:
                return *array_val == *other.array_val;
        }
    }

    bool operator!=(const Bson& other) const
    {
        return !(*this == other);
    }

private:
    friend std::ostream& operator<<(std::ostream& out, const Bson& m);
    template <typename T>
    friend bool holds_alternative(const Bson& bson);

    typedef enum Type {
        NONE,
        INT32,
        INT64,
        BOOL,
        DOUBLE,
        STRING,
        BINARY,
        TIME,
        DATE,
        OID,
        DECIMAL,
        REGEX,
        MAX_KEY,
        MIN_KEY,
        DOCUMENT,
        ARRAY
    } Type;
    Type m_type;

    union {
        int32_t int32_val;
        int64_t int64_val;
        bool bool_val;
        double double_val;
        time_t time_val;
        Timestamp date_val;
        ObjectId oid_val;
        Decimal128 decimal_val;
        MaxKey max_key_val;
        MinKey min_key_val;
        // ref types
        RegularExpression* regex_val;
        std::string* string_val;
        std::vector<char>* binary_val;
        IndexedMap<Bson>* document_val;
        std::vector<Bson>* array_val;
    };
};

inline Bson::Bson(int32_t v) noexcept
{
    m_type = Bson::INT32;
    int32_val = v;
}

inline Bson::Bson(int64_t v) noexcept
{
    m_type = Bson::INT64;
    int64_val = v;
}

inline Bson::Bson(bool v) noexcept
{
    m_type = Bson::BOOL;
    bool_val = v;
}

inline Bson::Bson(double v) noexcept
{
    m_type = Bson::DOUBLE;
    double_val = v;
}

inline Bson::Bson(MinKey v) noexcept
{
    m_type = Bson::MIN_KEY;
    min_key_val = v;
}

inline Bson::Bson(MaxKey v) noexcept
{
    m_type = Bson::MAX_KEY;
    max_key_val = v;
}
inline Bson::Bson(const RegularExpression& v) noexcept
{
    m_type = Bson::REGEX;
    regex_val = new RegularExpression(v);
}

inline Bson::Bson(const std::vector<char>& v) noexcept
{
    m_type = Bson::BINARY;
    binary_val = new std::vector<char>(v);
}

inline Bson::Bson(const std::string& v) noexcept
{
    m_type = Bson::STRING;
    string_val = new std::string(v);
}

inline Bson::Bson(std::string&& v) noexcept
{
    m_type = Bson::STRING;
    string_val = new std::string(std::move(v));
}

inline Bson::Bson(time_t v) noexcept
{
    m_type = Bson::TIME;
    time_val = v;
}

inline Bson::Bson(Timestamp v) noexcept
{
    m_type = Bson::DATE;
    date_val = v;
}

inline Bson::Bson(Decimal128 v) noexcept
{
    m_type = Bson::DECIMAL;
    decimal_val = v;
}

inline Bson::Bson(ObjectId v) noexcept
{
    m_type = Bson::OID;
    oid_val = v;
}

inline Bson::Bson(const IndexedMap<Bson>& v) noexcept
{
    m_type = Bson::DOCUMENT;
    document_val = new IndexedMap<Bson>(v);
}

inline Bson::Bson(const std::vector<Bson>& v) noexcept
{
    m_type = Bson::ARRAY;
    array_val = new std::vector(v);
}

inline Bson::Bson(IndexedMap<Bson>&& v) noexcept
{
    m_type = Bson::DOCUMENT;
    document_val = new IndexedMap<Bson>(std::move(v));
}

inline Bson::Bson(std::vector<Bson>&& v) noexcept
{
    m_type = Bson::ARRAY;
    array_val = new std::vector(std::move(v));
//    array_val = std::make_unique<std::vector<Bson>>(std::move(v));
}

template <typename T>
bool holds_alternative(const Bson& bson);

template<>
inline bool holds_alternative<util::None>(const Bson& bson)
{
    return bson.m_type == Bson::NONE;
}

template<>
inline bool holds_alternative<int32_t>(const Bson& bson)
{
    return bson.m_type == Bson::INT32;
}

template<>
inline bool holds_alternative<int64_t>(const Bson& bson)
{
    return bson.m_type == Bson::INT64;
}

template<>
inline bool holds_alternative<bool>(const Bson& bson)
{
    return bson.m_type == Bson::BOOL;
}

template<>
inline bool holds_alternative<double>(const Bson& bson)
{
    return bson.m_type == Bson::DOUBLE;
}

template<>
inline bool holds_alternative<std::string>(const Bson& bson)
{
    return bson.m_type == Bson::STRING;
}

template<>
inline bool holds_alternative<std::vector<char>>(const Bson& bson)
{
    return bson.m_type == Bson::BINARY;
}

template<>
inline bool holds_alternative<time_t>(const Bson& bson)
{
    return bson.m_type == Bson::TIME;
}

template<>
inline bool holds_alternative<Timestamp>(const Bson& bson)
{
    return bson.m_type == Bson::DATE;
}

template<>
inline bool holds_alternative<ObjectId>(const Bson& bson)
{
    return bson.m_type == Bson::OID;
}

template<>
inline bool holds_alternative<Decimal128>(const Bson& bson)
{
    return bson.m_type == Bson::DECIMAL;
}

template<>
inline bool holds_alternative<RegularExpression>(const Bson& bson)
{
    return bson.m_type == Bson::REGEX;
}

template<>
inline bool holds_alternative<MinKey>(const Bson& bson)
{
    return bson.m_type == Bson::MIN_KEY;
}

template<>
inline bool holds_alternative<MaxKey>(const Bson& bson)
{
    return bson.m_type == Bson::MAX_KEY;
}

template<>
inline bool holds_alternative<IndexedMap<Bson>>(const Bson& bson)
{
    return bson.m_type == Bson::DOCUMENT;
}

template<>
inline bool holds_alternative<std::vector<Bson>>(const Bson& bson)
{
    return bson.m_type == Bson::ARRAY;
}

using BsonDocument = IndexedMap<Bson>;
using BsonArray = std::vector<Bson>;

std::ostream& operator<<(std::ostream& out, const Bson& b);

Bson parse(const std::string& json);

} // namespace bson
} // namespace realm
            
#endif // REALM_BSON_HPP
