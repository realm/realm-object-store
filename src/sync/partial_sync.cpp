////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
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

#include "sync/partial_sync.hpp"

#include "impl/notification_wrapper.hpp"
#include "impl/object_accessor_impl.hpp"
#include "object_schema.hpp"
#include "results.hpp"
#include "shared_realm.hpp"
#include "sync/sync_config.hpp"

#include <realm/util/scope_exit.hpp>

namespace realm {
namespace partial_sync {

namespace {

constexpr const char* result_sets_type_name = "__ResultSets";

void update_schema(Group& group, Property matches_property)
{
    Schema current_schema;
    std::string table_name = ObjectStore::table_name_for_object_type(result_sets_type_name);
    if (group.has_table(table_name))
        current_schema = {ObjectSchema{group, result_sets_type_name}};

    Schema desired_schema({
        ObjectSchema(result_sets_type_name, {
            {"matches_property", PropertyType::String},
            {"name", PropertyType::String},
            {"query", PropertyType::String},
            {"status", PropertyType::Int},
            {"error_message", PropertyType::String},
            {"query_parse_counter", PropertyType::Int},
            std::move(matches_property)
        })
    });
    auto required_changes = current_schema.compare(desired_schema);
    if (!required_changes.empty())
        ObjectStore::apply_additive_changes(group, required_changes, true);
}

util::Optional<Object> validate_existing_subscription(std::shared_ptr<Realm> realm,
                                                      std::string const& name,
                                                      std::string const& query,
                                                      std::string const& matches_property,
                                                      ObjectSchema const& result_sets_schema)
{
    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), result_sets_type_name);
    size_t name_idx = result_sets_schema.property_for_name("name")->table_column;
    auto existing_row_ndx = table->find_first_string(name_idx, name);
    if (existing_row_ndx == npos)
        return util::none;

    Object existing_object(realm, result_sets_schema, table->get(existing_row_ndx));

    CppContext context;
    std::string existing_query = any_cast<std::string>(existing_object.get_property_value<util::Any>(context, "query"));
    if (existing_query != query)
        throw std::runtime_error("An existing subscription exists with the same name, but a different query.");

    std::string existing_matches_property = any_cast<std::string>(existing_object.get_property_value<util::Any>(context, "matches_property"));
    if (existing_matches_property != matches_property)
        throw std::runtime_error("An existing subscription exists with the same name, but a different result type.");

    return existing_object;
}

} // unnamed namespace

void register_query(std::shared_ptr<Realm> realm, const std::string &object_class, const std::string &query,
                    std::function<void (Results, std::exception_ptr)> callback)
{
    auto sync_config = realm->config().sync_config;
    if (!sync_config || !sync_config->is_partial)
        throw std::logic_error("A partial sync query can only be registered in a partially synced Realm");

    if (realm->schema().find(object_class) == realm->schema().end())
        throw std::logic_error("A partial sync query can only be registered for a type that exists in the Realm's schema");

    auto matches_property = object_class + "_matches";

    // The object schema must outlive `object` below.
    std::unique_ptr<ObjectSchema> result_sets_schema;
    Object raw_object;
    {
        realm->begin_transaction();
        auto cleanup = util::make_scope_exit([&]() noexcept {
            if (realm->is_in_transaction())
                realm->cancel_transaction();
        });

        update_schema(realm->read_group(),
                      Property(matches_property, PropertyType::Object|PropertyType::Array, object_class));

        result_sets_schema = std::make_unique<ObjectSchema>(realm->read_group(), result_sets_type_name);

        CppContext context;
        raw_object = Object::create<util::Any>(context, realm, *result_sets_schema,
                                               AnyDict{
                                                   {"matches_property", matches_property},
                                                   {"query", query},
                                                   {"status", int64_t(0)},
                                                   {"error_message", std::string()},
                                                   {"query_parse_counter", int64_t(0)},
                                               }, false);

        realm->commit_transaction();
    }

    auto object = std::make_shared<_impl::NotificationWrapper<Object>>(std::move(raw_object));

    // Observe the new object and notify listener when the results are complete (status != 0).
    auto notification_callback = [object, matches_property,
                                  result_sets_schema=std::move(result_sets_schema),
                                  callback=std::move(callback)](CollectionChangeSet, std::exception_ptr error) mutable {
        if (error) {
            callback(Results(), error);
            object.reset();
            return;
        }

        CppContext context;
        auto status = any_cast<int64_t>(object->get_property_value<util::Any>(context, "status"));
        if (status == 0) {
            // Still computing...
            return;
        } else if (status == 1) {
            // Finished successfully.
            auto list = any_cast<List>(object->get_property_value<util::Any>(context, matches_property));
            callback(list.as_results(), nullptr);
        } else {
            // Finished with error.
            auto message = any_cast<std::string>(object->get_property_value<util::Any>(context, "error_message"));
            callback(Results(), std::make_exception_ptr(std::runtime_error(std::move(message))));
        }
        object.reset();
    };
    object->add_notification_callback(std::move(notification_callback));
}

