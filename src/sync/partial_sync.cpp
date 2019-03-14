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

#include "impl/collection_notifier.hpp"
#include "impl/notification_wrapper.hpp"
#include "impl/object_accessor_impl.hpp"
#include "impl/realm_coordinator.hpp"
#include "object_schema.hpp"
#include "results.hpp"
#include "shared_realm.hpp"
#include "sync/impl/work_queue.hpp"
#include "sync/subscription_state.hpp"
#include "sync/sync_config.hpp"
#include "sync/sync_session.hpp"

#include <realm/lang_bind_helper.hpp>
#include <realm/util/scope_exit.hpp>

#define __STDC_LIMIT_MACROS // See https://stackoverflow.com/a/3233069/1389357
#include <cstdint>

using namespace std::chrono;

namespace {

    // Delete all subscriptions that are no longer relevant.
    // This method must be called within a write transaction.
    void cleanup_subscriptions(realm::Group& group, realm::Timestamp now)
    {
        // Remove all subscriptions no longer considered "active"
        // "inactive" is currently defined as any subscription with an `expires_at` < now()`
        //
        // Note, that we do not check if someone is actively using the subscription right now (this
        // is also hard to get right). This does leave some loop holes where a subscription might be
        // removed while still in use. E.g. consider a Kiosk app showing a screen 24/7 with a background
        // job that accidentially triggers the cleanup codepath. This case is considered rare, but should
        // still be documented.
        auto table = realm::ObjectStore::table_for_object_type(group, realm::partial_sync::result_sets_type_name);

        size_t expires_at_col_ndx = table->get_column_index(realm::partial_sync::property_expires_at);
        realm::TableView results = table->where().less(expires_at_col_ndx, now).find_all();
        results.clear(realm::RemoveMode::unordered);
    }

    // FIXME: Replace these methods with: https://github.com/realm/realm-core/pull/3259

    realm::Timestamp timestamp_now()
    {
        auto now = std::chrono::system_clock::now();
        auto sec = std::chrono::time_point_cast<std::chrono::seconds>(now);
        auto ns = static_cast<int32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - sec).count());
        return {sec.time_since_epoch().count(), ns};
    }

    // Calculates the expiry date, claming at the high end if a timestamp overflows
    realm::Timestamp calculate_expiry_date(realm::Timestamp starting_time, int64_t user_ttl_ms)
    {

        // Short-circuit the common case and prevent a bunch of annoying edge cases in the below calculations
        // if a max value has been provided.
        if (user_ttl_ms == INT64_MAX) {
            return realm::Timestamp(INT64_MAX, realm::Timestamp::nanoseconds_per_second - 1);
        }

        // Get {sec, nanosec} pair representing `now`
        int64_t s_arg = starting_time.get_seconds();
        auto ns_arg = starting_time.get_nanoseconds();

        // Convert millisecond input to match Timestamp representation
        int64_t s_ttl = user_ttl_ms / 1000;
        auto ns_ttl = static_cast<int32_t>((user_ttl_ms % 1000) * 1000000);

        // Add user TTL to `now` but clamp at MAX if it overflows
        // Also handle the slightly complicated situation where the ns part doesn't overflow but
        // exceeds `nanoseconds_pr_second`.
        int64_t modified_s_arg = s_arg;
        int32_t modified_ns_arg = ns_arg + ns_ttl;

        // The nano-second part can never overflow the int32_t type itself as the maximum result
        // is `999.999.999ns + 999.999.999ns`, but we need to handle the case where it
        // exceeds `nanoseconds_pr_second`
        if (modified_ns_arg > realm::Timestamp::nanoseconds_per_second) {
            modified_s_arg++;
            modified_ns_arg = modified_ns_arg - realm::Timestamp::nanoseconds_per_second;
        }

        // Modify the seconds argument. Even if modified_ns_arg caused the addition of an extra second, we only
        // need to check for a normal overflow as the complicated case of INT64_MAX + 1 + INT64_MAX is
        // handled by the short-circuit at the top of this function.
        modified_s_arg = (modified_s_arg + s_ttl < modified_s_arg) ? INT64_MAX : modified_s_arg + s_ttl;

        return realm::Timestamp(modified_s_arg, modified_ns_arg);
    }
}

