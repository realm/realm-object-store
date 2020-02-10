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


#include "property.hpp"
#include <realm/util/assert.hpp>

namespace realm {

const char* string_for_property_type(PropertyType type)
{
    if (is_array(type)) {
        if (type == PropertyType::LinkingObjects)
            return "linking objects";
        return "array";
    }
    switch (type & ~PropertyType::Flags) {
        case PropertyType::String: return "string";
        case PropertyType::Int: return "int";
        case PropertyType::Bool: return "bool";
        case PropertyType::Date: return "date";
        case PropertyType::Data: return "data";
        case PropertyType::Double: return "double";
        case PropertyType::Float: return "float";
        case PropertyType::Object: return "object";
        case PropertyType::Any: return "any";
        case PropertyType::LinkingObjects: return "linking objects";
        default: REALM_COMPILER_HINT_UNREACHABLE();
    }
}

PropertyType from_core_type(DataType type)
{
    switch (type) {
        case type_Int:
            return PropertyType::Int;
        case type_Float:
            return PropertyType::Float;
        case type_Double:
            return PropertyType::Double;
        case type_Bool:
            return PropertyType::Bool;
        case type_String:
            return PropertyType::String;
        case type_Binary:
            return PropertyType::Data;
        case type_Timestamp:
            return PropertyType::Date;
        case type_OldMixed:
            return PropertyType::Any;
        case type_Link:
            return PropertyType::Object | PropertyType::Nullable;
        case type_LinkList:
            return PropertyType::Object | PropertyType::Array;
        default:
            REALM_UNREACHABLE();
    }
}

PropertyType from_core_type(ColKey col)
{
    auto flags = PropertyType::Required;
    auto attr = col.get_attrs();
    if (attr.test(col_attr_Nullable))
        flags |= PropertyType::Nullable;
    if (attr.test(col_attr_List))
        flags |= PropertyType::Array;

    PropertyType ret = from_core_type(DataType(col.get_type()));
    return ret | flags;
}

DataType to_core_type(PropertyType type)
{
    REALM_ASSERT(type != PropertyType::Object); // Link columns have to be handled differently
    REALM_ASSERT(type != PropertyType::Any);    // Mixed columns can't be created
    switch (type & ~PropertyType::Flags) {
        case PropertyType::Int:
            return type_Int;
        case PropertyType::Bool:
            return type_Bool;
        case PropertyType::Float:
            return type_Float;
        case PropertyType::Double:
            return type_Double;
        case PropertyType::String:
            return type_String;
        case PropertyType::Date:
            return type_Timestamp;
        case PropertyType::Data:
            return type_Binary;
        default:
            REALM_COMPILER_HINT_UNREACHABLE();
    }
    return type_Int; // Satisfy compiler
}

}
