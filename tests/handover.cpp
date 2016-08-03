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

#include "util/test_file.hpp"

#include "list.hpp"
#include "object_accessor.hpp"
#include "object_schema.hpp"
#include "property.hpp"
#include "schema.hpp"
#include "handover.hpp"

#include <thread>

using namespace realm;

static TableRef get_table(Realm& realm, const ObjectSchema &object_schema) {
    StringData name (("class_" + object_schema.name).c_str());
    return realm.read_group().get_table(name);
}

static Object create_object(SharedRealm realm, const ObjectSchema &object_schema) {
    TableRef table = get_table(*realm, object_schema);
    return Object(std::move(realm), object_schema, Row((*table)[table->add_empty_row()]));
}

static List get_list(const Object& object, size_t column_ndx) {
    return List(object.realm(), object.row().get_table()->get_linklist(column_ndx, object.row().get_index()));
}

static Property nullable(Property p) {
    p.is_nullable = true;
    return p;
}

TEST_CASE("handover") {
    InMemoryTestFile config;
    config.cache = false;
    config.automatic_change_notifications = false;

    SharedRealm r = Realm::get_shared_realm(config);

    static const ObjectSchema string_object ({"string_object", {
        nullable({"value", PropertyType::String}),
    }});
    static const ObjectSchema int_object ({"int_object", {
        {"value", PropertyType::Int},
    }});
    static const ObjectSchema int_array_object ({"int_array_object", {
        {"value", PropertyType::Array, "int_object"}
    }});
    r->update_schema({string_object, int_object, int_array_object});

    SECTION("objects") {
        r->begin_transaction();
        Object str = create_object(r, string_object);
        Object num = create_object(r, int_object);
        r->commit_transaction();

        REQUIRE(str.row().get_string(0).is_null());
        REQUIRE(num.row().get_int(0) == 0);
        auto h = r->package_for_handover({{str}, {num}});
        std::thread t([h, config]{
            SharedRealm r = Realm::get_shared_realm(config);
            auto h_import = r->accept_handover(*h);
            Object str = h_import[0].get_object();
            Object num = h_import[1].get_object();

            REQUIRE(str.row().get_string(0).is_null());
            REQUIRE(num.row().get_int(0) == 0);
            r->begin_transaction();
            str.row().set_string(0, "the meaning of life");
            num.row().set_int(0, 42);
            r->commit_transaction();
            REQUIRE(str.row().get_string(0) == "the meaning of life");
            REQUIRE(num.row().get_int(0) == 42);
        });
        t.join();

        REQUIRE(str.row().get_string(0).is_null());
        REQUIRE(num.row().get_int(0) == 0);
        r->refresh();
        REQUIRE(str.row().get_string(0) == "the meaning of life");
        REQUIRE(num.row().get_int(0) == 42);
    }

    SECTION("array") {
        r->begin_transaction();
        Object zero = create_object(r, int_object);
        zero.row().set_int(0, 0);
        List lst = get_list(create_object(r, int_array_object), 0);
        lst.add(zero.row().get_index());
        r->commit_transaction();

        REQUIRE(lst.size() == 1);
        REQUIRE(lst.get(0).get_int(0) == 0);
        auto h = r->package_for_handover({{lst}});
        std::thread t([h, config]{
            SharedRealm r = Realm::get_shared_realm(config);
            auto h_import = r->accept_handover(*h);
            List lst = h_import[0].get_list();

            REQUIRE(lst.size() == 1);
            REQUIRE(lst.get(0).get_int(0) == 0);
            r->begin_transaction();
            lst.remove_all();
            Object one = create_object(r, int_object);
            one.row().set_int(0, 1);
            lst.add(one.row().get_index());
            Object two = create_object(r, int_object);
            two.row().set_int(0, 2);
            lst.add(two.row().get_index());
            r->commit_transaction(); 
            REQUIRE(lst.size() == 2);
            REQUIRE(lst.get(0).get_int(0) == 1);
            REQUIRE(lst.get(1).get_int(0) == 2);
        });
        t.join();

        REQUIRE(lst.size() == 1);
        REQUIRE(lst.get(0).get_int(0) == 0);
        r->refresh();
        REQUIRE(lst.size() == 2);
        REQUIRE(lst.get(0).get_int(0) == 1);
        REQUIRE(lst.get(1).get_int(0) == 2);
    }

    SECTION("results") {
        auto results = Results(r, get_table(*r, string_object)->where().not_equal(0, "C")).sort({{0}, {false}});

        r->begin_transaction();
        Object strA = create_object(r, string_object);
        strA.row().set_string(0, "A");
        Object strB = create_object(r, string_object);
        strB.row().set_string(0, "B");
        Object strC = create_object(r, string_object);
        strC.row().set_string(0, "C");
        Object strD = create_object(r, string_object);
        strD.row().set_string(0, "D");
        r->commit_transaction();

        REQUIRE(results.size() == 3);
        REQUIRE(results.get(0).get_string(0) == "D");
        REQUIRE(results.get(1).get_string(0) == "B");
        REQUIRE(results.get(2).get_string(0) == "A");
        auto h = r->package_for_handover({{results}});
        std::thread t([h, config]{
            SharedRealm r = Realm::get_shared_realm(config);
            auto h_import = r->accept_handover(*h);
            Results results = h_import[0].get_results();

            REQUIRE(results.size() == 3);
            REQUIRE(results.get(0).get_string(0) == "D");
            REQUIRE(results.get(1).get_string(0) == "B");
            REQUIRE(results.get(2).get_string(0) == "A");
            r->begin_transaction();
            get_table(*r, string_object)->move_last_over(results.get(2).get_index());
            get_table(*r, string_object)->move_last_over(results.get(0).get_index());
            Object strE = create_object(r, string_object);
            strE.row().set_string(0, "E");
            r->commit_transaction();
            REQUIRE(results.size() == 2);
            REQUIRE(results.get(0).get_string(0) == "E");
            REQUIRE(results.get(1).get_string(0) == "B");
        });
        t.join();

        REQUIRE(results.size() == 3);
        REQUIRE(results.get(0).get_string(0) == "D");
        REQUIRE(results.get(1).get_string(0) == "B");
        REQUIRE(results.get(2).get_string(0) == "A");
        r->refresh();
        REQUIRE(results.size() == 2);
        REQUIRE(results.get(0).get_string(0) == "E");
        REQUIRE(results.get(1).get_string(0) == "B");
    }
}