namespace realm {

namespace _impl {
using namespace ::realm::partial_sync;

void initialize_schema(Group& group)
{
    std::string result_sets_table_name = ObjectStore::table_name_for_object_type(result_sets_type_name);
    TableRef table = group.get_table(result_sets_table_name);
    if (!table) {
        // Create the schema required by Sync
        table = sync::create_table(group, result_sets_table_name);
        table->add_column(type_String, property_query);
        table->add_column(type_String, property_matches_property_name);
        table->add_column(type_Int, property_status);
        table->add_column(type_String, property_error_message);
        table->add_column(type_Int, property_query_parse_counter);
    }
    else {
        // The table already existed, so it should have all of the columns that are in the shared schema.
        REALM_ASSERT(table->get_column_index(property_query) != npos);
        REALM_ASSERT(table->get_column_index(property_matches_property_name) != npos);
        REALM_ASSERT(table->get_column_index(property_status) != npos);
        REALM_ASSERT(table->get_column_index(property_error_message) != npos);
        REALM_ASSERT(table->get_column_index(property_query_parse_counter) != npos);
    }

    // Add columns not required by Sync, but used by the bindings to offer better tracking of subscriptions.
    // These columns are not automatically added by the server, so we need to add them manually if needed.

    // Name used to uniquely identify a subscription. If a name isn't provided for a subscription one will be
    // autogenerated.
    if (table->get_column_index(property_name) == npos) {
        size_t idx = table->add_column(type_String, property_name);
        table->add_search_index(idx);
    }

    // Timestamp for when then the subscription is created. This should only be set the first time the subscription
    // is created.
    if (table->get_column_index(property_created_at) == npos) {
        table->add_column(type_Timestamp, property_created_at);
    }

    // Timestamp for when the subscription is either updated or someone resubscribes to it.
    if (table->get_column_index(property_updated_at) == npos) {
        table->add_column(type_Timestamp, property_updated_at);
    }

    // Relative time-to-live in milliseconds. This indicates the period from when a subscription was last updated
    // to when it isn't considered valid anymore and can be safely deleted. Realm will attempt to perform this
    // cleanup automatically either when the app is started or someone discards the subscription token for it.
    if (table->get_column_index(property_time_to_live) == npos) {
        table->add_column(type_Int, property_time_to_live, true); // null = infinite TTL
    }

    // Timestamp representing the fixed point in time when this subscription isn't valid anymore and can
    // be safely deleted. This value should be considered read-only from the perspective of any Bindings
    // and should never be modified by itself, but only updated whenever the `updatedAt` or `timefield is.
    if (table->get_column_index(property_expires_at) == npos) {
        table->add_column(type_Timestamp, property_expires_at, true); // null = Subscription never expires
    }

    // Remove any subscriptions no longer relevant
    cleanup_subscriptions(group, timestamp_now());
}

// A stripped-down version of WriteTransaction that can promote an existing read transaction
// and that notifies the sync session after committing a change.
class WriteTransactionNotifyingSync {
public:
    WriteTransactionNotifyingSync(Realm::Config const& config, SharedGroup& sg)
    : m_config(config)
    , m_shared_group(&sg)
    {
        if (m_shared_group->get_transact_stage() == SharedGroup::transact_Reading)
            LangBindHelper::promote_to_write(*m_shared_group);
        else
            m_shared_group->begin_write();
    }

    ~WriteTransactionNotifyingSync()
    {
        if (m_shared_group)
            m_shared_group->rollback();
    }

    SharedGroup::version_type commit()
    {
        REALM_ASSERT(m_shared_group);
        auto version = m_shared_group->commit();
        m_shared_group = nullptr;

        auto session = SyncManager::shared().get_session(m_config.path, *m_config.sync_config);
        SyncSession::Internal::nonsync_transact_notify(*session, version);
        return version;
    }

    void rollback()
    {
        REALM_ASSERT(m_shared_group);
        m_shared_group->rollback();
        m_shared_group = nullptr;
    }

    Group& get_group() const noexcept
    {
        REALM_ASSERT(m_shared_group);
        return _impl::SharedGroupFriend::get_group(*m_shared_group);
    }

private:
    Realm::Config const& m_config;
    SharedGroup* m_shared_group;
};

// Provides a convenient way for code in this file to access private details of `Realm`
// without having to add friend declarations for each individual use.
class PartialSyncHelper {
public:
    static decltype(auto) get_shared_group(Realm& realm)
    {
        return Realm::Internal::get_shared_group(realm);
    }