void subscribe(Results results, util::Optional<std::string> user_provided_name, std::function<void(bool, std::exception_ptr)> callback)
{
    auto realm = results.get_realm();

    auto sync_config = realm->config().sync_config;
    if (!sync_config || !sync_config->is_partial)
        throw std::logic_error("A partial sync query can only be registered in a partially synced Realm");

    auto matches_property = std::string(results.get_object_type()) + "_matches";

    // The object schema must outlive `object` below.
    std::unique_ptr<ObjectSchema> result_sets_schema;
    Object raw_object;
    bool found_existing_subscription = false;
    {
        // FIXME: I suspect we want this to work when already inside a write transaction?
        realm->begin_transaction();
        auto cleanup = util::make_scope_exit([&]() noexcept {
            if (realm->is_in_transaction())
                realm->cancel_transaction();
        });

        update_schema(realm->read_group(),
                      Property(matches_property, PropertyType::Object|PropertyType::Array, results.get_object_type()));

        result_sets_schema = std::make_unique<ObjectSchema>(realm->read_group(), result_sets_type_name);

        auto query = results.get_query().get_description();
        std::string name = std::move(user_provided_name).value_or(query);

        if (auto existing_subscription = validate_existing_subscription(realm, name, query,
                                                                        matches_property, *result_sets_schema)) {
            raw_object = std::move(*existing_subscription);
            found_existing_subscription = true;
        }
        else {
            CppContext context;
            raw_object = Object::create<util::Any>(context, realm, *result_sets_schema,
                                                   AnyDict{
                                                       {"matches_property", matches_property},
                                                       {"name", name},
                                                       {"query", query},
                                                       {"status", int64_t(0)},
                                                       {"error_message", std::string()},
                                                       {"query_parse_counter", int64_t(0)},
                                                   }, false);
        }

        realm->commit_transaction();
    }

    auto object = std::make_shared<_impl::NotificationWrapper<Object>>(std::move(raw_object));

    // Observe the new object and notify listener when the results are complete (status != 0).
    auto notification_callback = [object, result_sets_schema=std::move(result_sets_schema),
                                  callback=std::move(callback)](CollectionChangeSet, std::exception_ptr error) mutable {
        if (error) {
            callback(false, error);
            object.reset();
            return;
        }

        CppContext context;
        auto status = any_cast<int64_t>(object->get_property_value<util::Any>(context, "status"));
        if (status == 0) {
            // Still computing...
            return;
        } else if (status == 1) {
            // Finished successfully.
            callback(true, nullptr);
        } else {
            // Finished with error.
            auto message = any_cast<std::string>(object->get_property_value<util::Any>(context, "error_message"));
            callback(true, std::make_exception_ptr(std::runtime_error(std::move(message))));
        }
        object.reset();
    };

    // If we found an existing subscription, call the callback immediately so that
    // our caller is aware of the existing state of the subscription.
    // FIXME: Do we need to do this on a runloop iteration?
    if (found_existing_subscription)
        notification_callback(CollectionChangeSet{}, nullptr);

    object->add_notification_callback(std::move(notification_callback));
}

} // namespace partial_sync
} // namespace realm
