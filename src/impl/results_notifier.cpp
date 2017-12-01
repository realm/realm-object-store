////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
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

#include "impl/results_notifier.hpp"

#include "shared_realm.hpp"
#include <sstream>

using namespace realm;
using namespace realm::_impl;

ResultsNotifier::ResultsNotifier(Results& target)
: CollectionNotifier(target.get_realm())
, m_target_results(&target)
, m_target_is_in_table_order(target.is_in_table_order())
{
    Query q = target.get_query();
    set_table(*q.get_table());
    m_query_handover = source_shared_group().export_for_handover(q, MutableSourcePayload::Move);
    DescriptorOrdering::generate_patch(target.get_descriptor_ordering(), m_ordering_handover);
}

void ResultsNotifier::target_results_moved(Results& old_target, Results& new_target)
{
    auto lock = lock_target();

    REALM_ASSERT(m_target_results == &old_target);
    m_target_results = &new_target;
}

void ResultsNotifier::release_data() noexcept
{
    m_query = nullptr;
}

// Most of the inter-thread synchronization for run(), prepare_handover(),
// attach_to(), detach(), release_data() and deliver() is done by
// RealmCoordinator external to this code, which has some potentially
// non-obvious results on which members are and are not safe to use without
// holding a lock.
//
// add_required_change_info(), attach_to(), detach(), run(),
// prepare_handover(), and release_data() are all only ever called on a single
// background worker thread. call_callbacks() and deliver() are called on the
// target thread. Calls to prepare_handover() and deliver() are guarded by a
// lock.
//
// In total, this means that the safe data flow is as follows:
//  - add_Required_change_info(), prepare_handover(), attach_to(), detach() and
//    release_data() can read members written by each other
//  - deliver() can read members written to in prepare_handover(), deliver(),
//    and call_callbacks()
//  - call_callbacks() and read members written to in deliver()
//
// Separately from the handover data flow, m_target_results is guarded by the target lock

bool ResultsNotifier::do_add_required_change_info(TransactionChangeInfo& info)
{
    REALM_ASSERT(m_query);
    m_info = &info;

    auto& table = *m_query->get_table();
    if (!table.is_attached())
        return false;

    auto table_ndx = table.get_index_in_group();
    if (table_ndx == npos) { // is a subtable
        auto& parent = *table.get_parent_table();
        size_t row_ndx = table.get_parent_row_index();
        size_t col_ndx = find_container_column(parent, row_ndx, &table, type_Table, &Table::get_subtable);
        info.lists.push_back({parent.get_index_in_group(), row_ndx, col_ndx, &m_changes});
    }
    else { // is a top-level table
        if (info.table_moves_needed.size() <= table_ndx)
            info.table_moves_needed.resize(table_ndx + 1);
        info.table_moves_needed[table_ndx] = true;
    }

    return has_run() && have_callbacks();
}

bool ResultsNotifier::need_to_run()
{
    REALM_ASSERT(m_info);
    REALM_ASSERT(!m_tv.is_attached());

    {
        auto lock = lock_target();
        // Don't run the query if the results aren't actually going to be used
        if (!get_realm() || (!have_callbacks() && !m_target_results->wants_background_updates())) {
            return false;
        }
    }

    // If we've run previously, check if we need to rerun
    if (has_run() && m_query->sync_view_if_needed() == m_last_seen_version) {
        return false;
    }

    return true;
}

void ResultsNotifier::calculate_changes()
{
    size_t table_ndx = m_query->get_table()->get_index_in_group();
    if (has_run()) {
        CollectionChangeBuilder* changes = nullptr;
        if (table_ndx == npos)
            changes = &m_changes;
        else if (table_ndx < m_info->tables.size())
            changes = &m_info->tables[table_ndx];

        std::vector<size_t> next_rows;
        next_rows.reserve(m_tv.size());
        for (size_t i = 0; i < m_tv.size(); ++i)
            next_rows.push_back(m_tv[i].get_index());

        util::Optional<IndexSet> move_candidates;
        if (changes) {
            auto const& moves = changes->moves;
            for (auto& idx : m_previous_rows) {
                if (changes->deletions.contains(idx)) {
                    // check if this deletion was actually a move
                    auto it = lower_bound(begin(moves), end(moves), idx,
                                          [](auto const& a, auto b) { return a.from < b; });
                    idx = it != moves.end() && it->from == idx ? it->to : npos;
                }
                else
                    idx = changes->insertions.shift(changes->deletions.unshift(idx));
            }
            if (m_target_is_in_table_order && !m_descriptor_ordering.will_apply_sort())
                move_candidates = changes->insertions;
        }

        m_changes = CollectionChangeBuilder::calculate(m_previous_rows, next_rows,
                                                       get_modification_checker(*m_info, *m_query->get_table()),
                                                       move_candidates);

        m_previous_rows = std::move(next_rows);
    }
    else {
        m_previous_rows.resize(m_tv.size());
        for (size_t i = 0; i < m_tv.size(); ++i)
            m_previous_rows[i] = m_tv[i].get_index();
    }
}