    static decltype(auto) get_coordinator(Realm& realm)
    {
        return Realm::Internal::get_coordinator(realm);
    }
};

struct RowHandover {
    RowHandover(Realm& realm, Row row)
    : source_shared_group(*PartialSyncHelper::get_shared_group(realm))
    , row(source_shared_group.export_for_handover(std::move(row)))
    , version(source_shared_group.pin_version())
    {
    }

    ~RowHandover()
    {
        // If the row isn't already null we've not been imported and the version pin will leak.
        REALM_ASSERT(!row);
    }

    SharedGroup& source_shared_group;
    std::unique_ptr<SharedGroup::Handover<Row>> row;
    VersionID version;
};

} // namespace _impl

namespace partial_sync {

InvalidRealmStateException::InvalidRealmStateException(const std::string& msg)
: std::logic_error(msg)
{}

ExistingSubscriptionException::ExistingSubscriptionException(const std::string& msg)
: std::runtime_error(msg)
{}

QueryTypeMismatchException::QueryTypeMismatchException(const std::string& msg)
: std::logic_error(msg)
{}

namespace {

template<typename F>
void with_open_shared_group(Realm::Config const& config, F&& function)
{
    std::unique_ptr<Replication> history;
    std::unique_ptr<SharedGroup> sg;
    std::unique_ptr<Group> read_only_group;
    Realm::open_with_config(config, history, sg, read_only_group, nullptr);

    function(*sg);
}

struct ResultSetsColumns {
    ResultSetsColumns(Table& table, std::string const& matches_property_name)
    {
        name = table.get_column_index(property_name);
        REALM_ASSERT(name != npos);

        query = table.get_column_index(property_query);
        REALM_ASSERT(query != npos);

        this->matches_property_name = table.get_column_index(property_matches_property_name);
        REALM_ASSERT(this->matches_property_name != npos);

        created_at = table.get_column_index(property_created_at);
        REALM_ASSERT(created_at != npos);

        updated_at = table.get_column_index(property_updated_at);
        REALM_ASSERT(updated_at != npos);

        expires_at = table.get_column_index(property_expires_at);
        REALM_ASSERT(expires_at != npos);

        time_to_live = table.get_column_index(property_time_to_live);
        REALM_ASSERT(time_to_live != npos);

        // This may be `npos` if the column does not yet exist.
        matches_property = table.get_column_index(matches_property_name);
    }

