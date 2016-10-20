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

namespace {

uint8_t value_of_hex_digit(char hex_digit)
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

bool character_is_unreserved(char character)
{
    bool is_capital_letter = (character >= 'A' && character <= 'Z');
    bool is_lowercase_letter = (character >= 'a' && character <= 'z');
    bool is_number = (character >= '0' && character <= '9');
    bool is_allowed_symbol = (character == '-' || character == '_');
    return is_capital_letter || is_lowercase_letter || is_number || is_allowed_symbol;
}

char decoded_char_for(const std::string& percent_encoding, size_t index)
{
    if (index+2 >= percent_encoding.length()) {
        throw std::invalid_argument("Malformed string: not enough characters after '%' before end of string.");
    }
    REALM_ASSERT(percent_encoding[index] == '%');
    return (16*value_of_hex_digit(percent_encoding[index + 1])) + value_of_hex_digit(percent_encoding[index + 2]);
}

} // (anonymous namespace)

namespace util {

void remove_nonempty_dir(const std::string& path)
{
#ifdef _WIN32
    static_assert(false, "Not implemented for Win32 yet");
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
            try {
                File::remove(file_path_by_appending_component(path, file_name));
            }
            catch (File::NotFound) {
            }
        } else if (file_type == DT_DIR) {
            // Directory, recurse
            remove_nonempty_dir(file_path_by_appending_component(path, file_name, FilePathType::Directory));
        }
    }
    closedir(dir_listing);
    // Delete the directory itself
    try {
        util::remove_dir(path);    
    }
    catch (File::NotFound) {
    }
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

std::string file_path_by_appending_component(const std::string& path, const std::string& component, FilePathType path_type)
{
    // FIXME: Does this have to be changed to accomodate Windows platforms?
    std::string buffer;
    buffer.reserve(2 + path.length() + component.length());
    buffer.append(path);
    std::string terminal = "";
    if (path_type == FilePathType::Directory && component[component.length() - 1] != '/') {
        terminal = "/";
    }
    char path_last = path[path.length() - 1];
    char component_first = component[0];
    if (path_last == '/' && component_first == '/') {
        buffer.append(component.substr(1));
        buffer.append(terminal);
    } else if (path_last == '/' || component_first == '/') {
        buffer.append(component);
        buffer.append(terminal);
    } else {
        buffer.append("/");
        buffer.append(component);
        buffer.append(terminal);
    }
    return buffer;
}

std::string file_path_by_appending_extension(const std::string& path, const std::string& extension)
{
    std::string buffer;
    buffer.reserve(1 + path.length() + extension.length());
    buffer.append(path);
    char path_last = path[path.length() - 1];
    char extension_first = extension[0];
    if (path_last == '.' && extension_first == '.') {
        buffer.append(extension.substr(1));
    } else if (path_last == '.' || extension_first == '.') {
        buffer.append(extension);
    } else {
        buffer.append(".");
        buffer.append(extension);
    }
    return buffer;
}

} // util

constexpr const char SyncFileManager::c_sync_directory[];
constexpr const char SyncFileManager::c_utility_directory[];
constexpr const char SyncFileManager::c_metadata_directory[];
constexpr const char SyncFileManager::c_metadata_realm[];

std::string SyncFileManager::get_utility_directory()
{
    auto util_path = file_path_by_appending_component(get_base_sync_directory(),
                                                      c_utility_directory,
                                                      util::FilePathType::Directory);
    util::try_make_dir(util_path);
    return util_path;
}

std::string SyncFileManager::get_base_sync_directory()
{
    auto sync_path = file_path_by_appending_component(m_base_path,
                                                      c_sync_directory,
                                                      util::FilePathType::Directory);
    util::try_make_dir(sync_path);
    return sync_path;
}

std::string SyncFileManager::user_directory(const std::string& user_identity)
{
    REALM_ASSERT(user_identity.length() > 0);
    auto user_path = file_path_by_appending_component(get_base_sync_directory(),
                                                      user_identity,
                                                      util::FilePathType::Directory);
    util::try_make_dir(user_path);
    return user_path;
}

void SyncFileManager::remove_user_directory(const std::string& user_identity)
{
    REALM_ASSERT(user_identity.length() > 0);
    auto user_path = file_path_by_appending_component(get_base_sync_directory(),
                                                      user_identity,
                                                      util::FilePathType
                                                      ::Directory);
    try {
        util::remove_nonempty_dir(user_path);
    }
    catch(File::AccessError) {
    }
}

bool SyncFileManager::remove_realm(const std::string& user_identity, const std::string& raw_realm_path)
{
    REALM_ASSERT(user_identity.length() > 0);
    REALM_ASSERT(raw_realm_path.length() > 0);
    auto escaped = util::make_percent_encoded_string(raw_realm_path);
    auto realm_path = util::file_path_by_appending_component(user_directory(user_identity), std::move(escaped));
    bool success = true;
    // Remove the base Realm file (e.g. "example.realm").
    try {
        File::remove(realm_path);
    }
    catch(File::NotFound) {
    }
    catch(File::AccessError) {
        success = false;
    }
    // Remove the lock file (e.g. "example.realm.lock").
    auto lock_path = util::file_path_by_appending_extension(realm_path, "lock");
    try {
        File::remove(lock_path);
    }
    catch(File::NotFound) {
    }
    catch(File::AccessError) {
        success = false;
    }
    // Remove the management directory (e.g. "example.realm.management").
    auto management_path = util::file_path_by_appending_extension(realm_path, "management");
    try {
        util::remove_nonempty_dir(management_path);
    }
    catch(File::NotFound) {
    }
    catch(File::AccessError) {
        success = false;
    }
    return success;
}

std::string SyncFileManager::path(const std::string& user_identity, const std::string& raw_realm_path)
{
    REALM_ASSERT(user_identity.length() > 0);
    REALM_ASSERT(raw_realm_path.length() > 0);
    auto escaped = util::make_percent_encoded_string(raw_realm_path);
    auto realm_path = util::file_path_by_appending_component(user_directory(user_identity), std::move(escaped));
    return realm_path;
}

std::string SyncFileManager::metadata_path()
{
    auto dir_path = file_path_by_appending_component(get_utility_directory(),
                                                     c_metadata_directory,
                                                     util::FilePathType::Directory);
    util::try_make_dir(dir_path);
    return util::file_path_by_appending_component(std::move(dir_path), c_metadata_realm);
}

bool SyncFileManager::remove_metadata_realm()
{
    auto dir_path = file_path_by_appending_component(get_utility_directory(),
                                                     c_metadata_directory,
                                                     util::FilePathType::Directory);
    try {
        util::remove_nonempty_dir(dir_path);
        return true;
    }
    catch(File::AccessError) {
        return false;
    }
}

} // realm
