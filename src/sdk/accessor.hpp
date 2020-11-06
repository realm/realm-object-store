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

#ifndef realm_sdk_accessor_h
#define realm_sdk_accessor_h

#include <string>
#include "../shared_realm.hpp"

namespace realm::sdk {

/**
 MARK: Property
 Specialization struct for Property Accessors.
 */
template <typename T, typename E = void>
struct Accessor;

/**
 MARK: AccessorBase

 Each Property contains an accessor (`AccessorBase`) that
 acts as a reader, writer, and translator between Core and Value
 types. E.g., an `Accessor<std::string>` would understand how to
 set and get core StringData and std::strings.
 */
struct AccessorBase {
    virtual ~AccessorBase() {}

    std::string column_name;

    virtual void attach(SharedRealm r, const TableRef& table, const ObjKey& key)
    {
        m_realm = r;
        m_table = table;
        m_key = key;
    }

    bool is_valid() const
    {
        return m_table && get_obj().is_valid();
    }

    Obj get_obj()
    {
        return !m_table ? Obj() : m_table->get_object(m_key);
    }

    ConstObj get_obj() const
    {
        return !m_table ? ConstObj() : m_table->get_object(m_key);
    }

    ColKey get_column_key() const
    {
        return !m_table ? ColKey() : m_table->get_column_key(column_name);
    }

    ObjKey get_obj_key() const
    {
        return m_key;
    }

    /**
     Explicitly convert a base Accessor to its appropriate specialization.
     */
    template <typename T>
    const Accessor<T>& as() const
    {
        return static_cast<const Accessor<T>&>(*this);
    }

    template <typename T>
    Accessor<T>& as()
    {
        return static_cast<Accessor<T>&>(*this);
    }
protected:
    SharedRealm realm() const
    {
        return m_realm;
    }

    SharedRealm m_realm;
    TableRef m_table;
    ObjKey m_key;
};

} // namespace realm::sdk

#endif /* accessor_h */
