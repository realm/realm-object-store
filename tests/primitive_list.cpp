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

#include "catch.hpp"

#include "util/test_file.hpp"
#include "util/index_helpers.hpp"

#include "binding_context.hpp"
#include "list.hpp"
#include "object.hpp"
#include "object_schema.hpp"
#include "property.hpp"
#include "results.hpp"
#include "schema.hpp"

#include "impl/realm_coordinator.hpp"

#include <realm/group_shared.hpp>
#include <realm/link_view.hpp>
#include <realm/version.hpp>

using namespace realm;

TEST_CASE("primitive list") {
    InMemoryTestFile config;
    config.automatic_change_notifications = false;
    config.cache = false;
    auto r = Realm::get_shared_realm(config);
    r->update_schema({
        {"object", {
            {"pk", PropertyType::Int, Property::IsPrimary{true}},

            {"bool", PropertyType::Array|PropertyType::Bool},
            {"int", PropertyType::Array|PropertyType::Int},
            {"float", PropertyType::Array|PropertyType::Float},
            {"double", PropertyType::Array|PropertyType::Double},
            {"string", PropertyType::Array|PropertyType::String},
            {"data", PropertyType::Array|PropertyType::Data},
            {"date", PropertyType::Array|PropertyType::Date},

            {"optional bool", PropertyType::Array|PropertyType::Bool|PropertyType::Nullable},
            {"optional int", PropertyType::Array|PropertyType::Int|PropertyType::Nullable},
            {"optional float", PropertyType::Array|PropertyType::Float|PropertyType::Nullable},
            {"optional double", PropertyType::Array|PropertyType::Double|PropertyType::Nullable},
            {"optional string", PropertyType::Array|PropertyType::String|PropertyType::Nullable},
            {"optional data", PropertyType::Array|PropertyType::Data|PropertyType::Nullable},
            {"optional date", PropertyType::Array|PropertyType::Date|PropertyType::Nullable},
        }}
    });
}
