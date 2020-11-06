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

#ifndef realm_sdk_realm_hpp
#define realm_sdk_realm_hpp

#include <stdio.h>
#include "../shared_realm.hpp"
#include "results.hpp"
#include "../object_store.hpp"
#include "util.hpp"
#include "schema_generator.hpp"

namespace realm::sdk {

struct Realm {
    Realm(realm::Realm::Config& config)
    {
        if (!config.schema) {
            // if there is no schema supplied,
            // use the SchemaGenerator schemas
            config.schema = Schema(util::get_schemas());
        }

        m_realm = realm::Realm::get_shared_realm(config);
    }

    Realm(SharedRealm realm)
    : m_realm(realm)
    {
    }

    template <typename T>
    realm::util::Optional<T> get_object(const typename T::primary_key_type& primary_key_value);

    template <typename T>
    Results<T> get_objects()
    {
        return Results<T>(m_realm);
    }

    template <typename T>
    T& add_object(T& object);

    template <typename T>
    T create_object()
    {
        auto class_name = util::demangle(typeid(T).name());
        auto table = ObjectStore::table_for_object_type(m_realm->read_group(), class_name);
        Obj obj;
        auto object = T();
        if constexpr(T::has_primary_key::value) {
            auto pk = object.primary_key_value();
            obj = table->create_object_with_primary_key(pk);
        } else {
            obj = table->create_object();
        }

        object.attach_and_write(m_realm, table, obj.get_key());
        return object;
    }

    template <typename T>
    void remove_object(T& object) {
        auto class_name = util::demangle(typeid(T).name());
        auto table = ObjectStore::table_for_object_type(m_realm->read_group(), class_name);
        table->remove_object(object.get_obj_key());
    }

    template <typename T>
    void remove_objects() {
        auto class_name = util::demangle(typeid(T).name());
        auto table = ObjectStore::table_for_object_type(m_realm->read_group(), class_name);
        table->clear();
    }

    bool is_in_write_transaction() const
    {
        return m_realm->is_in_transaction();
    }

    void begin_transaction()
    {
        m_realm->begin_transaction();
    }

    void commit_transaction()
    {
        m_realm->commit_transaction();
    }

    void write(const std::function<void()>& block)
    {
        m_realm->begin_transaction();
        block();
        m_realm->commit_transaction();
    }

    const Schema& schema() const {
        return m_realm->schema();
    }

    const realm::Realm::Config& config() const {
        return m_realm->config();
    }
private:
    /// Backing SharedRealm
    SharedRealm m_realm;
};

template <typename T>
realm::util::Optional<T> Realm::get_object(const typename T::primary_key_type& primary_key_value) {
    static_assert(T::has_primary_key::value, "T does not declare a primary key.");
    auto table = ObjectStore::table_for_object_type(m_realm->read_group(),
                                                    util::demangle(typeid(T).name()));
    if (auto key = table->find_primary_key(primary_key_value)) {
        auto value = T();
        value.attach(m_realm, table, key);
        return value;
    }
    return realm::util::none;
}

template <typename T>
T& Realm::add_object(T& object)
{
    auto class_name = util::demangle(typeid(static_cast<T&>(object)).name());
    auto table = ObjectStore::table_for_object_type(m_realm->read_group(), class_name);
    Obj obj;
    if constexpr(T::has_primary_key::value) {
        auto pk = object.primary_key_value();
        obj = table->create_object_with_primary_key(pk);
    } else {
        obj = table->create_object();
    }

    object.attach_and_write(m_realm, table, obj.get_key());
    return object;
}

}
#endif /* realm_hpp */
