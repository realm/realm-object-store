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

#include "sync/sync_session.hpp"

#include "sync/impl/sync_client.hpp"
#include "sync/sync_manager.hpp"
#include "sync/sync_user.hpp"

#include <realm/sync/protocol.hpp>

using namespace realm;
using namespace realm::_impl;
using namespace realm::_impl::sync_session_states;

/// A state which a `SyncSession` can currently be within. State classes handle various actions
/// and state transitions.
///
/// STATES:
///
/// WAITING_FOR_ACCESS_TOKEN: upon entering this state, the binding is informed
/// that the session wants an access token. The session is now waiting for the
/// binding to provide the token.
/// From: INACTIVE
/// To:
///    * ACTIVE: when the binding successfully refreshes the token
///    * INACTIVE: if asked to log out, or if asked to close and the stop policy
///                is Immediate.
///    * DYING: if asked to close and the stop policy is AfterChangesUploaded
///    * ERROR: if a fatal error occurs
///
/// ACTIVE: the session is connected to the Realm Object Server and is actively
/// transferring data.
/// From: WAITING_FOR_ACCESS_TOKEN, DYING
/// To:
///    * WAITING_FOR_ACCESS_TOKEN: if the session is informed (through the error
///                                handler) that the token expired
///    * INACTIVE: if asked to log out, or if asked to close and the stop policy
///                is Immediate.
///    * DYING: if asked to close and the stop policy is AfterChangesUploaded
///    * ERROR: if a fatal error occurs
///
/// DYING: the session is performing clean-up work in preparation to be destroyed.
/// From: ACTIVE
/// To:
///    * INACTIVE: when the clean-up work completes, if the session wasn't
///                revived, or if explicitly asked to log out before the
///                clean-up work begins
///    * ACTIVE: if the session is revived
///    * ERROR: if a fatal error occurs
///
/// INACTIVE: the user owning this session has logged out, the `sync::Session`
/// owned by this session is destroyed, and the session is quiescent.
/// From: initial, WAITING_FOR_ACCESS_TOKEN, ACTIVE, DYING
/// To:
///    * WAITING_FOR_ACCESS_TOKEN: if the session is revived
///    * ERROR: if a fatal error occurs
///
/// ERROR: a non-recoverable error has occurred, and this session is semantically
/// invalid. The binding must create a new session with a different configuration.
/// From: WAITING_FOR_ACCESS_TOKEN, ACTIVE, DYING, INACTIVE
/// To:
///    * (none, this is a terminal state)
///
struct SyncSession::State {
    virtual ~State() { }

    virtual void enter_state(std::unique_lock<std::mutex>&, SyncSession&) const { }

    virtual void refresh_access_token(std::unique_lock<std::mutex>&,
                                      SyncSession&, const std::string&,
                                      const util::Optional<std::string>&) const { }

    virtual void bind_with_admin_token(std::unique_lock<std::mutex>&,
                                       SyncSession&, const std::string&, const std::string&) const { }

    /// Returns true iff the lock is still locked when the method returns.
    virtual bool access_token_expired(std::unique_lock<std::mutex>&, SyncSession&) const { return true; }

    virtual void nonsync_transact_notify(std::unique_lock<std::mutex>&, SyncSession&, sync::Session::version_type) const { }

    virtual bool revive_if_needed(std::unique_lock<std::mutex>&, SyncSession&) const { return false; }

    virtual void log_out(std::unique_lock<std::mutex>&, SyncSession&) const { }

    virtual void close_if_connecting(std::unique_lock<std::mutex>&, SyncSession&) const { }

    virtual void close(std::unique_lock<std::mutex>&, SyncSession&) const { }

    static const State& waiting_for_access_token;
    static const State& active;
    static const State& dying;
    static const State& inactive;
    static const State& error;
};

struct sync_session_states::WaitingForAccessToken : public SyncSession::State {
    void enter_state(std::unique_lock<std::mutex>&, SyncSession& session) const override
    {
        session.m_deferred_close = false;
    }

