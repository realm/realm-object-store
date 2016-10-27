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

#ifndef REALM_SYNC_TEST_UTILS_HPP
#define REALM_SYNC_TEST_UTILS_HPP

#include "catch.hpp"

#include "sync/impl/sync_file.hpp"
#include "sync/impl/sync_metadata.hpp"

namespace realm {

/// Open a Realm at a given path, creating its files.
bool create_dummy_realm(std::string path);
void reset_test_directory(const std::string& base_path);
bool results_contains_user(SyncUserMetadataResults& results, const std::string& identity);
std::string tmp_dir();
std::vector<char> make_test_encryption_key(const char start = 0);

} // realm

#define REQUIRE_DIR_EXISTS(macro_path) \
{ \
    DIR *dir_listing = opendir((macro_path).c_str()); \
    CHECK(dir_listing); \
    if (dir_listing) closedir(dir_listing); \
}

#define REQUIRE_DIR_DOES_NOT_EXIST(macro_path) \
{ \
    DIR *dir_listing = opendir((macro_path).c_str()); \
    CHECK(dir_listing == NULL); \
    if (dir_listing) closedir(dir_listing); \
}

#endif // REALM_SYNC_TEST_UTILS_HPP