    size_t name;
    size_t query;
    size_t matches_property_name;
    size_t matches_property;
    size_t created_at;
    size_t updated_at;
    size_t expires_at;
    size_t time_to_live;
};

// Performs the logic of actually writing the subscription (if needed) to the Realm and making sure
// that the `matches_property` field is setup correctly. This method will throw if the query cannot
// be serialized or the name is already used by another subscription.
//
// The row of the resulting subscription is returned. If an old subscription exists that matches
// the one about to be created, a new subscription is not created, but the old one is returned
// instead.
//
// If `update = true` and  if a subscription with `name` already exists, its query and time_to_live
// will be updated instead of an exception being thrown if the query parsed in was different than
// the persisted query.
Row write_subscription(std::string const& object_type, std::string const& name, std::string const& query,
        util::Optional<int64_t> time_to_live_ms, bool update, Group& group)
{
    Timestamp now = timestamp_now();
    auto matches_property = std::string(object_type) + "_matches";

    auto table = ObjectStore::table_for_object_type(group, result_sets_type_name);
    ResultSetsColumns columns(*table, matches_property);

    // Update schema if needed.
    if (columns.matches_property == npos) {
        auto target_table = ObjectStore::table_for_object_type(group, object_type);
        columns.matches_property = table->add_column_link(type_LinkList, matches_property, *target_table);
    }
    else {
        // FIXME: Validate that the column type and link target are correct.
    }

    // Find existing subscription (if any)
    auto row_ndx = table->find_first_string(columns.name, name);
    if (row_ndx != npos) {

        // Check that we don't attempt to replace an existing query with a query on a new type.
        // There is nothing that prevents Sync from handling this, but allowing it will complicate
        // Binding API's, so for now it is disallowed.
        auto existing_matching_property = table->get_string(columns.matches_property_name, row_ndx);
        if (existing_matching_property != matches_property) {
            throw QueryTypeMismatchException(util::format("Replacing an existing query with a query on "
                                                          "a different type is not allowed: %1 vs. %2 for %3",
                                                          existing_matching_property, matches_property, name));
        }

        // If an subscription exist, we only update the query and TTL if allowed to.
        // TODO: Consider how Binding API's are going to use this. It might make sense to disallow
        // updating TTL using this API and instead require updates to TTL to go through a managed Subscription.
        if (update) {
            table->set_string(columns.query, row_ndx, query);
            table->set(columns.time_to_live, row_ndx, time_to_live_ms);
        }
        else {
            StringData existing_query = table->get_string(columns.query, row_ndx);
            if (existing_query != query)
                throw ExistingSubscriptionException(util::format("An existing subscription exists with the name '%1' "
                                                                 "but with a different query: '%1' vs '%2'",
                                                                 name, existing_query, query));
        }

    }
    else {
        // No existing subscription was found. Create a new one
        row_ndx = sync::create_object(group, *table);
        table->set_string(columns.name, row_ndx, name);
        table->set_string(columns.query, row_ndx, query);
        table->set_string(columns.matches_property_name, row_ndx, matches_property);
        table->set_timestamp(columns.created_at, row_ndx, now);
        if (time_to_live_ms) {
            table->set_int(columns.time_to_live, row_ndx, time_to_live_ms.value());
        }
        else {
            table->set_null(columns.time_to_live, row_ndx);
        }
    }

    // Always set updated_at/expires_at when a subscription is touched, no matter if it is new, updated or someone just
    // resubscribes.
    table->set_timestamp(columns.updated_at, row_ndx, now);
    if (table->is_null(columns.time_to_live, row_ndx)) {
        table->set_null(columns.expires_at, row_ndx);
    }
    else {
        table->set_timestamp(columns.expires_at, row_ndx, calculate_expiry_date(now, table->get_int(columns.time_to_live, row_ndx)));
    }

    // Fetch subscription first and return it. Cleanup needs to be performed after as it might delete subscription
    // causing the row_ndx to change.
    Row subscription = table->get(row_ndx);
    cleanup_subscriptions(group, now);
    return subscription;
}

void enqueue_registration(Realm& realm, std::string object_type, std::string query, std::string name, util::Optional<int64_t> time_to_live,
        bool update, std::function<void(std::exception_ptr)> callback)
{
    auto config = realm.config();

    auto& work_queue = _impl::PartialSyncHelper::get_coordinator(realm).partial_sync_work_queue();
    work_queue.enqueue([object_type=std::move(object_type), query=std::move(query), name=std::move(name),
                        callback=std::move(callback), config=std::move(config), time_to_live=std::move(time_to_live), update=update] {
        try {
            with_open_shared_group(config, [&](SharedGroup& sg) {
                _impl::WriteTransactionNotifyingSync write(config, sg);
                write_subscription(object_type, name, query, time_to_live, update, write.get_group());
                write.commit();
            });
        } catch (...) {
            callback(std::current_exception());
            return;
        }

        callback(nullptr);
    });
}

void enqueue_unregistration(Object result_set, std::function<void()> callback)
{
    auto realm = result_set.realm();
    auto config = realm->config();
    auto& work_queue = _impl::PartialSyncHelper::get_coordinator(*realm).partial_sync_work_queue();

    // Export a reference to the __ResultSets row so we can hand it to the worker thread.
    // We store it in a shared_ptr as it would otherwise prevent the lambda from being copyable,
    // which `std::function` requires.
    auto handover = std::make_shared<_impl::RowHandover>(*realm, result_set.row());

    work_queue.enqueue([handover=std::move(handover), callback=std::move(callback),
                        config=std::move(config)] () {
        with_open_shared_group(config, [&](SharedGroup& sg) {
            // Import handed-over object.
            sg.begin_read(handover->version);
            Row row = *sg.import_from_handover(std::move(handover->row));
            sg.unpin_version(handover->version);

            _impl::WriteTransactionNotifyingSync write(config, sg);
            if (row.is_attached()) {
                row.move_last_over();
                write.commit();
            }
            else {
                write.rollback();
            }
        });
        callback();
    });
}

std::string default_name_for_query(const std::string& query, const std::string& object_type)
{
    return util::format("[%1] %2", object_type, query);
}

} // unnamed namespace


struct Subscription::Notifier : public _impl::CollectionNotifier {
    enum State {
        Creating,
        Complete,
        Removed,
    };

