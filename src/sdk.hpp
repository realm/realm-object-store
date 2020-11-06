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

#ifndef realm_sdk_h
#define realm_sdk_h

#include "sdk/object.hpp"
#include "sdk/optional.hpp"
#include "sdk/string.hpp"
#include "sdk/primitive.hpp"
#include "sdk/arithmetic.hpp"
#include "sdk/list.hpp"
#include "sdk/realm.hpp"
#include "sdk/time_point.hpp"
#include "sdk/object_id.hpp"
#include "sdk/property.hpp"

// MARK: Macro Utilities

#define SECOND(a, b, ...) b

#define IS_PROBE(...) SECOND(__VA_ARGS__, 0)
#define PROBE() ~, 1

#define CAT(a,b) a ## b

#define NOT(x) IS_PROBE(CAT(_NOT_, x))
#define _NOT_0 PROBE()

#define BOOL(x) NOT(NOT(x))

#define IF_ELSE(condition) _IF_ELSE(BOOL(condition))
#define _IF_ELSE(condition) CAT(_IF_, condition)

#define _IF_1(...) __VA_ARGS__ _IF_1_ELSE
#define _IF_0(...)             _IF_0_ELSE

#define _IF_1_ELSE(...)
#define _IF_0_ELSE(...) __VA_ARGS__

#define SECOND(a, b, ...) b

#define IS_PROBE(...) SECOND(__VA_ARGS__, 0)
#define PROBE() ~, 1

#define CAT(a,b) a ## b

#define NOT(x) IS_PROBE(CAT(_NOT_, x))
#define _NOT_0 PROBE()

#define BOOL(x) NOT(NOT(x))

#define IF_ELSE(condition) _IF_ELSE(BOOL(condition))
#define _IF_ELSE(condition) CAT(_IF_, condition)

#define _IF_1(...) __VA_ARGS__ _IF_1_ELSE
#define _IF_0(...)             _IF_0_ELSE

#define _IF_1_ELSE(...)
#define _IF_0_ELSE(...) __VA_ARGS__

#define FE_CALLITn01(a,b)  a b
#define FE_CALLITn02(a,b)  a b
#define FE_CALLITn03(a,b)  a b
#define FE_CALLITn04(a,b)  a b
#define FE_CALLITn04(a,b)  a b
#define FE_CALLITn05(a,b)  a b
#define FE_CALLITn06(a,b)  a b
#define FE_CALLITn07(a,b)  a b
#define FE_CALLITn08(a,b)  a b
#define FE_CALLITn09(a,b)  a b
#define FE_CALLITn10(a,b)  a b
#define FE_CALLITn11(a,b)  a b
#define FE_CALLITn12(a,b)  a b
#define FE_CALLITn13(a,b)  a b
#define FE_CALLITn14(a,b)  a b
#define FE_CALLITn15(a,b)  a b
#define FE_CALLITn16(a,b)  a b
#define FE_CALLITn17(a,b)  a b
#define FE_CALLITn18(a,b)  a b
#define FE_CALLITn19(a,b)  a b
#define FE_CALLITn20(a,b)  a b
#define FE_CALLITn21(a,b)  a b

