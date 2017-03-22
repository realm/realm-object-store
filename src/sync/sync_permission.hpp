////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
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

#ifndef REALM_OS_SYNC_PERMISSION_HPP
#define REALM_OS_SYNC_PERMISSION_HPP

#include "results.hpp"

#include <string>

namespace realm {

class Permissions;
class SyncUser;
class Object;

// Permission object used to represent a user permission
struct Permission {
    // The Realm path this permission pertains to
    std::string path;

    // Will have one access level
    // Each level included capabilities of the previous
    enum class AccessLevel {
        None,
        Read,
        Write,
        Admin,
    };
    AccessLevel access;

    // Condition is a userId or a KeyValue pair
    // Other conditions may be supported in the future
    struct Condition {
        enum class Type { UserId, KeyValue };
        Type type;

        union {
            std::string user_id;
            std::pair<std::string, std::string> key_value;
        };

        Condition(std::string id) : type(Type::UserId), user_id(id) {}

        Condition(const Condition &c) : type(c.type), user_id(c.user_id) {}
        ~Condition() { if (type == Type::UserId) user_id.std::basic_string<char>::~basic_string<char>(); }
    };
    Condition condition;
};

class PermissionResults {
public:
    // Get number of permissions
    size_t size();

    // Get the permission for the given index
    // Throws OutOfBoundsIndexException if index >= size()
    const Permission get(size_t index);

    // Create an async query from this Results
    // The query will be run on a background thread and delivered to the callback,
    // and then rerun after each commit (if needed) and redelivered if it changed
    NotificationToken async(std::function<void (std::exception_ptr)> target) { return m_results->async(target); }

    // Create a new PermissionResults by further filtering or sorting this PermissionResults
    PermissionResults filter(Query&& q) const;

private:
    PermissionResults(std::unique_ptr<Results> results) : m_results(std::move(results)) {}
    std::unique_ptr<Results> m_results;

    friend Permissions;
};

class Permissions {
public:
    // Get PermissionResults for the provided user - Async
    static void get_permissions(std::shared_ptr<SyncUser> user,
                                std::function<void (std::unique_ptr<PermissionResults>, std::exception_ptr)> callback);

    // Callback used to monitor success or errors when changing permissions
    // exception_ptr is NULL on success
    using PermissionChangeCallback = std::function<void (std::exception_ptr)>;

    // Set permission as the provided user
    static void set_permission(std::shared_ptr<SyncUser> user, Permission permission, PermissionChangeCallback callback);

    // Delete permission as the provided user
    static void delete_permission(std::shared_ptr<SyncUser> user, Permission permission, PermissionChangeCallback callback);

private:
    static SharedRealm management_realm(std::shared_ptr<SyncUser> user);
    static SharedRealm permission_realm(std::shared_ptr<SyncUser> user);
};
}

#endif /* REALM_OS_SYNC_PERMISSION_HPP */
