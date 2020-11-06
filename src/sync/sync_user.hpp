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

#ifndef REALM_OS_SYNC_USER_HPP
#define REALM_OS_SYNC_USER_HPP

#include "object_schema.hpp"
#include "object.hpp"
#include "shared_realm.hpp"

#include "impl/object_accessor_impl.hpp"

#include "util/atomic_shared_ptr.hpp"
#include "util/bson/bson.hpp"
#include "util/functional.h"

#include <realm/util/any.hpp>
#include <realm/util/optional.hpp>
#include <realm/table.hpp>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "sdk.hpp"

namespace realm {
namespace app {
    struct AppError;
} // namespace app

class CppContext;
class SyncSession;
class SyncManager;
// A superclass that bindings can inherit from in order to store information
// upon a `SyncUser` object.
class SyncUserContext {
public:
    virtual ~SyncUserContext() = default;
};

using SyncUserContextFactory = std::function<std::shared_ptr<SyncUserContext>()>;

// A struct that decodes a given JWT.
struct RealmJWT {
    // The token being decoded from.
    std::string token;

    // When the token expires.
    long expires_at;
    // When the token was issued.
    long issued_at;
    // Custom user data embedded in the encoded token.
    util::Optional<bson::BsonDocument> user_data;

    RealmJWT(const std::string& token);

    bool operator==(const RealmJWT& other) const
    {
        return token == other.token;
    }
};

struct SyncUserProfile : public sdk::EmbeddedObject<SyncUserProfile> {
    // The full name of the user.
    REALM(util::Optional<std::string>) name;
    // The email address of the user.
    REALM(util::Optional<std::string>) email;
    // A URL to the user's profile picture.
    REALM(util::Optional<std::string>) picture_url;
    // The first name of the user.
    REALM(util::Optional<std::string>) first_name;
    // The last name of the user.
    REALM(util::Optional<std::string>) last_name;
    // The gender of the user.
    REALM(util::Optional<std::string>) gender;
    // The birthdate of the user.
    REALM(util::Optional<std::string>) birthday;
    // The minimum age of the user.
    REALM(util::Optional<std::string>) min_age;
    // The maximum age of the user.
    REALM(util::Optional<std::string>) max_age;

    REALM_EXPORT(name, email, picture_url, first_name, last_name, gender, birthday, min_age, max_age);
};

// A struct that represents an identity that a `User` is linked to
struct SyncUserIdentity : public sdk::EmbeddedObject<SyncUserIdentity> {
    // the id of the identity
    REALM(std::string) id;
    // the associated provider type of the identity
    REALM(std::string) provider_type;

    REALM_EXPORT(id, provider_type);

    bool operator==(const SyncUserIdentity& other) const
    {
        return id == other.id && provider_type == other.provider_type;
    }

    bool operator!=(const SyncUserIdentity& other) const
    {
        return id != other.id || provider_type != other.provider_type;
    }
};

// MARK: User
// A `SyncUser` represents a single user account. Each user manages the sessions that
// are associated with it.
class SyncUser : public sdk::Object<SyncUser>, public std::enable_shared_from_this<SyncUser> {
friend class SyncSession;
public:
    enum class State : std::size_t {
        LoggedOut,
        LoggedIn,
        Removed,
    };

    REALM_PRIMARY_KEY(std::string) id;
    REALM(util::Optional<std::string>) refresh_token;
    REALM(util::Optional<std::string>) access_token;
    REALM(std::string) device_id;
    REALM(SyncUserProfile) profile;
    REALM(std::vector<SyncUserIdentity>) identities;
    REALM(State) state;
    REALM(std::string) app_id;

    REALM_EXPORT(id, refresh_token, access_token, device_id, profile, identities, state, app_id);

    void set_logged_in_state(const std::string& refresh_token,
                             const std::string& access_token,
                             const std::string& device_id);

    // Return a list of all sessions belonging to this user.
    std::vector<std::shared_ptr<SyncSession>> all_sessions();

    // Return a session for a given on disk path.
    // In most cases, bindings shouldn't expose this to consumers, since the on-disk
    // path for a synced Realm is an opaque implementation detail. This API is retained
    // for testing purposes, and for bindings for consumers that are servers or tools.
    std::shared_ptr<SyncSession> session_for_on_disk_path(const std::string& path);

    // Log the user out and mark it as such. This will also close its associated Sessions.
    void log_out();

    /// Returns true id the users access_token and refresh_token are set.
    bool is_logged_in() const;

    const std::string& local_identity() const noexcept
    {
        return m_local_identity;
    }

    RealmJWT refresh_jwt() const
    {
        return RealmJWT(refresh_token);
    }

    bool has_device_id() const;

    // Custom user data embedded in the access token.
    util::Optional<bson::BsonDocument> custom_data() const;

    // Register a session to this user.
    // A registered session will be bound at the earliest opportunity: either
    // immediately, or upon the user becoming Active.
    // Note that this is called by the SyncManager, and should not be directly called.
    void register_session(std::shared_ptr<SyncSession>);

    /// Refreshes the custom data for this user
    void refresh_custom_data(std::function<void(util::Optional<app::AppError>)> completion_block);

    std::shared_ptr<SyncManager> sync_manager() const;
private:
    // A locally assigned UUID intended to provide a level of indirection for various features.
    std::string m_local_identity;

    // Sessions are owned by the SyncManager, but the user keeps a map of weak references
    // to them.
    std::unordered_map<std::string, std::weak_ptr<SyncSession>> m_sessions;

    // Waiting sessions are those that should be asked to connect once this user is logged in.
    std::unordered_map<std::string, std::weak_ptr<SyncSession>> m_waiting_sessions;
};

//REALM_MANAGED3(SyncUser, id, refresh_token, access_token, device_id, profile, identities, state);

bool operator==(const SyncUser& lhs, const SyncUser& rhs) noexcept;

}

namespace std {
template<> struct hash<realm::SyncUserIdentity> {
    size_t operator()(realm::SyncUserIdentity const&) const;
};
}

#endif // REALM_OS_SYNC_USER_HPP
