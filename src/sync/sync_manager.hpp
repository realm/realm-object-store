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

#ifndef REALM_OS_SYNC_MANAGER_HPP
#define REALM_OS_SYNC_MANAGER_HPP

#include "shared_realm.hpp"

#include "app.hpp"
#include "sync_user.hpp"

#include <realm/sync/client.hpp>
#include <realm/util/logger.hpp>
#include <realm/util/optional.hpp>

#include <memory>
#include <mutex>
#include <unordered_map>

struct TestSyncManager;

namespace realm {

struct SyncConfig;
class SyncSession;
class SyncUser;
class SyncFileManager;
class SyncAppMetadata;

namespace _impl {
struct SyncClient;
}

enum class SyncSessionStopPolicy {
    Immediately,                    // Immediately stop the session as soon as all Realms/Sessions go out of scope.
    LiveIndefinitely,               // Never stop the session.
    AfterChangesUploaded,           // Once all Realms/Sessions go out of scope, wait for uploads to complete and stop.
};

class SyncLoggerFactory {
public:
    virtual std::unique_ptr<util::Logger> make_logger(util::Logger::Level) = 0;
};

struct SyncClientTimeouts {
    // See sync::Client::Config for the meaning of these fields.
    uint64_t connect_timeout = sync::Client::default_connect_timeout;
    uint64_t connection_linger_time = sync::Client::default_connection_linger_time;
    uint64_t ping_keepalive_period = sync::Client::default_ping_keepalive_period;
    uint64_t pong_keepalive_timeout = sync::Client::default_pong_keepalive_timeout;
    uint64_t fast_reconnect_limit = sync::Client::default_fast_reconnect_limit;
};

struct SyncClientConfig {
    using ReconnectMode = sync::Client::ReconnectMode;
    enum class MetadataMode {
        NoEncryption,                   // Enable metadata, but disable encryption.
        Encryption,                     // Enable metadata, and use encryption (automatic if possible).
        NoMetadata,                     // Disable metadata.
    };

    std::string base_file_path;
    MetadataMode metadata_mode = MetadataMode::Encryption;
    util::Optional<std::vector<char>> custom_encryption_key;
    bool reset_metadata_on_error = false;

    SyncLoggerFactory* logger_factory = nullptr;
    // FIXME: Should probably be util::Logger::Level::error
    util::Logger::Level log_level = util::Logger::Level::info;
    ReconnectMode reconnect_mode = ReconnectMode::normal;
    bool multiplex_sessions = false;

    // Optional information about the binding/application that is sent as part of the User-Agent
    // when establishing a connection to the server.
    std::string user_agent_binding_info;
    std::string user_agent_application_info;

    SyncClientTimeouts timeouts;
};

// A facade for a metadata Realm object representing a pending action to be carried out upon a specific file(s).
struct SyncFileAction : sdk::Object<SyncFileAction> {
    enum class Action {
        // The Realm files at the given directory will be deleted.
        DeleteRealm,
        // The Realm file will be copied to a 'recovery' directory, and the original Realm files will be deleted.
        BackUpThenDeleteRealm
    };

    // The absolute path to the Realm file in question.
    REALM_PRIMARY_KEY(std::string) original_name;

    // The meaning of this parameter depends on the `Action` specified.
    // For `BackUpThenDeleteRealm`, it is the absolute path where the backup copy
    // of the Realm file found at `original_name()` will be placed.
    // For all other `Action`s, it is ignored.
    REALM(util::Optional<std::string>) new_name;

    // Get the local UUID of the user associated with this file action metadata.
    REALM(std::string) user_local_uuid;

    // An enum value describing the action to take.
    REALM(Action) action;

    // The full remote URL of the Realm.
    REALM(std::string) url;

    REALM_EXPORT(original_name, new_name, user_local_uuid, action, url);
};

class SyncManager : public std::enable_shared_from_this<SyncManager> {
friend class SyncSession;
friend struct ::TestSyncManager;
public:
    using MetadataMode = SyncClientConfig::MetadataMode;

    // Immediately run file actions for a single Realm at a given original path.
    // Returns whether or not a file action was successfully executed for the specified Realm.
    // Preconditions: all references to the Realm at the given path must have already been invalidated.
    // The metadata and file management subsystems must also have already been configured.
    bool immediately_run_file_actions(const std::string& original_name);

    // Use a single connection for all sync sessions for each host/port rather
    // than one per session.
    // This must be called before any sync sessions are created, cannot be
    // disabled afterwards, and currently is incompatible with automatic failover.
    void enable_session_multiplexing();

    // Sets the log level for the Sync Client.
    // The log level can only be set up until the point the Sync Client is created. This happens when the first Session
    // is created.
    void set_log_level(util::Logger::Level) noexcept;
    void set_logger_factory(SyncLoggerFactory&) noexcept;

    // Create a new logger of the type which will be used by the sync client
    std::unique_ptr<util::Logger> make_logger() const;

    // Sets the application level user agent string.
    // This should have the format specified here: https://github.com/realm/realm-sync/blob/develop/src/realm/sync/client.hpp#L126
    // The user agent can only be set up  until the  point the Sync Client is created. This happens when the first
    // Session is created.
    void set_user_agent(std::string user_agent);

