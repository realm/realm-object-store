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

#include "object_schema.hpp"
#include "results.hpp"
#include "shared_realm.hpp"
#include "subscription_state.hpp"
#include "impl/notification_wrapper.hpp"
#include "impl/object_accessor_impl.hpp"
#include "sync/sync_config.hpp"
#include "sync/sync_session.hpp"

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

} // unnamed namespace

std::string get_default_name(Query query) {
    // Include table name as part of key to be able to disambiguate the same query on different
    // tables.
    std::string object_class = query.get_table()->get_name().substr(6);
    return util::format("[%1] %2", object_class, query.get_description());
}

void register_query(Group& group,
                    std::string const& key,
                    std::string const& object_class,
                    std::string const& serialized_query,
                    _impl::ResultsNotifier& notifier)
{
    auto table = ObjectStore::table_for_object_type(group, "__ResultSets").get();
    size_t name_idx = table->get_column_index("name");
    size_t query_idx = table->get_column_index("query");
    size_t matches_property_idx = table->get_column_index("matches_property");
    size_t object_class_idx = table->get_column_index("object_class");

    // Verify that we are not attempting to create a subscription that conflicts with an existing
    // one. In that case, manually set the error code/message in the existing subscription.
    // This will cause the callback in both the original query and the new one to report the
    // "Conflicting names" error.
    // Ideally this check would have been a lot sooner, but it is impossible to do reliably
    // due to a write transaction being required here.
    // This also means that some flaky behaviour is introduced in how subscription errors are
    // reported. The sequence of events will either be: a) .initial -> .error or b) directly .error
    auto existing_row = table->find_first_string(table->get_column_index("name"), key);
    bool query_conflict = false;
    if (existing_row != npos) {
        if (table->get_string(table->get_column_index("object_class"), existing_row) != object_class)
            query_conflict = true;

        if (table->get_string(table->get_column_index("query"), existing_row) != serialized_query)
            query_conflict = true;
    }
    if (query_conflict) {
        // This will result in an empty write, which will still trigger the notification system,
        // this will in turn detect this message and report it to the user without having to
        // persist it.
        notifier.set_partial_sync_local_error_message(util::format("A different subscription already existed with the same name: %1", key));
        return;
    }

    // Create the subscription
    auto matches_result_property = object_class + "_matches";
    size_t row_idx = sync::create_object(group, *table);
    table->set_string(name_idx, row_idx, key);
    table->set_string(query_idx, row_idx, serialized_query);
    table->set_string(matches_property_idx, row_idx, matches_result_property);
    table->set_string(object_class_idx, row_idx, object_class);

    // If necessary, Add new schema field for keeping matches.
    if (table->get_column_index(matches_result_property) == realm::not_found) {
        TableRef target_table = ObjectStore::table_for_object_type(group, object_class);
        table->add_column_link(type_LinkList, matches_result_property, *target_table);
    }
}

void get_query_status(Group& group, std::string const& name,
                      SubscriptionState& new_state, std::string& error)
{
    TableRef table = ObjectStore::table_for_object_type(group, "__ResultSets");

    size_t row = table->find_first_string(table->get_column_index("name"), name);
    if (row == npos) {
        new_state = SubscriptionState::Uninitialized;
        error = "";
        return;
    }

    new_state = static_cast<SubscriptionState>(table->get_int(table->get_column_index("status"), row));
    error = table->get_string(table->get_column_index("error_message"), row);
}

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
                                                   {"name", query},
                                                   {"matches_property", matches_property},
                                                   {"query", query},
                                                   {"status", int64_t(0)},
                                                   {"error_message", std::string()},
                                                   {"query_parse_counter", int64_t(0)},
                                                   {"object_class", object_class},
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

} // namespace partial_sync
} // namespace realm
