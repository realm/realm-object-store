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

#ifndef REALM_OS_SYNC_CONFIG_HPP
#define REALM_OS_SYNC_CONFIG_HPP

#include <realm/sync/protocol.hpp>

#include <functional>
#include <memory>
#include <string>
#include <system_error>

namespace realm {

class SyncUser;
class SyncSession;

enum class SyncSessionStopPolicy;

struct SyncConfig;
using SyncBindSessionHandler = void(const std::string&,          // path on disk of the Realm file.
                                    const SyncConfig&,           // the sync configuration object.
                                    std::shared_ptr<SyncSession> // the session which should be bound.
                                    );

struct SyncError;
using SyncSessionErrorHandler = void(std::shared_ptr<SyncSession>, SyncError);

struct SyncError {
    using ProtocolError = realm::sync::ProtocolError;

    std::error_code error_code;
    std::string message;
    bool is_fatal;

    /// The error applies to the entire sync client. Errors are either client, session, or user errors.
    bool is_client_error() const
    {
        return !is_user_error() && !is_session_error();
    }

    /// The error is specific to a session. Errors are either client, session, or user errors.
    bool is_session_error() const
    {
        ProtocolError value = enum_value();
        return !is_user_error() && realm::sync::is_session_level_error(value);
    }

    /// The error is specific to the user owning the session attached to this error. Errors are either
    /// client session, or user errors.
    bool is_user_error() const
    {
        return enum_value() == ProtocolError::bad_authentication;
    }

    bool is_access_denied_error() const
    {
        return enum_value() == ProtocolError::permission_denied;
    }

    ProtocolError enum_value() const
    {
        return static_cast<ProtocolError>(error_code.value());
    }
};

struct SyncConfig {
    std::shared_ptr<SyncUser> user;
    std::string realm_url;
    SyncSessionStopPolicy stop_policy;
    std::function<SyncBindSessionHandler> bind_session_handler;
    std::function<SyncSessionErrorHandler> error_handler;
};

} // namespace realm

#endif // REALM_OS_SYNC_CONFIG_HPP
