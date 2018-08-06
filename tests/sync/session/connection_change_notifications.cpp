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

#include "catch.hpp"

#include "sync/session/session_util.hpp"

#include "feature_checks.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"
#include "property.hpp"
#include "schema.hpp"

#include "util/event_loop.hpp"
#include "util/templated_test_case.hpp"
#include "util/time.hpp"

#include <realm/util/scope_exit.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <unistd.h>

using namespace realm;
using namespace realm::util;

static const std::string dummy_auth_url = "https://realm.example.org";

TEST_CASE("sync: Connection state changes", "[sync]") {
    if (!EventLoop::has_implementation())
        return;

    SyncServer server;
    // Disable file-related functionality and metadata functionality for testing purposes.
    SyncManager::shared().configure_file_system(tmp_dir(), SyncManager::MetadataMode::NoMetadata);
    auto user = SyncManager::shared().get_user({"user", dummy_auth_url}, "not_a_real_token");

    SECTION("register connection change listener") {
        auto session = sync_session(server, user, "/connection-state-changes-1",
                                    [](const auto &, const auto &) { return s_test_token; },
                                    [](auto, auto) {},
                                    SyncSessionStopPolicy::AfterChangesUploaded);

        std::atomic<bool> listener_called(false);
        auto token = session->register_connection_change_callback([&](SyncSession::PublicConnectionState, SyncSession::PublicConnectionState) {
            listener_called = true;
        });

        REQUIRE(session->connectionState() == SyncSession::PublicConnectionState::Connecting);
        REQUIRE(session->state() == SyncSession::PublicState::Active);
        user->log_out();
        EventLoop::main().run_until([&] { return sessions_are_inactive(*session); });
        REQUIRE(listener_called == true);
    }


    SECTION("unregister connection change listener") {
        auto session = sync_session(server, user, "/connection-state-changes-2",
                                           [](const auto &, const auto &) { return s_test_token; },
                                           [](auto, auto) {},
                                           SyncSessionStopPolicy::AfterChangesUploaded);

        std::atomic<bool> listener1_called(false);
        std::atomic<bool> listener2_called(false);
        auto token1 = session->register_connection_change_callback([&](SyncSession::PublicConnectionState , SyncSession::PublicConnectionState) {
            listener1_called = true;
        });
        session->unregister_connection_change_callback(token1);
        auto token2 = session->register_connection_change_callback([&](SyncSession::PublicConnectionState , SyncSession::PublicConnectionState) {
            listener2_called = true;
        });

        REQUIRE(session->connectionState() == SyncSession::PublicConnectionState::Connecting);
        REQUIRE(session->state() == SyncSession::PublicState::Active);
        user->log_out();
        EventLoop::main().run_until([&] { return sessions_are_inactive(*session); });
        REQUIRE(listener1_called == false);
        REQUIRE(listener2_called == true);
    }
}