    Notifier(std::shared_ptr<Realm> realm)
    : _impl::CollectionNotifier(std::move(realm))
    , m_coordinator(&_impl::PartialSyncHelper::get_coordinator(*get_realm()))
    {
    }

    void release_data() noexcept override { }
    void run() override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_has_results_to_deliver) {
            // Mark the object as being modified so that CollectionNotifier is aware
            // that there are changes to deliver.
            m_changes.modify(0);
        }
    }

    void deliver(SharedGroup&) override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_error = m_pending_error;
        m_pending_error = nullptr;

        m_state = m_pending_state;
        m_has_results_to_deliver = false;
    }

    void finished_subscribing(std::exception_ptr error)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_pending_error = error;
            m_pending_state = Complete;
            m_has_results_to_deliver = true;
        }

        // Trigger processing of change notifications.
        m_coordinator->wake_up_notifier_worker();
    }

    void finished_unsubscribing()
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_pending_state = Removed;
            m_has_results_to_deliver = true;
        }

        // Trigger processing of change notifications.
        m_coordinator->wake_up_notifier_worker();
    }

    std::exception_ptr error() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_error;
    }

    State state() const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_state;
    }

private:
    void do_attach_to(SharedGroup&) override { }
    void do_detach_from(SharedGroup&) override { }

    void do_prepare_handover(SharedGroup&) override
    {
        add_changes(std::move(m_changes));
    }

    bool do_add_required_change_info(_impl::TransactionChangeInfo&) override { return false; }
    bool prepare_to_deliver() override { return m_has_results_to_deliver; }

    _impl::RealmCoordinator *m_coordinator;

    mutable std::mutex m_mutex;
    _impl::CollectionChangeBuilder m_changes;
    std::exception_ptr m_pending_error = nullptr;
    std::exception_ptr m_error = nullptr;
    bool m_has_results_to_deliver = false;

    State m_state = Creating;
    State m_pending_state = Creating;
};

Subscription subscribe(Results const& results, util::Optional<std::string> user_provided_name, util::Optional<int64_t> time_to_live_ms, bool update)
{
    auto realm = results.get_realm();

    auto sync_config = realm->config().sync_config;
    if (!sync_config || !sync_config->is_partial)
        throw InvalidRealmStateException("A Subscription can only be created in a Query-based Realm.");

    auto query = results.get_query().get_description(); // Throws if the query cannot be serialized.
    query += " " + results.get_descriptor_ordering().get_description(results.get_query().get_table());

    std::string name = user_provided_name ? std::move(*user_provided_name)
                                          : default_name_for_query(query, results.get_object_type());

    Subscription subscription(name, results.get_object_type(), realm);
    std::weak_ptr<Subscription::Notifier> weak_notifier = subscription.m_notifier;
    enqueue_registration(*realm, results.get_object_type(), std::move(query), std::move(name), std::move(time_to_live_ms), update,
                         [weak_notifier=std::move(weak_notifier)](std::exception_ptr error) {
        if (auto notifier = weak_notifier.lock())
            notifier->finished_subscribing(error);
    });
    return subscription;
}

Row subscribe_blocking(Results const& results, util::Optional<std::string> user_provided_name, util::Optional<int64_t> time_to_live_ms, bool update)
{

    auto realm = results.get_realm();
    if (!realm->is_in_transaction()) {
        throw InvalidRealmStateException("The subscription can only be created inside a write transaction.");
    }
    auto sync_config = realm->config().sync_config;
    if (!sync_config || !sync_config->is_partial) {
        throw InvalidRealmStateException("A Subscription can only be created in a Query-based Realm.");
    }

    auto query = results.get_query().get_description(); // Throws if the query cannot be serialized.
    query += " " + results.get_descriptor_ordering().get_description(results.get_query().get_table());
    std::string name = user_provided_name ? std::move(*user_provided_name)
                                          : default_name_for_query(query, results.get_object_type());
    return write_subscription(results.get_object_type(), name, query, time_to_live_ms, update, realm->read_group());
}

