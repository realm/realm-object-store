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

#ifndef realm_sdk_property_h
#define realm_sdk_property_h

#include "accessor.hpp"

namespace realm::sdk {

/**
 MARK: Property
 Specialization struct for Property facades.
 */
template <typename T, typename E = void>
struct Property;

/**
 MARK: PropertyBase
 Base struct for any Realm Property.

 Properties act as a facade for an underlying Realm type, and inherit
 `PropertyBase` from their `Property` specialization struct.

 Each Property contains an accessor (`AccessorBase`) that
 acts as a reader, writer, and translator between Core and Value
 types. E.g., an `Accessor<std::string>` would understand how to
 set and get core StringData and std::strings.

 Properties use the accessor to read/write actual values from/to
 the realm (once attached), and then emulate the functionality
 of their specialized type, e.g.,:

 ```
 struct Property<std::string> {
    Property& operator +=(const std::string& other_value)  {
        if (!is_managed()) {
            m_unmanaged_value += other_value;
            return *this;
        }
        auto specialized_accessor = m_accessor.as<std::string>();
        auto core_value = specialized_accessor.get();
        auto actual_value = specialized_accessor.to_value_type(core_value);
        specialized_accessor.set(other_value + actual_value);
        return *this;
    }
 }
 ```

 Properties generally hold a reference to an unmanaged version of
 the underlying type. If the Property is unmanaged, all requested
 operations should be conducted as normal on the underlying type.
 */
struct PropertyBase {
    /**
     Whether or not this property is managed by a Realm.
     @return true if managed, false if not
     */
    bool is_managed() const
    {
        return m_accessor.is_valid();
    }

    /**
     Attach this property to a given Realm.
     @param realm the realm this property is stored in.
     @param table the table of the enclosing object
     @param key the key of the enclosing object
     */
    virtual void attach(SharedRealm realm, TableRef table, ObjKey key)
    {
        m_accessor.attach(realm, table, key);
    }

    /**
     Attach this property to a given Realm. Write the unmanaged value to the Realm.
     @param realm the realm this property is stored in.
     @param table the table of the enclosing object
     @param key the key of the enclosing object
     */
    virtual void attach_and_write(SharedRealm realm, TableRef table, ObjKey key) = 0;

    /**
     Set the associated name of the property given a schema.
     */
    void set_column_name(const std::string& name)
    {
        m_accessor.column_name = name;
    }
protected:
    AccessorBase m_accessor;
};

// MARK: Equality Operators

template <typename T, typename E>
static inline bool operator==(const Property<T, E>& lhs, const Property<T, E>& rhs)
{
    return lhs.value() == rhs.value();
}
template <typename T, typename E>
static inline bool operator!=(const Property<T, E>& lhs, const Property<T, E>& rhs)
{
    return !(lhs == rhs);
}
template <typename T, typename E>
static inline bool operator==(const Property<T, E>& lhs, const T& rhs)
{
    return lhs.value() == rhs;
}
template <typename T, typename E>
static inline bool operator!=(const Property<T, E>& lhs, const T& rhs)
{
    return !(lhs == rhs);
}

}

namespace std {
template <typename T, typename V>
struct hash<realm::sdk::Property<T, V>> {
    size_t operator()(const realm::sdk::Property<T, V>& k) const
    {
        return (hash<T>()(k.value()));
    }
};
} // namespace std

#endif /* property_h */
