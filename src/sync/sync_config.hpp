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

#include <functional>
#include <memory>
#include <string>
#include <system_error>

namespace realm {

class SyncUser;
class SyncSession;

enum class SyncSessionStopPolicy;

enum class SyncSystemError {
    Debug,        // An informational error, nothing to do. Only for debug purposes.
    AccessDenied, // Permissions error with the session.
    User,         // The user associated with the session is invalid.
    Client,       // A client error.
    Session,      // A session error other than those enumerated above.
};

struct SyncConfig;
using SyncBindSessionHandler = void(const std::string&,          // path on disk of the Realm file.
                                    const SyncConfig&,           // the sync configuration object.
                                    std::shared_ptr<SyncSession> // the session which should be bound.
                                    );

using SyncSessionErrorHandler = void(std::shared_ptr<SyncSession>,
                                     std::error_code error_code,
                                     bool is_fatal,
                                     std::string message,
                                     SyncSystemError);

struct SyncConfig {
    std::shared_ptr<SyncUser> user;
    std::string realm_url;
    SyncSessionStopPolicy stop_policy;
    std::function<SyncBindSessionHandler> bind_session_handler;
    std::function<SyncSessionErrorHandler> error_handler;
};

} // namespace realm

#endif // REALM_OS_SYNC_CONFIG_HPP
