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

#include "catch.hpp"

#include "shared_realm.hpp"
#include "sync_file.hpp"
#include <realm/util/file.hpp>

using namespace realm;
using File = realm::util::File;

static const std::string base_path = "/tmp/realm_objectstore_sync_file/";

static void prepare_sync_manager_test(void) {
    // Remove the base directory in /tmp where all test-related file status lives.
    remove_nonempty_dir(base_path);
    const std::string manager_path = base_path + "syncmanager/";
    util::make_dir(base_path);
    util::make_dir(manager_path);
}

/// Open a Realm at a given path, creating its files.
static bool create_dummy_realm(std::string path) {
    Realm::Config config;
    config.path = std::move(path);
    try {
        return Realm::make_shared_realm(config) != nullptr;
    } catch(std::exception) {
        return false;
    }
}

#define REQUIRE_DIR_EXISTS(macro_path) \
{ \
DIR *dir_listing = opendir((macro_path).c_str()); \
REQUIRE(dir_listing); \
if (dir_listing) closedir(dir_listing); \
}

#define REQUIRE_DIR_DOES_NOT_EXIST(macro_path) \
{ \
DIR *dir_listing = opendir((macro_path).c_str()); \
REQUIRE(dir_listing == NULL); \
if (dir_listing) closedir(dir_listing); \
}


TEST_CASE("sync_file: percent-encoding APIs") {
	SECTION("does not encode a string that has no restricted characters") {
		const std::string expected = "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_-";
		auto actual = make_percent_encoded_string(expected);
		REQUIRE(actual == expected);
	}

    SECTION("properly encodes a sample Realm URL") {
    	const std::string expected = "realms%3A%2F%2Fexample%2Ecom%2F%7E%2Ffoo_bar%2Fuser-realm";
    	const std::string raw_string = "realms://example.com/~/foo_bar/user-realm";
    	auto actual = make_percent_encoded_string(raw_string);
    	REQUIRE(actual == expected);
    }

    SECTION("properly decodes a sample Realm URL") {
        const std::string expected = "realms://example.com/~/foo_bar/user-realm";
        const std::string encoded_string = "realms%3A%2F%2Fexample%2Ecom%2F%7E%2Ffoo_bar%2Fuser-realm";
        auto actual = make_raw_string(encoded_string);
        REQUIRE(actual == expected);
    }
}

TEST_CASE("sync_file: URL manipulation APIs") {
    SECTION("properly concatenates a path when the path has a trailing slash") {
        const std::string expected = "/foo/bar";
        const std::string path = "/foo/";
        const std::string component = "bar";
        auto actual = file_path_by_appending_component(path, component);
        REQUIRE(actual == expected);
    }

    SECTION("properly concatenates a path when the component has a leading slash") {
        const std::string expected = "/foo/bar";
        const std::string path = "/foo";
        const std::string component = "/bar";
        auto actual = file_path_by_appending_component(path, component);
        REQUIRE(actual == expected);
    }

    SECTION("properly concatenates a path when both arguments have slashes") {
        const std::string expected = "/foo/bar";
        const std::string path = "/foo/";
        const std::string component = "/bar";
        auto actual = file_path_by_appending_component(path, component);
        REQUIRE(actual == expected);
    }

    SECTION("properly concatenates a directory path when the component doesn't have a trailing slash") {
        const std::string expected = "/foo/bar/";
        const std::string path = "/foo/";
        const std::string component = "/bar";
        auto actual = file_path_by_appending_component(path, component, true);
        REQUIRE(actual == expected);
    }

    SECTION("properly concatenates a directory path when the component has a trailing slash") {
        const std::string expected = "/foo/bar/";
        const std::string path = "/foo/";
        const std::string component = "/bar/";
        auto actual = file_path_by_appending_component(path, component, true);
        REQUIRE(actual == expected);
    }

    SECTION("properly concatenates an extension when the path has a trailing dot") {
        const std::string expected = "/foo.management";
        const std::string path = "/foo.";
        const std::string component = "management";
        auto actual = file_path_by_appending_extension(path, component);
        REQUIRE(actual == expected);
    }

    SECTION("properly concatenates a path when the extension has a leading dot") {
        const std::string expected = "/foo.management";
        const std::string path = "/foo";
        const std::string component = ".management";
        auto actual = file_path_by_appending_extension(path, component);
        REQUIRE(actual == expected);
    }

    SECTION("properly concatenates a path when both arguments have dots") {
        const std::string expected = "/foo.management";
        const std::string path = "/foo.";
        const std::string component = ".management";
        auto actual = file_path_by_appending_extension(path, component);
        REQUIRE(actual == expected);
    }
}

