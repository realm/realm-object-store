////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef realm_sdk_util_h
#define realm_sdk_util_h

#include <cxxabi.h>
#include <type_traits>
#include "property.hpp"

namespace realm::sdk::util {

namespace type_traits {
struct embedded_object;
struct object;
struct list;
struct primitive;
struct enum_type;
struct primary_key;
} // namespace type_traits

template <typename T>
using EmbeddedObject = Property<T, util::type_traits::embedded_object>;

static inline std::string demangle(const char* name) {
    int status = -4; // some arbitrary value to eliminate the compiler warning

    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name, NULL, NULL, &status),
        std::free
    };

    return (status==0) ? res.get() : name;
}

template <typename, typename = void>
struct is_embedded : std::false_type {};
template <typename T>
struct is_embedded<T, typename std::enable_if_t<std::is_base_of_v<EmbeddedObject<T>, T>>> : std::true_type {};
template <typename T>
static constexpr auto is_embedded_v = is_embedded<T>::value;

template <class T, std::size_t = sizeof(T)>
std::true_type is_complete_impl(T *);

std::false_type is_complete_impl(...);

template <class T>
using is_complete = decltype(is_complete_impl(std::declval<T*>()));

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
    trim(s);
    return s;
}

template <typename T, typename E = void>
struct is_primary_key;
template <typename T>
struct is_primary_key<T> : std::false_type {};
template <typename T>
struct is_primary_key<Property<T, void>> : std::false_type {};
template <typename T>
struct is_primary_key<Property<T, util::type_traits::primary_key>> : std::true_type {};

static_assert(is_primary_key<Property<std::string, util::type_traits::primary_key>>::value, "");

template <typename T>
struct conditional_property_type {
    using type = std::conditional_t<realm::sdk::util::is_embedded_v<T>, T, realm::sdk::Property<T>>;
};

template <typename PrimaryKeyT, typename T>
bool deduce_primary_key(PrimaryKeyT& primary_key_ref, T& property)
{
    if constexpr (is_primary_key<std::remove_reference_t<T>>::value) {
        primary_key_ref = property.value();
        return true;
    } else {
        return false;
    }
}

template <typename Object, typename ...Properties>
auto deduce_primary_key(Properties ...ps)
{
    if constexpr ((is_primary_key<decltype(ps)>::value || ...)) {
        typename std::decay_t<Object>::primary_key_type primary_key;
        (... || deduce_primary_key(primary_key, ps));
        return primary_key;
    }
}

template <typename ...Args>
constexpr auto _has_primary_key() {
    return (is_primary_key<Args>::value || ...);
}

template <typename ...Args>
struct has_primary_key {
    static constexpr auto value = _has_primary_key<Args...>();
};

template <typename ...Args>
constexpr bool has_primary_key_v = has_primary_key<Args...>::value;
}

#endif /* util_h */
