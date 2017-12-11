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

#include "subscription_state.hpp"
#include "sync/partial_sync.hpp"
#include "impl/collection_change_builder.hpp"
#include "impl/notification_wrapper.hpp"
#include "impl/object_accessor_impl.hpp"
#include "object_schema.hpp"
#include "results.hpp"
#include "shared_realm.hpp"
#include "sync/sync_config.hpp"

#include <realm/util/scope_exit.hpp>

namespace realm {
namespace partial_sync {

SubscriptionState create_or_update_subscription(SharedGroup &sg, realm::_impl::CollectionChangeBuilder &changes, Query &query, SubscriptionState previous_state) {
#if REALM_ENABLE_SYNC
    // FIXME: Question: Should we report back an initial changeset here?
    // FIXME: Question: Is it problematic to do a write transaction here? Should we move it to a background thread`?

    SubscriptionState old_partial_sync_state = previous_state;
    SubscriptionState new_partial_sync_state = SubscriptionState::UNDEFINED;
    std::string partial_sync_error_message = "";

    // TODO: Determine how to create key. Right now we just used the serialized query (are there any
    // disadvantages to that?). Would it make sense to hash it (to keep it short), but then we need
    // to handle hash collisions as well.
    std::string key = "SELECT * FROM XXX"; // Awaiting a release with query.get_description();

    // TODO: It might change how the query is serialized. See https://realmio.slack.com/archives/C80PLGQ8Z/p1511797410000188
    std::string serialized_query = "SELECT * FROM XXX"; // Awaiting a release with query.get_description();

    LangBindHelper::promote_to_write(sg);
    Group& group = _impl::SharedGroupFriend::get_group(sg);
    // Check current state and create subscription if needed. Throw in an error is found:
    TableRef table = ObjectStore::table_for_object_type(group, "__ResultSets");
    size_t name_idx = table->get_descriptor()->get_column_index("name");
    size_t query_idx = table->get_descriptor()->get_column_index("query");
    size_t status_idx = table->get_descriptor()->get_column_index("status");
    size_t error_idx = table->get_descriptor()->get_column_index("error_message");
    size_t matches_property_idx = table->get_descriptor()->get_column_index("matches_property");
    TableView results = (key == table->column<StringData>(name_idx)).find_all(0);
    if (results.size() > 0) {
        // Subscription with that ID already exist. Verify that we are not trying to reuse an
        // existing name for a different query.
        REALM_ASSERT(results.size() == 1);
        auto obj = results.get(0);
        if (obj.get_string(query_idx) != serialized_query) {
            std::stringstream ss;
            ss << "Subscription cannot be created as another subscription already exists with the same name. ";
            ss << "Name: " << key << ". ";
            ss << "Existing query: " << obj.get_string(query_idx) << ". ";
            ss << "New query: " << serialized_query << ".";

            // Make an error trigger a notification
            // FIXME: If an old error happened, new errors will not be reported. Can this happen?
            old_partial_sync_state = previous_state;
            new_partial_sync_state = SubscriptionState::ERROR;
            partial_sync_error_message = ss.str();

        } else {
            // The same Subscription already exist, just update the changeset information.
            old_partial_sync_state = previous_state;
            new_partial_sync_state = realm::partial_sync::status_code_to_state(obj.get_int(status_idx));
            partial_sync_error_message = obj.get_string(error_idx);
        }
        LangBindHelper::rollback_and_continue_as_read(sg);

    } else {
        // Create the subscription
        Table* t = table.get();
        auto query_table_name = std::string(query.get_table().get()->get_name().substr(6));
        auto matches_result_property = query_table_name + "_matches";
        size_t row_idx = sync::create_object(group, *table);
        t->set_string(name_idx, row_idx, key);
        t->set_string(query_idx, row_idx, serialized_query);
        t->set_string(matches_property_idx, row_idx, matches_result_property);

        // If necessary, Add new schema field for keeping matches.
        if (t->get_column_index(matches_result_property) == realm::not_found) {
            TableRef target_table = ObjectStore::table_for_object_type(group, query_table_name);
            t->add_column_link(type_LinkList, matches_result_property, *target_table.get());
        }

        // Don't trigger notification for creating the subscription (by making old/new status the same)
        old_partial_sync_state = SubscriptionState::UNINITIALIZED;
        new_partial_sync_state = SubscriptionState::UNINITIALIZED;
        partial_sync_error_message = "";
        LangBindHelper::commit_and_continue_as_read(sg);
    }

    // Update the ChangeSet
    changes.set_old_partial_sync_state(old_partial_sync_state);
    changes.set_new_partial_sync_state(new_partial_sync_state);
    changes.set_partial_sync_error_message(partial_sync_error_message);
    return new_partial_sync_state;
#else
    return SubscriptionState::NOT_SUPPORTED;
#endif
}

namespace {

constexpr const char* result_sets_type_name = "__ResultSets";

void update_schema(Group&, Property)
{
/**
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
        **/
}

} // unnamed namespace

/**

bool subscribe_query(std::shared_ptr<Realm> realm, std::string id = nullptr, Query* query) {

    if (!realm->is_in_transaction())
        throw std::logic_error("Must be in a write transaction");

    auto sync_config = realm->config().sync_config;
    if (!sync_config || !sync_config->is_partial)
        throw std::logic_error("A subscription can only be created for a partially synced Realm.");
    
    std::string queryErrorMessage = query->validate();
    if (queryErrorMessage != "")
        throw std::logic_error("Invalid query: " + queryErrorMessage);









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


    s





    return true;
}
*/

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

} // namespace partial_sync
} // namespace realm
