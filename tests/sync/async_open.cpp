////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Realm Inc.
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

#include "feature_checks.hpp"

#include "sync_test_utils.hpp"

#include "keypath_helpers.hpp"
#include "object.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"
#include "results.hpp"
#include "schema.hpp"
#include "shared_realm.hpp"

#include "impl/object_accessor_impl.hpp"
#include "sync/partial_sync.hpp"
#include "sync/subscription_state.hpp"
#include "sync/sync_config.hpp"
#include "sync/sync_manager.hpp"
#include "sync/sync_session.hpp"

#include "util/event_loop.hpp"
#include "util/test_file.hpp"
#include "util/test_utils.hpp"

#include <realm/parser/parser.hpp>
#include <realm/parser/query_builder.hpp>
#include <realm/util/optional.hpp>

using namespace realm;
using namespace std::string_literals;

namespace {

struct TypeA {
    size_t number;
    size_t second_number;
    std::string string;
    size_t link_id = realm::npos;
};

struct TypeB {
    size_t number;
    std::string string;
    std::string second_string;
};

struct TypeC {
    size_t number;
};

Schema partial_sync_schema() {
    return Schema{
        {"object_a", {
            {"number", PropertyType::Int},
            {"second_number", PropertyType::Int},
            {"string", PropertyType::String},
            {"link", PropertyType::Object|PropertyType::Nullable, "link_target"},
        }},
        {"object_b", {
            {"number", PropertyType::Int},
            {"string", PropertyType::String},
            {"second_string", PropertyType::String},
        }},
        {"link_target", {
            {"id", PropertyType::Int}
        },{
            {"parents", PropertyType::LinkingObjects|PropertyType::Array, "object_a", "link"},
        }}
    };
}
}

TEST_CASE("Get Realm using Async Open", "[asyncOpen]") {

    if (!EventLoop::has_implementation())
        return;

    SyncManager::shared().configure(tmp_dir(), SyncManager::MetadataMode::NoEncryption);

    SyncServer server;
    SyncTestFile config(server, "test");
    config.schema = partial_sync_schema();
    SyncTestFile partial_config(server, "test", true);
    partial_config.schema = partial_sync_schema();
    // Add some objects for test purposes.

    SECTION("works in the most basic case") {
        AsyncOpenTask task = Realm::get_synchronized_realm(config);
        std::atomic<bool> done(false);
        task.start([&](std::shared_ptr<Realm> realm, std::exception_ptr error) {
            REQUIRE(!error);
            REQUIRE(realm);
            done = true;
        });
        EventLoop::main().run_until([&] { return done.load(); });
    }

}
