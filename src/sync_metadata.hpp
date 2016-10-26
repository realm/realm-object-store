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

#ifndef REALM_OS_SYNC_METADATA_HPP
#define REALM_OS_SYNC_METADATA_HPP

#include <string>

#include <realm/row.hpp>
#include <realm/table.hpp>
#include <realm/util/optional.hpp>

#include "results.hpp"
#include "shared_realm.hpp"

namespace realm {
template<typename T> class BasicRowExpr;
using RowExpr = BasicRowExpr<Table>;
class SyncMetadataManager;
class SyncUserMetadata;
class SyncFileActionMetadata;

template<class T>
class SyncMetadataResults {
public:
    size_t size() const
    {
        return m_results.size();
    }

    T get(size_t idx) const
    {
        RowExpr row = m_results.get(idx);
        return T(m_realm, row);
    }

    SyncMetadataResults(Results results, SharedRealm realm, typename T::Columns columns)
    : m_results(std::move(results))
    , m_realm(std::move(realm))
    , m_columns(std::move(columns)) { }
private:
    typename T::Columns m_columns;
    SharedRealm m_realm;
    // FIXME: remove 'mutable' once `realm::Results` is properly annotated for const
    mutable Results m_results;
};
using SyncUserMetadataResults = SyncMetadataResults<SyncUserMetadata>;
using SyncFileActionMetadataResults = SyncMetadataResults<SyncFileActionMetadata>;

class SyncUserMetadata {
friend class SyncFileActionMetadata;
public:
    struct Columns {
        size_t idx_identity;
        size_t idx_marked_for_removal;
        size_t idx_user_token;
        size_t idx_auth_server_url;
    };

    static Columns columns()
    {
        return m_columns;
    }
    static void discover_columns(Group& read_group);

    std::string identity() const;

    util::Optional<std::string> server_url() const
    {
        return get_optional_string_field(m_columns.idx_auth_server_url);
    }

    util::Optional<std::string> user_token() const
    {
        return get_optional_string_field(m_columns.idx_user_token);
    }

    void set_state(util::Optional<std::string> server_url, util::Optional<std::string> user_token);

    // Remove the user from the metadata database.
    void remove();
    // Mark the user as "ready for removal". Since Realm files cannot be safely deleted after being opened, the actual
    // deletion of a user must be deferred until the next time the host application is launched.
    void mark_for_removal();

    bool is_valid() const
    {
        return !m_invalid;
    }

    static ObjectSchema object_schema();

    // Construct a new user.
    //
    // If `make_if_absent` is false and the user is absent or removed, a 'removed' user will be returned for which all
    // set operations are no-ops and all get operations cause an assert to fail.
    //
    // If `make_if_absent` is true and the user was previously marked for deletion, it will be unmarked.
    SyncUserMetadata(SyncMetadataManager& manager, std::string identity, bool make_if_absent=true);

    SyncUserMetadata(SharedRealm realm, RowExpr row);

private:
    bool m_invalid = false;

    util::Optional<std::string> get_optional_string_field(size_t col_idx) const;

    static Columns m_columns;
    SharedRealm m_realm;
    Row m_row;
};

class SyncFileActionMetadata {
public:
    enum class Action {
        DeleteRealmFiles = 0,
        MoveRealmFiles = 1,
    };

    struct Columns {
        size_t idx_action;
        size_t idx_user;
        size_t idx_current_path;
        size_t idx_future_path;
    };

    static Columns columns()
    {
        return m_columns;
    }
    static void discover_columns(Group& read_group);
    static ObjectSchema object_schema();

    /// Remove metadata from the Realm. Do not further access metadata after calling this method.
    void remove();

    std::string current_path() const;
    util::Optional<std::string> future_path() const;
    Action action() const;
    bool completed() const;
    SyncUserMetadata user();

    // Construct a new file action metadata object.
    SyncFileActionMetadata(SyncMetadataManager& manager,
                           Action action,
                           const std::string& current_path,
                           util::Optional<SyncUserMetadata> user=none,
                           util::Optional<std::string> future_path=none);

    SyncFileActionMetadata(SharedRealm realm, RowExpr row);

private:
    static Columns m_columns;

    SharedRealm m_realm;
    Row m_row;
};

class SyncMetadataManager {
friend class SyncUserMetadata;
public:
    // Return a Results object containing all users not marked for removal.
    SyncUserMetadataResults all_unmarked_users() const
    {
        return get_users(false);
    }

    // Return a Results object containing all users marked for removal. It is the binding's responsibility to call
    // `remove()` on each user to actually remove it from the database. (This is so that already-open Realm files can be
    // safely cleaned up the next time the host is launched.)
    SyncUserMetadataResults all_users_marked_for_removal() const
    {
        return get_users(true);
    }

    SyncFileActionMetadataResults all_file_actions() const;

    util::Optional<SyncFileActionMetadata> get_existing_file_action(const std::string& current_path);

    Realm::Config get_configuration() const;

    /// Construct the metadata manager.
    ///
    /// If the platform supports it, setting `should_encrypt` to `true` and not specifying an encryption key will make
    /// the object store handle generating and persisting an encryption key for the metadata database. Otherwise, an
    /// exception will be thrown.
    SyncMetadataManager(std::string path,
                        bool should_encrypt,
                        util::Optional<std::vector<char>> encryption_key=none);

private:
    SyncUserMetadataResults get_users(bool marked) const;

    Realm::Config m_metadata_config;

    mutable std::mutex m_metadata_lock;
};

}

#endif // REALM_OS_SYNC_METADATA_HPP