    void refresh_access_token(std::unique_lock<std::mutex>& lock, SyncSession& session,
                              const std::string& access_token,
                              const util::Optional<std::string>& server_url) const override
    {
        // Since the sync session was previously unbound, it's safe to do this from the
        // calling thread.
        if (!session.m_server_url) {
            session.m_server_url = std::move(server_url);
        }
        session.m_session->bind(*session.m_server_url, std::move(access_token));
        if (session.m_deferred_commit_notification) {
            session.m_session->nonsync_transact_notify(*session.m_deferred_commit_notification);
            session.m_deferred_commit_notification = util::none;
        }
        session.advance_state(lock, active);
        if (session.m_deferred_close) {
            session.m_deferred_close = false;
            session.m_state->close(lock, session);
        }
    }

    void log_out(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        session.advance_state(lock, inactive);
    }

    void nonsync_transact_notify(std::unique_lock<std::mutex>&,
                                 SyncSession& session,
                                 sync::Session::version_type version) const override
    {
        // Notify at first available opportunity.
        session.m_deferred_commit_notification = version;
    }

    void close_if_connecting(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        // Ignore the sync configuration's stop policy as we're not yet connected.
        session.advance_state(lock, inactive);
    }

    void close(std::unique_lock<std::mutex>&, SyncSession& session) const override
    {
        session.m_deferred_close = true;
    }
};

struct sync_session_states::Active : public SyncSession::State {
    void refresh_access_token(std::unique_lock<std::mutex>&, SyncSession& session,
                              const std::string& access_token,
                              const util::Optional<std::string>&) const override
    {
        session.m_session->refresh(std::move(access_token));
    }

    bool access_token_expired(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        session.advance_state(lock, waiting_for_access_token);
        std::shared_ptr<SyncSession> session_ptr = session.shared_from_this();
        lock.unlock();
        session.m_config.bind_session_handler(session_ptr->m_realm_path, session_ptr->m_config, std::move(session_ptr));
        return false;
    }

    void log_out(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        session.advance_state(lock, inactive);
    }

    void nonsync_transact_notify(std::unique_lock<std::mutex>&, SyncSession& session,
                                 sync::Session::version_type version) const override
    {
        // Fully ready sync session, notify immediately.
        session.m_session->nonsync_transact_notify(version);
    }

    void close(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        switch (session.m_config.stop_policy) {
            case SyncSessionStopPolicy::Immediately:
                session.advance_state(lock, inactive);
                break;
            case SyncSessionStopPolicy::LiveIndefinitely:
                // Don't do anything; session lives forever.
                break;
            case SyncSessionStopPolicy::AfterChangesUploaded:
                // Wait for all pending changes to upload.
                session.advance_state(lock, dying);
                break;
        }
    }
};

struct sync_session_states::Dying : public SyncSession::State {
    void enter_state(std::unique_lock<std::mutex>&, SyncSession& session) const override
    {
        size_t current_death_count = ++session.m_death_count;
        session.m_session->async_wait_for_upload_completion([session=&session, current_death_count](std::error_code error_code) {
            if (error_code == util::error::operation_aborted) {
                // Session was killed beneath us. Don't do anything.
                return;
            }
            // FIXME: It's possible for the session to be destroyed while the callback is running,
            // which is something we should address in the future. It is only possible when 
            // SyncManager::reset_for_testing() is called.
            // c.f. https://github.com/realm/realm-object-store/issues/269
            std::unique_lock<std::mutex> lock(session->m_state_mutex);
            if (session->m_state == &State::dying && session->m_death_count == current_death_count) {
                session->advance_state(lock, inactive);
            }
        });
    }

    bool revive_if_needed(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        // Revive.
        session.advance_state(lock, active);
        return false;
    }

    void log_out(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        session.advance_state(lock, inactive);
    }
};

struct sync_session_states::Inactive : public SyncSession::State {
    void enter_state(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        session.m_session = nullptr;
        session.m_server_url = util::none;
        session.unregister(lock);
    }

    void bind_with_admin_token(std::unique_lock<std::mutex>& lock, SyncSession& session,
                               const std::string& admin_token,
                               const std::string& server_url) const override
    {
        session.create_sync_session();
        session.advance_state(lock, waiting_for_access_token);
        session.m_state->refresh_access_token(lock, session, admin_token, server_url);
    }

    bool revive_if_needed(std::unique_lock<std::mutex>& lock, SyncSession& session) const override
    {
        // Revive.
        session.create_sync_session();
        session.advance_state(lock, waiting_for_access_token);
        return true;
    }
};

