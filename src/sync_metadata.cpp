////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
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

#include "sync_metadata.hpp"

#include "object_schema.hpp"
#include "object_store.hpp"
#include "property.hpp"
#include "results.hpp"
#include "schema.hpp"
#if REALM_PLATFORM_APPLE
#include "impl/apple/keychain_helper.hpp"
#endif

#include <realm/descriptor.hpp>
#include <realm/table.hpp>

namespace realm {

static const char * const c_sync_userMetadata = "UserMetadata";
static const char * const c_sync_fileActionMetadata = "FileActionMetadata";
static const char * const c_sync_marked_for_removal = "marked_for_removal";
static const char * const c_sync_identity = "identity";
static const char * const c_sync_auth_server_url = "auth_server_url";
static const char * const c_sync_user_token = "user_token";
static const char * const c_sync_action = "action";
static const char * const c_sync_current_path = "current_path";
static const char * const c_sync_future_path = "future_path";
static const char * const c_sync_user = "user";

static Property nullable_string_property(std::string name)
{
    Property p = { std::move(name), PropertyType::String };
    p.is_nullable = true;
    return p;
}

static Property object_property(std::string name, std::string object_type)
{
    Property p = { std::move(name), PropertyType::Object };
    p.object_type = std::move(object_type);
    p.is_nullable = true;
    return p;
}

SyncMetadataManager::SyncMetadataManager(std::string path,
                                         bool should_encrypt,
                                         util::Optional<std::vector<char>> encryption_key)
{
    std::lock_guard<std::mutex> lock(m_metadata_lock);

    Property primary_key = { c_sync_identity, PropertyType::String };
    primary_key.is_indexed = true;
    primary_key.is_primary = true;

    Realm::Config config;
    config.path = std::move(path);
    Schema schema = {
        SyncFileActionMetadata::object_schema(),
        SyncUserMetadata::object_schema(),
    };
    config.schema = std::move(schema);
    config.schema_mode = SchemaMode::Additive;
#if REALM_PLATFORM_APPLE
    if (should_encrypt && !encryption_key) {
        encryption_key = keychain::metadata_realm_encryption_key();
    }
#endif
    if (should_encrypt) {
        if (!encryption_key) {
            throw std::invalid_argument("Metadata Realm encryption was specified, but no encryption key was provided.");
        }
        config.encryption_key = std::move(*encryption_key);
    }

    // Open the Realm and get schema information
    SharedRealm realm = Realm::get_shared_realm(config);
    SyncUserMetadata::discover_columns(realm->read_group());
    SyncFileActionMetadata::discover_columns(realm->read_group());

    m_metadata_config = std::move(config);
}

Realm::Config SyncMetadataManager::get_configuration() const
{
    std::lock_guard<std::mutex> lock(m_metadata_lock);
    return m_metadata_config;
}

SyncUserMetadataResults SyncMetadataManager::get_users(bool marked) const
{
    auto columns = SyncUserMetadata::columns();

    // Open the Realm.
    SharedRealm realm = Realm::get_shared_realm(get_configuration());

    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_userMetadata);
    Query query = table->where().equal(columns.idx_marked_for_removal, marked);

    Results results(realm, std::move(query));
    return SyncUserMetadataResults(std::move(results), std::move(realm), std::move(columns));
}

SyncFileActionMetadataResults SyncMetadataManager::all_file_actions() const
{
    auto columns = SyncFileActionMetadata::columns();
    SharedRealm realm = Realm::get_shared_realm(get_configuration());

    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_fileActionMetadata);
    Query query = table->where();

    Results results(realm, std::move(query));
    return SyncFileActionMetadataResults(std::move(results), std::move(realm), std::move(columns));
}

util::Optional<SyncFileActionMetadata> SyncMetadataManager::get_existing_file_action(const std::string& current_path)
{
    auto columns = SyncFileActionMetadata::columns();
    SharedRealm realm = Realm::get_shared_realm(get_configuration());

    TableRef table = ObjectStore::table_for_object_type(realm->read_group(), c_sync_fileActionMetadata);
    size_t row_idx = table->find_first_string(columns.idx_current_path, current_path);
    if (row_idx == not_found) {
        return none;
    }
    return SyncFileActionMetadata(std::move(realm), std::move(table->get(row_idx)));
}

SyncUserMetadata::Columns SyncUserMetadata::m_columns;

