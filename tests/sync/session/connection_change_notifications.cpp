////////////////////////////////////////////////////////////////////////////
//
// Copyright 2018 Realm Inc.
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

#include "catch2/catch.hpp"

#include "sync/session/session_util.hpp"

#include "feature_checks.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"
#include "property.hpp"
#include "schema.hpp"

#include "util/event_loop.hpp"
#include "util/test_utils.hpp"

#include <realm/util/scope_exit.hpp>
#include <realm/util/time.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <unistd.h>

using namespace realm;
using namespace realm::util;

static const std::string dummy_auth_url = "https://realm.example.org";
static const std::string dummy_device_id = "123400000000000000000000";

static const std::string base_path = tmp_dir() + "realm_objectstore_sync_connection_state_changes";

TEST_CASE("sync: Connection state changes", "[sync]") {
    if (!EventLoop::has_implementation())
        return;

    TestSyncManager init_sync_manager({ .base_path = base_path });
    auto app = init_sync_manager.app();
    auto user = app->sync_manager()->get_user("user",
                                               ENCODE_FAKE_JWT("not_a_real_token"),
                                               ENCODE_FAKE_JWT("also_not_a_real_token"),
                                               dummy_auth_url,
                                               dummy_device_id);

    SECTION("register connection change listener") {
        auto session = sync_session(user, "/connection-state-changes-1",
                                    [](auto, auto) {},
                                    SyncSessionStopPolicy::AfterChangesUploaded);

        EventLoop::main().run_until([&] { return sessions_are_active(*session); });
        EventLoop::main().run_until([&] { return sessions_are_connected(*session); });

        std::atomic<bool> listener_called(false);
        session->register_connection_change_callback([&](SyncSession::ConnectionState, SyncSession::ConnectionState) {
            listener_called = true;
        });

        user->log_out();
        EventLoop::main().run_until([&] { return sessions_are_disconnected(*session); });
        REQUIRE(listener_called == true);
    }

    SECTION("unregister connection change listener") {
        auto session = sync_session(user, "/connection-state-changes-2",
                                    [](auto, auto) {},
                                    SyncSessionStopPolicy::AfterChangesUploaded);

        EventLoop::main().run_until([&] { return sessions_are_active(*session); });
        EventLoop::main().run_until([&] { return sessions_are_connected(*session); });

        std::atomic<bool> listener1_called(false);
        std::atomic<bool> listener2_called(false);
        auto token1 = session->register_connection_change_callback([&](SyncSession::ConnectionState , SyncSession::ConnectionState) {
            listener1_called = true;
        });
        session->unregister_connection_change_callback(token1);
        session->register_connection_change_callback([&](SyncSession::ConnectionState , SyncSession::ConnectionState) {
            listener2_called = true;
        });

        user->log_out();
        REQUIRE(sessions_are_disconnected(*session));
        REQUIRE(listener1_called == false);
        REQUIRE(listener2_called == true);
    }
}