struct sync_session_states::Error : public SyncSession::State {
    void enter_state(std::unique_lock<std::mutex>&, SyncSession& session) const override
    {
        session.m_session = nullptr;
        session.m_config = { nullptr, "", SyncSessionStopPolicy::Immediately, nullptr };
    }

    // Everything else is a no-op when in the error state.
};


const SyncSession::State& SyncSession::State::waiting_for_access_token = WaitingForAccessToken();
const SyncSession::State& SyncSession::State::active = Active();
const SyncSession::State& SyncSession::State::dying = Dying();
const SyncSession::State& SyncSession::State::inactive = Inactive();
const SyncSession::State& SyncSession::State::error = Error();


SyncSession::SyncSession(std::shared_ptr<SyncClient> client, std::string realm_path, SyncConfig config)
: m_state(&State::inactive)
, m_config(std::move(config))
, m_realm_path(std::move(realm_path))
, m_client(std::move(client)) { }

void SyncSession::create_sync_session()
{
    REALM_ASSERT(!m_session);
    m_session = std::make_unique<sync::Session>(m_client->client, m_realm_path);

    // Set up the wrapped handler
    std::weak_ptr<SyncSession> weak_self = shared_from_this();
    auto wrapped_handler = [this, weak_self](int error_code, std::string message) {
        auto self = weak_self.lock();
        if (!self) {
            // An error was delivered after the session it relates to was destroyed. There's nothing useful
            // we can do with it.
            return;
        }

        using ProtocolError = realm::sync::ProtocolError;

        SyncSessionError error_type = SyncSessionError::Debug;
        // Precondition: error_code is a valid realm::sync::Error raw value.
        ProtocolError strong_code = static_cast<ProtocolError>(error_code);

        switch (strong_code) {
            // Client errors; all ignored (for now)
            case ProtocolError::invalid_error:
            case ProtocolError::connection_closed:
            case ProtocolError::other_error:
            case ProtocolError::unknown_message:
            case ProtocolError::bad_syntax:
            case ProtocolError::limits_exceeded:
            case ProtocolError::wrong_protocol_version:
            case ProtocolError::bad_session_ident:
            case ProtocolError::reuse_of_session_ident:
            case ProtocolError::bound_in_other_session:
            case ProtocolError::bad_message_order:
                return;
            // Session errors
            case ProtocolError::disabled_session:
            case ProtocolError::session_closed:
            case ProtocolError::other_session_error:
                // The binding doesn't need to be aware of these because they are strictly informational, and do not
                // represent actual errors.
                return;
            case ProtocolError::token_expired: {
                std::unique_lock<std::mutex> lock(m_state_mutex);
                // This isn't an error from the binding's point of view. If we're connected we'll
                // simply ask the binding to log in again.
                m_state->access_token_expired(lock, *this);
                return;
            }
            case ProtocolError::bad_authentication: {
                std::shared_ptr<SyncUser> user_to_invalidate;
                {
                    std::unique_lock<std::mutex> lock(m_state_mutex);
                    error_type = SyncSessionError::UserFatal;
                    user_to_invalidate = user();
                    advance_state(lock, State::error);
                }
                if (user_to_invalidate)
                    user_to_invalidate->invalidate();
                break;
            }
            case ProtocolError::illegal_realm_path:
            case ProtocolError::no_such_realm:
            case ProtocolError::bad_server_file_ident:
            case ProtocolError::diverging_histories:
            case ProtocolError::bad_changeset: {
                std::unique_lock<std::mutex> lock(m_state_mutex);
                error_type = SyncSessionError::SessionFatal;
                advance_state(lock, State::error);
                break;
            }
            case ProtocolError::permission_denied:
                error_type = SyncSessionError::AccessDenied;
                break;
            case ProtocolError::bad_client_file_ident:
            case ProtocolError::bad_server_version:
            case ProtocolError::bad_client_version:
                error_type = SyncSessionError::Debug;
                break;
        }
        if (m_error_handler) {
            m_error_handler(std::move(self), error_code, message, error_type);
        }
    };
    m_session->set_error_handler(std::move(wrapped_handler));

    // Set up the wrapped sync transact callback
    auto wrapped_callback = [this, weak_self](VersionID old_version, VersionID new_version) {
        if (auto self = weak_self.lock()) {
            if (m_sync_transact_callback) {
                m_sync_transact_callback(old_version, new_version);
            }
        }
    };
    m_session->set_sync_transact_callback(std::move(wrapped_callback));
}