SyncUserMetadata::SyncUserMetadata(SharedRealm realm, RowExpr row)
: m_invalid(row.get_bool(columns().idx_marked_for_removal))
, m_realm(std::move(realm))
, m_row(row)
{ }

SyncUserMetadata::SyncUserMetadata(SyncMetadataManager& manager, std::string identity, bool make_if_absent)
: m_realm(Realm::get_shared_realm(manager.get_configuration()))
{
    // Retrieve or create the row for this object.
    TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), c_sync_userMetadata);
    size_t row_idx = table->find_first_string(m_columns.idx_identity, identity);
    if (row_idx == not_found) {
        if (!make_if_absent) {
            m_invalid = true;
            m_realm = nullptr;
            return;
        }
        m_realm->begin_transaction();
        row_idx = table->find_first_string(m_columns.idx_identity, identity);
        if (row_idx == not_found) {
            row_idx = table->add_empty_row();
            table->set_string(m_columns.idx_identity, row_idx, identity);
            m_realm->commit_transaction();
        } else {
            // Someone beat us to adding this user.
            m_realm->cancel_transaction();
        }
    }
    m_row = table->get(row_idx);
    if (make_if_absent) {
        // User existed in the table, but had been marked for deletion. Unmark it.
        m_realm->begin_transaction();
        table->set_bool(m_columns.idx_marked_for_removal, row_idx, false);
        m_realm->commit_transaction();
        m_invalid = false;
    } else {
        m_invalid = m_row.get_bool(m_columns.idx_marked_for_removal);
    }
}

ObjectSchema SyncUserMetadata::object_schema()
{
    Property primary_key = { c_sync_identity, PropertyType::String };
    primary_key.is_indexed = true;
    primary_key.is_primary = true;
    return { c_sync_userMetadata,
        {
            primary_key,
            { c_sync_marked_for_removal, PropertyType::Bool },
            nullable_string_property(c_sync_auth_server_url),
            nullable_string_property(c_sync_user_token),
        }
    };
}

void SyncUserMetadata::discover_columns(Group& read_group)
{
    DescriptorRef descriptor = ObjectStore::table_for_object_type(read_group,
                                                                  c_sync_userMetadata)->get_descriptor();
    m_columns = {
        descriptor->get_column_index(c_sync_identity),
        descriptor->get_column_index(c_sync_marked_for_removal),
        descriptor->get_column_index(c_sync_user_token),
        descriptor->get_column_index(c_sync_auth_server_url),
    };
}

std::string SyncUserMetadata::identity() const
{
    m_realm->verify_thread();
    if (!m_row.is_attached())
        throw std::runtime_error("This user object has been deleted from the metadata database.");
    StringData result = m_row.get_string(m_columns.idx_identity);
    return result;
}

util::Optional<std::string> SyncUserMetadata::get_optional_string_field(size_t col_idx) const
{
    REALM_ASSERT(!m_invalid && m_row.is_attached());
    m_realm->verify_thread();
    StringData result = m_row.get_string(col_idx);
    return result.is_null() ? util::none : util::make_optional(std::string(result));
}

void SyncUserMetadata::set_state(util::Optional<std::string> server_url, util::Optional<std::string> user_token)
{
    if (m_invalid || !m_row.is_attached()) {
        return;
    }
    m_realm->verify_thread();
    m_realm->begin_transaction();
    m_row.set_string(m_columns.idx_user_token, *user_token);
    m_row.set_string(m_columns.idx_auth_server_url, *server_url);
    m_realm->commit_transaction();
}

void SyncUserMetadata::mark_for_removal()
{
    if (m_invalid || !m_row.is_attached()) {
        return;
    }
    m_realm->verify_thread();
    m_realm->begin_transaction();
    m_row.set_bool(m_columns.idx_marked_for_removal, true);
    m_realm->commit_transaction();
}

void SyncUserMetadata::remove()
{
    m_invalid = true;
    if (m_row.is_attached()) {
        m_realm->begin_transaction();
        TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), c_sync_userMetadata);
        table->move_last_over(m_row.get_index());
        m_realm->commit_transaction();
    }
    m_realm = nullptr;
}

SyncFileActionMetadata::Columns SyncFileActionMetadata::m_columns;

SyncFileActionMetadata::SyncFileActionMetadata(SyncMetadataManager& manager,
                                               Action action,
                                               const std::string& current_path,
                                               util::Optional<SyncUserMetadata> user,
                                               util::Optional<std::string> future_path)
