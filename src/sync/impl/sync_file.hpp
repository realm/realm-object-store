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

#ifndef REALM_OS_SYNC_FILE_HPP
#define REALM_OS_SYNC_FILE_HPP

#include <string>

#include "sync/app.hpp"
#include "sync/sync_user.hpp"

#include <realm/util/optional.hpp>

namespace realm {

namespace util {

enum class FilePathType {
    File, Directory
};

// FIXME: Does it make sense to use realm::StringData arguments for these functions instead of std::string?

/// Given a string, turn it into a percent-encoded string.
std::string make_percent_encoded_string(const std::string& raw_string);

/// Given a percent-encoded string, turn it into the original (non-encoded) string.
std::string make_raw_string(const std::string& percent_encoded_string);

/// Given a file path and a path component, return a new path created by appending the component to the path.
std::string file_path_by_appending_component(const std::string& path,
                                             const std::string& component,
                                             FilePathType path_type=FilePathType::File);

/// Given a file path and an extension, append the extension to the path.
std::string file_path_by_appending_extension(const std::string& path, const std::string& extension);

/// Create a timestamped `mktemp`-compatible template string using the current local time.
std::string create_timestamped_template(const std::string& prefix, int wildcard_count=8);

/// Reserve a unique file name based on a base directory path and a `mktemp`-compatible template string.
/// Returns the path of the file.
std::string reserve_unique_file_name(const std::string& path, const std::string& template_string);

} // util

// This class manages how Synced Realms are stored on the filesystem.
class SyncFileManager {
public:
    SyncFileManager(std::string base_path, std::string app_id)
        : m_base_path(std::move(base_path))
        , m_app_id(std::move(app_id))
        {
        }

    /// Return the user directory for a given user, creating it if it does not already exist.
    std::string user_directory(const std::string& local_identity) const;

    /// Remove the user directory for a given user.
    void remove_user_directory(const std::string& local_identity) const;       // throws

    /// Rename a user directory. Returns true if a directory at `old_name` existed
    /// and was successfully renamed to `new_name`. Returns false if no directory
    /// exists at `old_name`.
    bool try_rename_user_directory(const std::string& old_name, const std::string& new_name) const;

    /// A non throw version of File::exists(),  returning false if any exceptions are thrown when attempting to access this file.
    static bool try_file_exists(const std::string& path) noexcept;

    /// Return the path for a given Realm, creating the user directory if it does not already exist.
    std::string realm_file_path(const std::string& local_user_identity, const std::string& realm_file_name) const;

    /// Remove the Realm at a given path for a given user. Returns `true` if the remove operation fully succeeds.
    bool remove_realm(const std::string& local_user_identity, const std::string& realm_file_name) const;

    /// Remove the Realm whose primary Realm file is located at `absolute_path`. Returns `true` if the remove
    /// operation fully succeeds.
    bool remove_realm(const std::string& absolute_path) const;

    /// Copy the Realm file at the location `old_path` to the location of `new_path`.
    bool copy_realm_file(const std::string& old_path, const std::string& new_path) const;

    /// Return the path for the metadata Realm files.
    std::string metadata_path() const;

    /// Remove the metadata Realm.
    bool remove_metadata_realm() const;

    const std::string& base_path() const
    {
        return m_base_path;
    }

    std::string recovery_directory_path(util::Optional<std::string> const& directory=none) const
    {
        return get_special_directory(directory.value_or(c_recovery_directory));
    }

private:
    const std::string m_base_path;
    const std::string m_app_id;

    static constexpr const char c_sync_directory[] = "mongodb-realm";
    static constexpr const char c_utility_directory[] = "server-utility";
    static constexpr const char c_recovery_directory[] = "recovered-realms";
    static constexpr const char c_metadata_directory[] = "metadata";
    static constexpr const char c_metadata_realm[] = "sync_metadata.realm";
    static constexpr const char c_realm_file_suffix[] = ".realm";
    static constexpr const char c_realm_file_test_suffix[] = ".rtest"; // Must have same length as c_realm_file_suffix.
    static constexpr const char c_legacy_sync_directory[] = "realm-object-server";

    std::string get_special_directory(std::string directory_name) const;

    std::string get_utility_directory() const
    {
        return get_special_directory(c_utility_directory);
    }

    std::string get_base_sync_directory() const;

    // Construct the absolute path to the users directory
    std::string get_user_directory_path(const std::string& local_user_identity) const;
    std::string legacy_realm_file_path(const std::string& local_user_identity, const std::string& realm_file_name) const;
    std::string fallback_hashed_realm_file_path(const std::string& preferred_path) const;
};

} // realm

#endif // REALM_OS_SYNC_FILE_HPP