void unsubscribe(Subscription& subscription)
{
    if (auto result_set_object = subscription.result_set_object()) {
        // The subscription has its result set object, so we can queue up the unsubscription immediately.
        std::weak_ptr<Subscription::Notifier> weak_notifier = subscription.m_notifier;
        enqueue_unregistration(*result_set_object, [weak_notifier=std::move(weak_notifier)]() {
            if (auto notifier = weak_notifier.lock())
                notifier->finished_unsubscribing();
        });
        return;
    }

    switch (subscription.state()) {
        case SubscriptionState::Creating: {
            // The result set object is in the process of being created. Try unsubscribing again once it exists.
            auto token = std::make_shared<SubscriptionNotificationToken>();
            *token = subscription.add_notification_callback([token, &subscription] () {
                if (subscription.state() == SubscriptionState::Creating)
                    return;

                unsubscribe(subscription);

                // Invalidate the notification token so we do not receive further callbacks.
                *token = SubscriptionNotificationToken();
            });
            return;
        }

        case SubscriptionState::Error:
            // We encountered an error when creating the subscription. There's nothing to remove, so just
            // mark the subscription as removed.
            subscription.m_notifier->finished_unsubscribing();
            break;

        case SubscriptionState::Invalidated:
            // Nothing to do. We have already removed the subscription.
            break;

        case SubscriptionState::Pending:
        case SubscriptionState::Complete:
            // This should not be reachable as these states require the result set object to exist.
            REALM_ASSERT(false);
            break;
    }
}

void unsubscribe(Object&& subscription)
{
    REALM_ASSERT(subscription.get_object_schema().name == result_sets_type_name);
    auto realm = subscription.realm();
    enqueue_unregistration(std::move(subscription), [=] {
        // The partial sync worker thread bypasses the normal machinery which
        // would trigger notifications since it does its own notification things
        // in the other cases, so manually trigger it here.
        _impl::PartialSyncHelper::get_coordinator(*realm).wake_up_notifier_worker();
    });
}

Subscription::Subscription(std::string name, std::string object_type, std::shared_ptr<Realm> realm)
: m_object_schema(realm->read_group(), result_sets_type_name)
{
    // FIXME: Why can't I do this in the initializer list?
    m_notifier = std::make_shared<Notifier>(realm);
    _impl::RealmCoordinator::register_notifier(m_notifier);

    auto matches_property = std::string(object_type) + "_matches";

    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), result_sets_type_name);
    Query query = table->where();
    query.equal(m_object_schema.property_for_name("name")->table_column, name);
    query.equal(m_object_schema.property_for_name("matches_property")->table_column, matches_property);
    m_result_sets = Results(std::move(realm), std::move(query));
}

Subscription::~Subscription() = default;
Subscription::Subscription(Subscription&&) = default;
Subscription& Subscription::operator=(Subscription&&) = default;

SubscriptionNotificationToken Subscription::add_notification_callback(std::function<void ()> callback)
{
    auto result_sets_token = m_result_sets.add_notification_callback([callback] (CollectionChangeSet, std::exception_ptr) {
        callback();
    });
    NotificationToken registration_token(m_notifier, m_notifier->add_callback([callback] (CollectionChangeSet, std::exception_ptr) {
        callback();
    }));
    return SubscriptionNotificationToken{std::move(registration_token), std::move(result_sets_token)};
}

util::Optional<Object> Subscription::result_set_object() const
{
    if (m_notifier->state() == Notifier::Complete) {
        if (auto row = m_result_sets.first())
            return Object(m_result_sets.get_realm(), m_object_schema, *row);
    }

    return util::none;
}

SubscriptionState Subscription::state() const
{
    switch (m_notifier->state()) {
        case Notifier::Creating:
            return SubscriptionState::Creating;
        case Notifier::Removed:
            return SubscriptionState::Invalidated;
        case Notifier::Complete:
            break;
    }

    if (m_notifier->error())
        return SubscriptionState::Error;

    if (auto object = result_set_object()) {
        CppContext context;
        auto value = any_cast<int64_t>(object->get_property_value<util::Any>(context, "status"));
        return (SubscriptionState)value;
    }

    // We may not have an object even if the subscription has completed if the completion callback fired
    // but the result sets callback is yet to fire.
    return SubscriptionState::Creating;
}

std::exception_ptr Subscription::error() const
{
    if (auto error = m_notifier->error())
        return error;

    if (auto object = result_set_object()) {
        CppContext context;
        auto message = any_cast<std::string>(object->get_property_value<util::Any>(context, "error_message"));
        if (message.size())
            return make_exception_ptr(std::runtime_error(message));
    }

    return nullptr;
}

} // namespace partial_sync
} // namespace realm