TEST_CASE("sync_file: SyncFileManager APIs") {    
    const std::string identity = "123456789";
    const std::string manager_path = base_path + "syncmanager/";
    prepare_sync_manager_test();
    auto manager = SyncFileManager(manager_path);

    SECTION("user directory APIs") {
        const std::string expected = manager_path + "realm-object-server/123456789/";

        SECTION("getting a user directory") {
            SECTION("that didn't exist before succeeds") {
                auto actual = manager.user_directory(identity);
                REQUIRE(actual == expected);
                REQUIRE_DIR_EXISTS(expected);
            }
            SECTION("that already existed succeeds") {
                auto actual = manager.user_directory(identity);
                REQUIRE(actual == expected);
                REQUIRE_DIR_EXISTS(expected);
            }
        }

        SECTION("deleting a user directory") {
            manager.user_directory(identity);
            REQUIRE_DIR_EXISTS(expected);
            SECTION("that wasn't yet deleted succeeds") {
                manager.remove_user_directory(identity);
                REQUIRE_DIR_DOES_NOT_EXIST(expected);
            }
            SECTION("that was already deleted succeeds") {
                manager.remove_user_directory(identity);
                REQUIRE(opendir(expected.c_str()) == NULL);
                REQUIRE_DIR_DOES_NOT_EXIST(expected);
            }
        }
    }

    SECTION("Realm path APIs") {
        auto relative_path = "realms://r.example.com/~/my/realm/path";

        SECTION("getting a Realm path") {
            const std::string expected = manager_path + "realm-object-server/123456789/realms%3A%2F%2Fr%2Eexample%2Ecom%2F%7E%2Fmy%2Frealm%2Fpath";
            auto actual = manager.path(identity, relative_path);
            REQUIRE(expected == actual);
        }

        SECTION("deleting a Realm for a valid user") {
            manager.path(identity, relative_path);
            // Create the required files
            auto realm_base_path = manager_path + "realm-object-server/123456789/realms%3A%2F%2Fr%2Eexample%2Ecom%2F%7E%2Fmy%2Frealm%2Fpath";
            REQUIRE(create_dummy_realm(realm_base_path));
            REQUIRE(File::exists(realm_base_path));
            REQUIRE(File::exists(realm_base_path + ".lock"));
            REQUIRE_DIR_EXISTS(realm_base_path + ".management");
            // Delete the Realm
            manager.remove_realm(identity, relative_path);
            // Ensure the files don't exist anymore
            REQUIRE(!File::exists(realm_base_path));
            REQUIRE(!File::exists(realm_base_path + ".lock"));
            REQUIRE_DIR_DOES_NOT_EXIST(realm_base_path + ".management");
        }

        SECTION("deleting a Realm for an invalid user") {
            manager.remove_realm("invalid_user", relative_path);
        }

        SECTION("deleting a Realm with an empty user ID string") {
            bool did_throw = false;
            try {
                manager.remove_realm("", relative_path);
            } catch (std::invalid_argument) {
                did_throw = true;
            }
            REQUIRE(did_throw);
        }

        SECTION("deleting a Realm with an empty raw Realm path string") {
            manager.path(identity, relative_path);
            bool did_throw = false;
            try {
                manager.remove_realm(identity, "");
            } catch (std::invalid_argument) {
                did_throw = true;
            }
            REQUIRE(did_throw);
        }
    }

    SECTION("Utility path APIs") {
        auto metadata_dir = manager_path + "realm-object-server/io.realm.object-server-utility/metadata/";

        SECTION("getting the metadata path") {
            auto path = manager.metadata_path();
            REQUIRE(path == (metadata_dir + "sync_metadata.realm"));
        }

        SECTION("removing the metadata Realm") {
            manager.metadata_path();
            REQUIRE_DIR_EXISTS(metadata_dir);
            manager.remove_metadata_realm();
            REQUIRE_DIR_DOES_NOT_EXIST(metadata_dir);
        }
    }
}