void ResultsNotifier::run()
{
    // Table's been deleted, so report all rows as deleted
    if (!m_query->get_table()->is_attached()) {
        m_changes = {};
        m_changes.deletions.set(m_previous_rows.size());
        m_previous_rows.clear();
        return;
    }

    if (!need_to_run())
        return;


    // Potential optimization: Ship the write to another background thread to prevent
    // blocking the notifiers?

    // TODO Partial Sync is only supported on non-looper threads, but since it shouldn't be possible
    // to register listeners on threads without run loops we should only enter this code if
    // we actually support notifications and thus partial sync subscriptions.

    // While this would trigger notifications it should be safe as we are on the background worker
    // thread where no notifications exist.
    int8_t partial_sync_status_code = (m_partial_sync_enabled) ? 0 : -2;
    std::string partial_sync_error_message = nullptr;
    if (m_partial_sync_enabled) {
        bool cancel = false;
        m_realm->begin_transaction();

        // TODO: Determine how to create key. Right now we just used the serialized query (are there any
        // disadvantages to that?). Would it make sense to hash it (to keep it short), but then we need
        // to handle hash collisions as well.
        std::string key = m_query.get_description();

        // TODO: It might change how the query is serialized. See https://realmio.slack.com/archives/C80PLGQ8Z/p1511797410000188
        std::string serialized_query = m_query.get_description();

        // Check current state and create subscription if needed. Throw in an error is found:
        TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), "__ResultSet");
        size_t name_idx = table->get_descriptor()->get_column_index("name");
        size_t query_idx = table->get_descriptor()->get_column_index("query");
        size_t status_idx = table->get_descriptor()->get_column_index("status");
        size_t error_idx = table->get_descriptor()->get_column_index("error_message");
        auto results = Results(realm, *table).filter(table->column<StringData>(col_idx).equal(key));
        if (results.size() > 0) {
            // Subscription with that ID already exist. Verify that we are not trying to reuse an
            // existing name for a different query.
            REALM_ASSERT(results.size() == 1);
            auto obj = results.get(0);
            if (obj.get_string(query_idx) != serialized_query)) {
                std::stringstream ss;
                ss << "Subscription cannot be created as another subscription already exists with the same name. ";
                ss << "Name: " << key << ". ";
                ss << "Existing query: " << obj.get_string(query_idx) << ". ";
                ss << "New query: " << serialized_query << ".";
                m_realm->cancel_transaction();
                throw new std::logic_error(ss.str());
            };

            partial_sync_status_code = (int8_t) obj->get_int(status_idx);
            partial_sync_error_message = obj->get_string(error_idx);

        } else {
            auto props = AnyDict{
           O     {"name", key},
                {"query", serialized_query}
            };
            Object::create<util::Any>(context, m_realm, m_realm->schema().find("__ResultSet"), std::move(props), false);
            partial_sync_status_code = 0;
            partial_sync_error_message = "";
        }

        m_realm->commit_transaction();
    }

    // m_query->sync_view_if_needed(); // No longer needed, begin_transaction will do this
    m_tv = m_query->find_all();
#if REALM_HAVE_COMPOSABLE_DISTINCT
    m_tv.apply_descriptor_ordering(m_descriptor_ordering);
#else
    if (m_descriptor_ordering.sort)
        m_tv.sort(m_descriptor_ordering.sort);

    if (m_descriptor_ordering.distinct)
        m_tv.distinct(m_descriptor_ordering.distinct);
#endif
    m_last_seen_version = m_tv.sync_if_needed();

    calculate_changes();

    // Set partial sync properties
    // TODO SHould we do this inside calculate_changes()?
    m_changes->new_partial_sync_status_code = partial_sync_status_code;
    m_changes->new_partial_sync_error_message = partial_sync_error_message;
}

void ResultsNotifier::do_prepare_handover(SharedGroup& sg)
{
    if (!m_tv.is_attached()) {
        // if the table version didn't change we can just reuse the same handover
        // object and bump its version to the current SG version
        if (m_tv_handover)
            m_tv_handover->version = sg.get_version_of_current_transaction();

        // add_changes() needs to be called even if there are no changes to
        // clear the skip flag on the callbacks
        add_changes(std::move(m_changes));
        return;
    }

    REALM_ASSERT(m_tv.is_in_sync());

    m_tv_handover = sg.export_for_handover(m_tv, MutableSourcePayload::Move);

    add_changes(std::move(m_changes));
    REALM_ASSERT(m_changes.empty());

    // detach the TableView as we won't need it again and keeping it around
    // makes advance_read() much more expensive
    m_tv = {};
}

void ResultsNotifier::deliver(SharedGroup& sg)
{
    auto lock = lock_target();

    // Target realm being null here indicates that we were unregistered while we
    // were in the process of advancing the Realm version and preparing for
    // delivery, i.e. the results was destroyed from the "wrong" thread
    if (!get_realm()) {
        return;
    }

    REALM_ASSERT(!m_query_handover);
    if (m_tv_to_deliver) {
        Results::Internal::set_table_view(*m_target_results,
                                          std::move(*sg.import_from_handover(std::move(m_tv_to_deliver))));
    }
    REALM_ASSERT(!m_tv_to_deliver);
}

bool ResultsNotifier::prepare_to_deliver()
{
    auto lock = lock_target();
    if (!get_realm())
        return false;
    m_tv_to_deliver = std::move(m_tv_handover);
    return true;
}

void ResultsNotifier::do_attach_to(SharedGroup& sg)
{
    REALM_ASSERT(m_query_handover);
    m_query = sg.import_from_handover(std::move(m_query_handover));
    m_descriptor_ordering = DescriptorOrdering::create_from_and_consume_patch(m_ordering_handover, *m_query->get_table());
}

void ResultsNotifier::do_detach_from(SharedGroup& sg)
{
    REALM_ASSERT(m_query);
    REALM_ASSERT(!m_tv.is_attached());

    DescriptorOrdering::generate_patch(m_descriptor_ordering, m_ordering_handover);
    m_query_handover = sg.export_for_handover(*m_query, MutableSourcePayload::Move);
    m_query = nullptr;
}