: m_realm(Realm::get_shared_realm(manager.get_configuration()))
{
    if (action == Action::MoveRealmFiles && !future_path) {
        throw std::invalid_argument("If action is 'MoveRealmFiles', a future path must be specified.");
    }
    TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), c_sync_fileActionMetadata);
    m_realm->begin_transaction();
    size_t row_idx = table->find_first_string(m_columns.idx_current_path, current_path);
    if (row_idx == not_found) {
        row_idx = table->add_empty_row();
        table->set_string(m_columns.idx_current_path, row_idx, current_path);
        if (user) {
            table->set_link(m_columns.idx_user, row_idx, user->m_row.get_index());
        }
    }
    m_row = table->get(row_idx);
    // Validate the user
    if (user) {
        TableRef user_table = ObjectStore::table_for_object_type(m_realm->read_group(), c_sync_userMetadata);
        auto proposed_user_identity = user->identity();
        auto current_user_identity = user_table->get_string(SyncUserMetadata::columns().idx_identity,
                                                            m_row.get_link(m_columns.idx_user));
        if (proposed_user_identity != current_user_identity) {
            m_realm->cancel_transaction();
            throw std::invalid_argument("Cannot change a file action metadatum's user to a different user.");
        }
    } else {
        table->nullify_link(m_columns.idx_user, row_idx);
    }
    table->set_int(m_columns.idx_action, row_idx, static_cast<size_t>(action));
    table->set_string(m_columns.idx_future_path, row_idx, future_path);
    m_realm->commit_transaction();
}

SyncFileActionMetadata::SyncFileActionMetadata(SharedRealm realm, RowExpr row)
: m_realm(std::move(realm))
, m_row(std::move(row)) { }

void SyncFileActionMetadata::discover_columns(Group& read_group)
{
    DescriptorRef descriptor = ObjectStore::table_for_object_type(read_group,
                                                                  c_sync_fileActionMetadata)->get_descriptor();
    m_columns = {
        descriptor->get_column_index(c_sync_action),
        descriptor->get_column_index(c_sync_user),
        descriptor->get_column_index(c_sync_current_path),
        descriptor->get_column_index(c_sync_future_path),
    };
}

ObjectSchema SyncFileActionMetadata::object_schema()
{
    Property primary_key = { c_sync_current_path, PropertyType::String };
    primary_key.is_indexed = true;
    primary_key.is_primary = true;
    return { c_sync_fileActionMetadata,
        {
            primary_key,
            object_property(c_sync_user, c_sync_userMetadata),
            { c_sync_action, PropertyType::Int },
            nullable_string_property(c_sync_future_path),
        }
    };
}

void SyncFileActionMetadata::remove()
{
    if (m_row.is_attached()) {
        m_realm->begin_transaction();
        TableRef table = ObjectStore::table_for_object_type(m_realm->read_group(), c_sync_fileActionMetadata);
        table->move_last_over(m_row.get_index());
        m_realm->commit_transaction();
    }
    m_realm = nullptr;
}

std::string SyncFileActionMetadata::current_path() const
{
    m_realm->verify_thread();
    if (!m_row.is_attached())
        throw std::runtime_error("This file action has been deleted from the metadata database.");
    return m_row.get_string(m_columns.idx_current_path);
}

util::Optional<std::string> SyncFileActionMetadata::future_path() const
{
    m_realm->verify_thread();
    if (!m_row.is_attached())
        throw std::runtime_error("This file action has been deleted from the metadata database.");
    StringData result = m_row.get_string(m_columns.idx_future_path);
    return result.is_null() ? util::none : util::make_optional(std::string(result));
}

SyncFileActionMetadata::Action SyncFileActionMetadata::action() const
{
    m_realm->verify_thread();
    if (!m_row.is_attached())
        throw std::runtime_error("This file action has been deleted from the metadata database.");
    int64_t raw = m_row.get_int(m_columns.idx_action);
    return static_cast<Action>(raw);
}

SyncUserMetadata SyncFileActionMetadata::user()
{
    m_realm->verify_thread();
    if (!m_row.is_attached())
        throw std::runtime_error("This file action has been deleted from the metadata database.");
    TableRef user_table = ObjectStore::table_for_object_type(m_realm->read_group(), c_sync_userMetadata);
    size_t user_idx = m_row.get_link(m_columns.idx_user);
    return SyncUserMetadata(m_realm, user_table->get(user_idx));
}

}
