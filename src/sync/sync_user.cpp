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

#include "sync/sync_user.hpp"

#include "sync/app_credentials.hpp"
#include "sync/generic_network_transport.hpp"
#include "sync/sync_manager.hpp"
#include "sync/sync_session.hpp"

#include <realm/util/base64.hpp>

namespace realm {

static std::string base64_decode(const std::string &in) {
    std::string out;
    out.resize(util::base64_decoded_size(in.size()));
    util::base64_decode(in, &out[0], out.size());
    return out;
}

static std::vector<std::string> split_token(const std::string& jwt) {
    constexpr static char delimiter = '.';

    std::vector<std::string> parts;
    size_t pos = 0, start_from = 0;

    while ((pos = jwt.find(delimiter, start_from)) != std::string::npos) {
        parts.push_back(jwt.substr(start_from, pos - start_from));
        start_from = pos + 1;
    }

    parts.push_back(jwt.substr(start_from));

    if (parts.size() != 3) {
        throw app::AppError(make_error_code(app::JSONErrorCode::bad_token), "jwt missing parts");
    }

    return parts;
}

RealmJWT::RealmJWT(const std::string& token)
: token(token) {
    auto parts = split_token(this->token);

    auto json_str = base64_decode(parts[1]);
    auto json = static_cast<bson::BsonDocument>(bson::parse(json_str));

    this->expires_at = static_cast<int64_t>(json["exp"]);
    this->issued_at = static_cast<int64_t>(json["iat"]);

    if (json.find("user_data") != json.end()) {
        this->user_data = static_cast<bson::BsonDocument>(json["user_data"]);
    }
}

// MARK: User

void SyncUser::set_logged_in_state(const std::string& refresh_token,
                                   const std::string& access_token,
                                   const std::string& device_id)
{
    REALM_ASSERT(!id.empty());

    std::vector<std::shared_ptr<SyncSession>> sessions_to_revive;
    switch (state) {
        case State::LoggedOut: {
            sessions_to_revive.reserve(m_waiting_sessions.size());
            state = State::LoggedIn;
            for (auto& pair : m_waiting_sessions) {
                if (auto ptr = pair.second.lock()) {
                    m_sessions[pair.first] = ptr;
                    sessions_to_revive.emplace_back(std::move(ptr));
                }
            }
            m_waiting_sessions.clear();
            break;
        }
        default:
            break;
    }

    this->refresh_token = refresh_token;
    this->access_token = access_token;
    this->device_id = device_id;
    this->state = State::LoggedIn;

    // (Re)activate all pending sessions.
    // Note that we do this after releasing the lock, since the session may
    // need to access protected User state in the process of binding itself.
    for (auto& session : sessions_to_revive) {
        session->revive_if_needed();
    }
}

std::vector<std::shared_ptr<SyncSession>> SyncUser::all_sessions()
{
    std::vector<std::shared_ptr<SyncSession>> sessions;
    if (state == State::Removed) {
        return sessions;
    }
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        if (auto ptr_to_session = it->second.lock()) {
            sessions.emplace_back(std::move(ptr_to_session));
            it++;
            continue;
        }
        // This session is bad, destroy it.
        it = m_sessions.erase(it);
    }
    return sessions;
}

std::shared_ptr<SyncSession> SyncUser::session_for_on_disk_path(const std::string& path)
{
    if (state == State::Removed) {
        return nullptr;
    }
    auto it = m_sessions.find(path);
    if (it == m_sessions.end()) {
        return nullptr;
    }
    auto locked = it->second.lock();
    if (!locked) {
        // Remove the session from the map, because it has fatally errored out or the entry is invalid.
        m_sessions.erase(it);
    }
    return locked;
}

void SyncUser::log_out()
{
    {
        if (state == State::LoggedOut) {
            return;
        }

        state = State::LoggedOut;
        access_token = util::none;
        refresh_token = util::none;
//        m_access_token.token = "";
//        m_refresh_token.token = "";

//        m_sync_manager->perform_metadata_update([=](const auto& manager) {
//            auto metadata = manager.get_or_make_user_metadata(m_identity, m_provider_type);
//            metadata->set_state(State::LoggedOut);
//            metadata->set_access_token("");
//            metadata->set_refresh_token("");
//        });
        // Move all active sessions into the waiting sessions pool. If the user is
        // logged back in, they will automatically be reactivated.
        for (auto& pair : m_sessions) {
            if (auto ptr = pair.second.lock()) {
                ptr->log_out();
                m_waiting_sessions[pair.first] = ptr;
            }
        }
        m_sessions.clear();
    }

//    m_sync_manager->log_out_user(m_identity);

    // Mark the user as 'dead' in the persisted metadata Realm
    // if they were an anonymous user
    // TODO: remove metadata for user
//    if (this->m_provider_type == app::IdentityProviderAnonymous) {
//        invalidate();
//        m_sync_manager->perform_metadata_update([=](const auto& manager) {
//            auto metadata = manager.get_or_make_user_metadata(m_identity, m_provider_type, false);
//            if (metadata)
//                metadata->remove();
//        });
//    }
}

bool SyncUser::is_logged_in() const
{
    return true;
//    return !m_access_token.token.empty() && !m_refresh_token.token.empty() && m_state == State::LoggedIn;
}

bool SyncUser::has_device_id() const
{
    auto d_id = device_id;
    return !d_id.empty() && d_id != "000000000000000000000000";
}

util::Optional<bson::BsonDocument> SyncUser::custom_data() const
{
    return RealmJWT(access_token).user_data;
}

void SyncUser::register_session(std::shared_ptr<SyncSession> session)
{
    const std::string& path = session->path();
    switch (state) {
        case State::LoggedIn:
            // Immediately ask the session to come online.
            m_sessions[path] = session;
            session->revive_if_needed();
            break;
        case State::LoggedOut:
            m_waiting_sessions[path] = session;
            break;
        default:
            break;
    }
}

void SyncUser::refresh_custom_data(std::function<void(util::Optional<app::AppError>)> completion_block)
{
    if (auto app = sync_manager()->app().lock()) {
        app->refresh_custom_data(*this, completion_block);
    } else {
        completion_block(app::AppError(app::make_client_error_code(app::ClientErrorCode::app_deallocated),
                                       "App has been deallocated"));
    }
}

std::shared_ptr<SyncManager> SyncUser::sync_manager() const
{
    return app::App::get_cached_app(app_id)->sync_manager();
}

bool operator==(const SyncUser& lhs, const SyncUser& rhs) noexcept
{
   return lhs.id == rhs.id;
}
} // namespace realm

namespace std {
size_t hash<realm::SyncUserIdentity>::operator()(const realm::SyncUserIdentity& k) const
{
    return ((hash<std::string>()(k.id) ^
             (hash<std::string>()(k.provider_type) << 1)) >> 1);
}
}
