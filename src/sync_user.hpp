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

#include <string>
#include <memory>
#include <unordered_map>

namespace realm {

struct SyncSession;

// A `SyncUser` represents a single user account. Each user manages the sessions that
// are associated with it.
class SyncUser {
public:	
	enum class State {
		LoggedOut, Active, Error;
	}

	// Don't use this directly; use the `SyncManager` APIs. Public for use with `make_shared`.
	SyncUser(std::string refresh_token, bool is_admin=false);

	// Return a list of all users.
	static std::vector<std::shared_ptr<SyncUser>> all_users();

	// Return a list of all sessions belonging to this user.
	std::vector<std::shared_ptr<SyncSession>> all_sessions();

	// Return a session for a given file path.
	std::shared_ptr<SyncSession> session_for_path(const std::string path&);

	// Log the user out and mark it as invalid. This will also close its associated Sessions.
	void log_out();

	// Whether the user was configured as an 'admin user' (directly uses its user token
	// to open Realms).
	bool is_admin() const
	{
		return m_is_admin;
	}

	std::string identity() const
	{
		return m_identity;
	}

	State state() const
	{
		return m_state;
	}

	// The auth server URL. Bindings should set this appropriately when they retrieve
	// instances of `SyncUser`s.
	// FIXME: once the new token system is implemented, this can be removed completely.
	std::string server_url;

private:
	State m_state;
	
	// Whether the user is an 'admin' user. Admin users use the admin tokens they were
	// configured with to directly open sessions, and do not make network requests.
	bool m_is_admin;
	// The user's refresh token.
	std::string m_refresh_token;
	// Set by the server. The unique ID of the user account on the Realm Object Server.
	std::string m_identity;

    // Sessions are owned by the SyncManager, but the user keeps a map of weak references
    // to them.
    std::unordered_map<std::string, std::weak_ptr<SyncSession>> m_sessions;
}

}

#endif // REALM_OS_SYNC_USER_HPP
