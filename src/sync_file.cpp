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

#include "sync_file.hpp"

#include <realm/util/file.hpp>

using File = realm::util::File;

namespace realm {

static uint8_t value_of_hex_digit(char hex_digit)
{
    if (hex_digit >= '0' && hex_digit <= '9') {
        return hex_digit - '0';
    } else if (hex_digit >= 'A' && hex_digit <= 'F') {
        return 10 + hex_digit - 'A';
    } else if (hex_digit >= 'a' && hex_digit <= 'f') {
        return 10 + hex_digit - 'a';
    } else {
        throw std::invalid_argument("Cannot get the value of a character that isn't a hex digit.");
    }
}

static bool character_is_unreserved(char character)
{
    bool is_capital_letter = (character >= 'A' && character <= 'Z');
    bool is_lowercase_letter = (character >= 'a' && character <= 'z');
    bool is_number = (character >= '0' && character <= '9');
    bool is_allowed_symbol = (character == '-' || character == '_');
    return is_capital_letter || is_lowercase_letter || is_number || is_allowed_symbol;
}

static char decoded_char_for(const std::string& percent_encoding, size_t index)
{
    if (index+2 >= percent_encoding.length()) {
        throw std::invalid_argument("Malformed string: not enough characters after '%' before end of string.");
    }
    REALM_ASSERT(percent_encoding[index] == '%');
    return (16*value_of_hex_digit(percent_encoding[index + 1])) + value_of_hex_digit(percent_encoding[index + 2]);
}

void remove_nonempty_dir(const std::string& path)
{
#ifdef _WIN32
    // Not implemented yet.
    REALM_ASSERT(false);
#else
    // Open the directory and list all the files.
    DIR *dir_listing = opendir(path.c_str());
    if (!dir_listing) {
        return;
    }
    while (struct dirent *file = readdir(dir_listing)) {
        auto file_type = file->d_type;
        std::string file_name = file->d_name;
        if (file_name == "." || file_name == "..") {
            continue;
        }
        if (file_type == DT_REG || file_type == DT_FIFO) {
            // Regular file (this really shouldn't ever throw, so if it does don't bother catching the exception).
            File::remove(file_path_by_appending_component(path, file_name));
        } else if (file_type == DT_DIR) {
            // Directory, recurse
            remove_nonempty_dir(file_path_by_appending_component(path, file_name, true));
        }
    }
    closedir(dir_listing);
    // Delete the directory itself
    util::remove_dir(path);
#endif
}

std::string make_percent_encoded_string(const std::string& raw_string)
{
    std::string buffer;
    buffer.reserve(raw_string.length());
    for (char character : raw_string) {
        if (character_is_unreserved(character)) {
            buffer.push_back(character);
        } else {
            char b[4] = {0, 0, 0, 0};
            snprintf(b, 4, "%%%2X", character);
            buffer.append(b);
        }
    }
    return buffer;
}

std::string make_raw_string(const std::string& percent_encoded_string)
{
    std::string buffer;
    size_t input_len = percent_encoded_string.length();
    buffer.reserve(input_len);
    size_t idx = 0;
    while (idx < input_len) {
        char current = percent_encoded_string[idx];
        if (current == '%') {
            // Decode. +3.
            buffer.push_back(decoded_char_for(percent_encoded_string, idx));
            idx += 3;
        } else {
            // No need to decode. +1.
            if (!character_is_unreserved(current)) {
                throw std::invalid_argument("Input string is invalid: contains reserved characters.");
            }
            buffer.push_back(current);
            idx++;
        }
    }
    return buffer;
}

std::string file_path_by_appending_component(const std::string& path, const std::string& component, bool is_directory)
{
    // FIXME: Does this have to be changed to accomodate Windows platforms?
    std::string terminal = "";
    if (is_directory && component[component.length() - 1] != '/') {
        terminal = "/";
    }
    char path_last = path[path.length() - 1];
    char component_first = component[0];
    if (path_last == '/' && component_first == '/') {
        return path + component.substr(1) + terminal;
    } else if (path_last == '/' || component_first == '/') {
        return path + component + terminal;
    } else {
        return path + "/" + component + terminal;
    }
}

std::string file_path_by_appending_extension(const std::string& path, const std::string& extension)
{
    char path_last = path[path.length() - 1];
    char extension_first = extension[0];
    if (path_last == '.' && extension_first == '.') {
        return path + extension.substr(1);
    } else if (path_last == '.' || extension_first == '.') {
        return path + extension;
    } else {
        return path + "." + extension;
    }
}

std::string SyncFileManager::get_utility_directory()
{
    auto util_path = file_path_by_appending_component(get_base_sync_directory(), c_utility_directory, true);
    util::try_make_dir(util_path);
    return util_path;
}

std::string SyncFileManager::get_base_sync_directory()
{
    auto sync_path = file_path_by_appending_component(m_base_path, c_sync_directory, true);
    util::try_make_dir(sync_path);
    return sync_path;
}

std::string SyncFileManager::user_directory(std::string user_identity)
{
    if (user_identity.length() == 0) {
        throw std::invalid_argument("user_identity cannot be an empty string");
    }
    auto user_path = file_path_by_appending_component(get_base_sync_directory(), std::move(user_identity), true);
    util::try_make_dir(user_path);
    return user_path;
}

void SyncFileManager::remove_user_directory(std::string user_identity)
{
    if (user_identity.length() == 0) {
        throw std::invalid_argument("user_identity cannot be an empty string");
    }
    auto user_path = file_path_by_appending_component(get_base_sync_directory(), std::move(user_identity), true);
    try {
        remove_nonempty_dir(user_path);
    } catch(File::AccessError) {
    }
}

bool SyncFileManager::remove_realm(std::string user_identity, std::string raw_realm_path)
{
    if (user_identity.length() == 0) {
        throw std::invalid_argument("user_identity cannot be an empty string");
    } else if (raw_realm_path.length() == 0) {
        throw std::invalid_argument("path cannot be an empty string");
    }
    auto escaped = make_percent_encoded_string(raw_realm_path);
    auto realm_path = file_path_by_appending_component(user_directory(std::move(user_identity)), std::move(escaped));
    bool success = true;
    // Remove the base Realm file (e.g. "example.realm").
    try {
        File::remove(realm_path);
    } catch(File::NotFound) {
    } catch(File::AccessError) {
        success = false;
    }
    // Remove the lock file (e.g. "example.realm.lock").
    auto lock_path = file_path_by_appending_extension(realm_path, "lock");
    try {
        File::remove(lock_path);
    } catch(File::NotFound) {
    } catch(File::AccessError) {
        success = false;
    }
    // Remove the management directory (e.g. "example.realm.management").
    auto management_path = file_path_by_appending_extension(realm_path, "management");
    try {
        remove_nonempty_dir(management_path);
    } catch(File::NotFound) {
    } catch(File::AccessError) {
        success = false;
    }
    return success;
}

std::string SyncFileManager::path(std::string user_identity, std::string raw_realm_path)
{
    if (user_identity.length() == 0) {
        throw std::invalid_argument("user_identity cannot be an empty string");
    } else if (raw_realm_path.length() == 0) {
        throw std::invalid_argument("path cannot be an empty string");
    }
    auto escaped = make_percent_encoded_string(raw_realm_path);
    auto realm_path = file_path_by_appending_component(user_directory(std::move(user_identity)), std::move(escaped));
    return realm_path;
}

std::string SyncFileManager::metadata_path()
{
    auto dir_path = file_path_by_appending_component(get_utility_directory(), c_metadata_directory, true);
    util::try_make_dir(dir_path);
    return file_path_by_appending_component(std::move(dir_path), c_metadata_realm);
}

bool SyncFileManager::remove_metadata_realm()
{
    auto dir_path = file_path_by_appending_component(get_utility_directory(), c_metadata_directory, true);
    try {
        remove_nonempty_dir(dir_path);
        return true;
    } catch(File::AccessError) {
        return false;
    }
}

}