    // Sets client timeout settings.
    // The timeout settings can only be set up until the point the Sync Client is created.
    // This happens when the first Session is created.
    void set_timeouts(SyncClientTimeouts timeouts);

    /// Ask all valid sync sessions to perform whatever tasks might be necessary to
    /// re-establish connectivity with the Realm Object Server. It is presumed that
    /// the caller knows that network connectivity has been restored.
    ///
    /// Refer to `SyncSession::handle_reconnect()` to see what sort of work is done
    /// on a per-session basis.
    void reconnect() const;

    util::Logger::Level log_level() const noexcept;

    std::shared_ptr<SyncSession> get_session(const std::string& path, const SyncConfig& config, bool force_client_resync=false);
    std::shared_ptr<SyncSession> get_existing_session(const std::string& path) const;
    std::shared_ptr<SyncSession> get_existing_active_session(const std::string& path) const;

    // Returns `true` if the SyncManager still contains any existing sessions not yet fully cleaned up.
    // This will return true as long as there is an external reference to a session object, no matter
    // the state of that session.
    bool has_existing_sessions();

    // Get the default path for a Realm for the given user and absolute unresolved URL.
    // If the default path of `<rootDir>/<appId>/<userId>/<realm_file_name>.realm` cannot
    // be created, this function may pass back `<rootDir>/<hashedFileName>.realm`
    std::string path_for_realm(const SyncUser& user, const std::string& realm_file_name) const;

    // Get the default path for a Realm for the given configuration.
    // The default value is `<rootDir>/<appId>/<userId>/<partitionValue>.realm`.
    // If the file cannot be created at this location, for example due to path length restrictions,
    // this function may pass back `<rootDir>/<hashedFileName>.realm`
    std::string path_for_realm(const SyncConfig& config, util::Optional<std::string> custom_file_name = none) const;

    // Get the path of the recovery directory for backed-up or recovered Realms.
    std::string recovery_directory_path(util::Optional<std::string> const& custom_dir_name=none) const;

    // Get the unique identifier of this client.
    std::string client_uuid() const;

    // Reset the singleton state for testing purposes. DO NOT CALL OUTSIDE OF TESTING CODE.
    // Precondition: any synced Realms or `SyncSession`s must be closed or rendered inactive prior to
    // calling this method.
    void reset_for_testing();

    void set_sync_route(std::string sync_route) {
        m_sync_route = std::move(sync_route);
    }

    const std::string sync_route() const {
        return m_sync_route;
    }

    std::weak_ptr<app::App> app() const
    {
        return m_app;
    }

    sdk::Realm open_app_realm() const
    {
        return sdk::Realm(Realm::get_shared_realm(m_metadata_realm_config));
    }

    SyncManager() = default;
    SyncManager(const SyncManager&) = delete;
    SyncManager& operator=(const SyncManager&) = delete;
private:
    friend class app::App;

    void configure(std::shared_ptr<app::App> app,
                   const std::string& sync_route,
                   const SyncClientConfig& config);

    using ReconnectMode = sync::Client::ReconnectMode;

    // Stop tracking the session for the given path if it is inactive.
    // No-op if the session is either still active or in the active sessions list
    // due to someone holding a strong reference to it.
    void unregister_session(const std::string& path);

    _impl::SyncClient& get_sync_client() const;
    std::unique_ptr<_impl::SyncClient> create_sync_client() const;

    std::shared_ptr<SyncSession> get_existing_session_locked(const std::string& path) const;

    mutable std::mutex m_mutex;

    bool run_file_action(const SyncFileAction&);
    void init_metadata(SyncClientConfig config, const std::string& app_id);

    // Protects m_users
    mutable std::mutex m_user_mutex;

    // A vector of all SyncUser objects.
    std::vector<std::shared_ptr<SyncUser>> m_users;
    std::shared_ptr<SyncUser> m_current_user;

    mutable std::unique_ptr<_impl::SyncClient> m_sync_client;

    SyncClientConfig m_config;

    // Protects m_file_manager and m_metadata_manager
    mutable std::mutex m_file_system_mutex;
    std::unique_ptr<SyncFileManager> m_file_manager;

    // Protects m_sessions
    mutable std::mutex m_session_mutex;

    // Map of sessions by path name.
    // Sessions remove themselves from this map by calling `unregister_session` once they're
    // inactive and have performed any necessary cleanup work.
    std::unordered_map<std::string, std::shared_ptr<SyncSession>> m_sessions;

    // Waiting sessions are those that should be asked to connect once a user is logged back in.
    std::unordered_map<std::string, std::weak_ptr<SyncSession>> m_waiting_sessions;

    // Internal method returning `true` if the SyncManager still contains sessions not yet fully closed.
    // Callers of this method should hold the `m_session_mutex` themselves.
    bool do_has_existing_sessions();

    // The unique identifier of this client.
    util::Optional<std::string> m_client_uuid;

    std::string m_sync_route;

    std::weak_ptr<app::App> m_app;
//    SharedRealm m_metadata_realm;
    Realm::Config m_metadata_realm_config;
};

} // namespace realm

#endif // REALM_OS_SYNC_MANAGER_HPP
