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

#ifndef realm_sdk_type_info_h
#define realm_sdk_type_info_h

#include "../property.hpp"
#include "util.hpp"

namespace realm::sdk {

/**
 Specialized types that provide compile time information
 on supported types.

 Each TypeInfo specialization has its core type,
 its const core type (mostly to support ConstObj and ConstLst),
 its actual type (e.g., std::string vs. StringData), and its
 realm::PropertyType.
 */
template <typename T, typename E = void>
struct TypeInfo;

template <>
struct TypeInfo<bool>
{
    using realm_type = Bool;
    using const_realm_type = Bool;
    using value_type = bool;
    static const auto property_type = PropertyType::Bool;
};
template <>
struct TypeInfo<int>
{
    using realm_type = Int;
    using const_realm_type = Int;
    using value_type = int;
    static const auto property_type = PropertyType::Int;
};
template <>
struct TypeInfo<double>
{
    using realm_type = Double;
    using const_realm_type = Double;
    using value_type = double;
    static const auto property_type = PropertyType::Double;
};
template <>
struct TypeInfo<float>
{
    using realm_type = Float;
    using const_realm_type = Float;
    using value_type = float;
    static const auto property_type = PropertyType::Float;
};
template <>
struct TypeInfo<ObjectId>
{
    using realm_type = ObjectId;
    using const_realm_type = ObjectId;
    using value_type = ObjectId;
    static const auto property_type = PropertyType::ObjectId;
};
template <typename C, typename D>
struct TypeInfo<std::chrono::time_point<C, D>>
{
    using realm_type = Timestamp;
    using const_realm_type = Timestamp;
    using value_type = std::chrono::time_point<C, D>;
    static const auto property_type = PropertyType::Date;
};
template <>
struct TypeInfo<std::string>
{
    using realm_type = StringData;
    using const_realm_type = StringData;
    using value_type = std::string;
    static const auto property_type = PropertyType::String;
};
template <typename T>
struct TypeInfo<T, typename std::enable_if_t<std::is_enum_v<T>>>
{
    using realm_type = Int;
    using const_realm_type = Int;
    using value_type = T;
    static const auto property_type = PropertyType::Int;
};
template <typename T>
struct TypeInfo<T, typename std::enable_if_t<std::is_pointer_v<T>>>
{
    using realm_type = ObjKey;
    using const_realm_type = ObjKey;
    using value_type = T;
    static const auto property_type = PropertyType::Object | PropertyType::Nullable;
};
template <typename T>
struct TypeInfo<T, typename std::enable_if_t<util::is_embedded_v<T>>>
{
    using realm_type = ObjKey;
    using const_realm_type = ObjKey;
    using value_type = T;
    static const auto property_type = PropertyType::Object | PropertyType::Nullable;
};
template <typename T>
struct TypeInfo<std::vector<T>>
{
    using value_type_type_info = TypeInfo<T>;
    using realm_type = Lst<typename value_type_type_info::realm_type>;
    using const_realm_type = ConstLst<typename value_type_type_info::const_realm_type>;
    using value_type = std::vector<T>;
    static const auto property_type =
        (PropertyType::Array | value_type_type_info::property_type) & ~PropertyType::Nullable;
};
template <typename T>
struct TypeInfo<realm::util::Optional<T>>
{
private:
    using non_optional_type_info = TypeInfo<T>;
public:
    using realm_type = realm::util::Optional<typename non_optional_type_info::realm_type>;
    using const_realm_type = realm::util::Optional<typename non_optional_type_info::const_realm_type>;
    using value_type = realm::util::Optional<typename non_optional_type_info::value_type>;
    static const auto property_type = non_optional_type_info::property_type | PropertyType::Nullable;
};

}

#endif /* type_info_h */