void SyncSession::set_sync_transact_callback(std::function<sync::Session::SyncTransactCallback> callback)
{
    m_sync_transact_callback = std::move(callback);
}

void SyncSession::set_error_handler(std::function<SyncSessionErrorHandler> handler)
{
    m_error_handler = std::move(handler);
}

void SyncSession::advance_state(std::unique_lock<std::mutex>& lock, const State& state)
{
    REALM_ASSERT(lock.owns_lock());
    REALM_ASSERT(&state != m_state);
    m_state = &state;
    m_state->enter_state(lock, *this);
}

void SyncSession::nonsync_transact_notify(sync::Session::version_type version)
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    m_state->nonsync_transact_notify(lock, *this, version);
}

void SyncSession::revive_if_needed(std::shared_ptr<SyncSession> session)
{
    REALM_ASSERT(session);
    util::Optional<std::function<SyncBindSessionHandler>&> handler;
    {
        std::unique_lock<std::mutex> lock(session->m_state_mutex);
        if (session->m_state->revive_if_needed(lock, *session)) {
            handler = session->m_config.bind_session_handler;
        }
    }
    if (handler) {
        handler.value()(session->m_realm_path, session->m_config, session);
    }
}

void SyncSession::log_out()
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    m_state->log_out(lock, *this);
}

void SyncSession::close()
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    m_state->close(lock, *this);
}

void SyncSession::close_if_connecting()
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    m_state->close_if_connecting(lock, *this);
}

void SyncSession::unregister(std::unique_lock<std::mutex>& lock)
{
    REALM_ASSERT(lock.owns_lock());
    REALM_ASSERT(m_state == &State::inactive); // Must stop an active session before unregistering.

    lock.unlock();
    SyncManager::shared().unregister_session(m_realm_path);
}

bool SyncSession::can_wait_for_network_completion() const
{
    return m_state == &State::active || m_state == &State::dying;
}

bool SyncSession::wait_for_upload_completion(std::function<void(std::error_code)> callback)
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    // FIXME: instead of dropping the callback if we haven't yet `bind()`ed,
    // save it and register it when the session `bind()`s.
    if (can_wait_for_network_completion()) {
        REALM_ASSERT(m_session);
        m_session->async_wait_for_upload_completion(std::move(callback));
        return true;
    }
    return false;
}

bool SyncSession::wait_for_download_completion(std::function<void(std::error_code)> callback)
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    // FIXME: instead of dropping the callback if we haven't yet `bind()`ed,
    // save it and register it when the session `bind()`s.
    if (can_wait_for_network_completion()) {
        REALM_ASSERT(m_session);
        m_session->async_wait_for_download_completion(std::move(callback));
        return true;
    }
    return false;
}

bool SyncSession::wait_for_upload_completion_blocking()
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    if (can_wait_for_network_completion()) {
        REALM_ASSERT(m_session);
        m_session->wait_for_upload_complete_or_client_stopped();
        return true;
    }
    return false;
}

void SyncSession::refresh_access_token(std::string access_token, util::Optional<std::string> server_url)
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    if (!m_server_url && !server_url) {
        // The first time this method is called, the server URL must be provided.
        return;
    }
    m_state->refresh_access_token(lock, *this, access_token, server_url);
}

void SyncSession::bind_with_admin_token(std::string admin_token, std::string server_url)
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    m_state->bind_with_admin_token(lock, *this, admin_token, server_url);
}

SyncSession::PublicState SyncSession::state() const
{
    std::unique_lock<std::mutex> lock(m_state_mutex);
    if (m_state == &State::waiting_for_access_token) {
        return PublicState::WaitingForAccessToken;
    } else if (m_state == &State::active) {
        return PublicState::Active;
    } else if (m_state == &State::dying) {
        return PublicState::Dying;
    } else if (m_state == &State::inactive) {
        return PublicState::Inactive;
    } else if (m_state == &State::error) {
        return PublicState::Error;
    }
    REALM_UNREACHABLE();
}