#define FE_n00()
#define FE_n01(what, arg, a, ...)  what(0, arg, a)
#define FE_n02(what, arg, a, ...)  what(1, arg, a) FE_CALLITn02(FE_n01,(what, arg, ##__VA_ARGS__))
#define FE_n03(what, arg, a, ...)  what(2, arg, a) FE_CALLITn03(FE_n02,(what, arg, ##__VA_ARGS__))
#define FE_n04(what, arg, a, ...)  what(3, arg, a) FE_CALLITn04(FE_n03,(what, arg, ##__VA_ARGS__))
#define FE_n05(what, arg, a, ...)  what(4, arg, a) FE_CALLITn05(FE_n04,(what, arg, ##__VA_ARGS__))
#define FE_n06(what, arg, a, ...)  what(5, arg, a) FE_CALLITn06(FE_n05,(what, arg, ##__VA_ARGS__))
#define FE_n07(what, arg, a, ...)  what(6, arg, a) FE_CALLITn07(FE_n06,(what, arg, ##__VA_ARGS__))
#define FE_n08(what, arg, a, ...)  what(7, arg, a) FE_CALLITn08(FE_n07,(what, arg, ##__VA_ARGS__))
#define FE_n09(what, arg, a, ...)  what(8, arg, a) FE_CALLITn09(FE_n08,(what, arg, ##__VA_ARGS__))
#define FE_n10(what, arg, a, ...)  what(9, arg, a) FE_CALLITn10(FE_n09,(what, arg, ##__VA_ARGS__))
#define FE_n11(what, arg, a, ...)  what(10, arg, a) FE_CALLITn11(FE_n10,(what, arg, ##__VA_ARGS__))
#define FE_n12(what, arg, a, ...)  what(11, arg, a) FE_CALLITn12(FE_n11,(what, arg, ##__VA_ARGS__))
#define FE_n13(what, arg, a, ...)  what(12, arg, a) FE_CALLITn13(FE_n12,(what, arg, ##__VA_ARGS__))
#define FE_n14(what, arg, a, ...)  what(13, arg, a) FE_CALLITn14(FE_n13,(what, arg, ##__VA_ARGS__))
#define FE_n15(what, arg, a, ...)  what(14, arg, a) FE_CALLITn15(FE_n14,(what, arg, ##__VA_ARGS__))
#define FE_n16(what, arg, a, ...)  what(15, arg, a) FE_CALLITn16(FE_n15,(what, arg, ##__VA_ARGS__))
#define FE_n17(what, arg, a, ...)  what(16, arg, a) FE_CALLITn17(FE_n16,(what, arg, ##__VA_ARGS__))
#define FE_n18(what, arg, a, ...)  what(17, arg, a) FE_CALLITn18(FE_n17,(what, arg, ##__VA_ARGS__))
#define FE_n19(what, arg, a, ...)  what(18, arg, a) FE_CALLITn19(FE_n18,(what, arg, ##__VA_ARGS__))
#define FE_n20(what, arg, a, ...)  what(19, arg, a) FE_CALLITn20(FE_n19,(what, arg, ##__VA_ARGS__))
#define FE_n21(what, arg, a, ...)  what(20, arg, a) FE_CALLITn21(FE_n20,(what, arg, ##__VA_ARGS__))
#define FE_n22(...)           ERROR: FOR_EACH only supports up to 21 arguments
#define FE_GET_MACRO(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,NAME,...) NAME
#define FOR_EACH(what, arg, ...) FE_CALLITn01(FE_GET_MACRO(_0, ##__VA_ARGS__,FE_n22,FE_n21,FE_n20,FE_n19, \
                            FE_n18,FE_n17,FE_n16,FE_n15,FE_n14,FE_n13,FE_n12,FE_n11,FE_n10,FE_n09,\
FE_n08,FE_n07,FE_n06,FE_n05,FE_n04,FE_n03,FE_n02,FE_n01,FE_n00), (what, arg, ##__VA_ARGS__))

#define DECLTYPE(idx, _, property) \
IF_ELSE(idx != 0)(decltype(property),)(decltype(property))

#define CASE(idx, arg, property) \
    case change::property: {\
        auto p = property_change_##property(); \
        realm::sdk::set_change_values<property_change_##property, decltype(property)>(p, std::get<0>(arg), std::get<1>(arg)); \
        return p; \
    }

#define IF_NAME_EQUAL_TO_PROPERTY_RETURN_CHANGE(idx, name, property) \
if (name == #property) { \
    return Property::property; \
}

/**
 MARK: REALM_PRIMARY_KEY
 Wrap a cpp type in a Realm Property specialization with a trait marking that it is the primary key.
 */
#define REALM_PRIMARY_KEY(arg)\
    using primary_key_type = arg; \
    realm::sdk::Property<arg, realm::sdk::util::type_traits::primary_key>

/**
 MARK: REALM
 Wrap a cpp type in a Realm Property specialization.
 For EmbeddedObjects, this wrapping will yield the original type, so that
 users can access its fields directly.
 */

#define REALM(arg) \
    realm::sdk::util::conditional_property_type<arg>::type

/**
 MARK: REALM_EXPORT
 Add metadata to an Object about the exported properties. This will add:
 - a method that provides access to a tuple of the exported properties
 - an inlined Schema generated based on the exported properties
 - an enum listing the Properties so that user's can better understand what has changed (during observation)
 - an observe method to allow Object observation
 */
#define REALM_EXPORT(...) \
using has_primary_key = realm::sdk::util::has_primary_key<FOR_EACH(DECLTYPE, 0, __VA_ARGS__)>; \
auto primary_key_value() { return realm::sdk::util::deduce_primary_key<decltype(*this)>(__VA_ARGS__); } \
auto _properties() { \
    realm::sdk::ObjectBase<self::derived_t>::assign_names(#__VA_ARGS__, __VA_ARGS__); \
    return std::forward_as_tuple(__VA_ARGS__); \
} \
static const inline realm::ObjectSchema schema = realm::sdk::util::SchemaGenerator<self, std::tuple<FOR_EACH(DECLTYPE, 0, __VA_ARGS__)>>(#__VA_ARGS__).schema; \
enum class Property { __VA_ARGS__ }; \
\
static inline Property _change_for_name(const std::string& name) { \
    FOR_EACH(IF_NAME_EQUAL_TO_PROPERTY_RETURN_CHANGE, name, __VA_ARGS__); \
    return static_cast<Property>(0); \
} \
realm::sdk::NotificationToken observe(std::function<void(realm::sdk::PropertyChange<Property>, std::exception_ptr)>&& callback) { \
    return realm::sdk::ObjectBase<self::derived_t>::observe<Property>(std::move(callback)); \
}

#endif /* realm_sdk_h */
