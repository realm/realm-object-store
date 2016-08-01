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

static Row create_object(TableRef table) {
    return Row((*table)[table->add_empty_row()]);
}

static List get_list(Row row, size_t column_ndx, SharedRealm& realm) {
    return List(realm, row.get_table()->get_linklist(column_ndx, row.get_index()));
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
    r->update_schema({
        {"string_object", {
            nullable({"value", PropertyType::String}),
        }},
        {"int_object", {
            {"value", PropertyType::Int},
        }},
        {"int_array_object", {
            {"value", PropertyType::Array, "int_object"}
        }},
    });

    static auto string_object_table = [](SharedRealm& r) -> TableRef {
        return r->read_group().get_table("class_string_object");
    };
    static auto int_object_table = [](SharedRealm& r) -> TableRef {
        return r->read_group().get_table("class_int_object");
    };
    static auto int_array_object_table = [](SharedRealm& r) -> TableRef {
        return r->read_group().get_table("class_int_array_object");
    };

    SECTION("objects") {
        r->begin_transaction();
        Row str = create_object(string_object_table(r));
        Row num = create_object(int_object_table(r));
        r->commit_transaction();

        REQUIRE(str.get_string(0).is_null());
        REQUIRE(num.get_int(0) == 0);
        auto h = r->package_for_handover({{str}, {num}});
        std::thread t([h, config]{
            SharedRealm r = Realm::get_shared_realm(config);
            auto h_import = r->accept_handover(*h);
            Row str = h_import[0].get_row();
            Row num = h_import[1].get_row();

            REQUIRE(str.get_string(0).is_null());
            REQUIRE(num.get_int(0) == 0);
            r->begin_transaction();
            str.set_string(0, "the meaning of life");
            num.set_int(0, 42);
            r->commit_transaction();
            REQUIRE(str.get_string(0) == "the meaning of life");
            REQUIRE(num.get_int(0) == 42);
        });
        t.join();

        REQUIRE(str.get_string(0).is_null());
        REQUIRE(num.get_int(0) == 0);
        r->refresh();
        REQUIRE(str.get_string(0) == "the meaning of life");
        REQUIRE(num.get_int(0) == 42);
    }

    SECTION("array") {
        r->begin_transaction();
        Row zero = create_object(int_object_table(r));
        zero.set_int(0, 0);
        List lst = get_list(create_object(int_array_object_table(r)), 0, r);
        lst.add(zero.get_index());
        r->commit_transaction();

        REQUIRE(lst.size() == 1);
        REQUIRE(lst.get(0).get_int(0) == 0);
        auto h = r->package_for_handover({{lst.get_linkview()}});
        std::thread t([h, config]{
            SharedRealm r = Realm::get_shared_realm(config);
            auto h_import = r->accept_handover(*h);
            List lst(r, h_import[0].get_link_view_ref());

            REQUIRE(lst.size() == 1);
            REQUIRE(lst.get(0).get_int(0) == 0);
            r->begin_transaction();
            lst.remove_all();
            Row one = create_object(int_object_table(r));
            one.set_int(0, 1);
            lst.add(one.get_index());
            Row two = create_object(int_object_table(r));
            two.set_int(0, 2);
            lst.add(two.get_index());
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
        auto results = Results(r, string_object_table(r)->where().not_equal(0, "C")).sort({{0}, {false}});

        r->begin_transaction();
        Row strA = create_object(string_object_table(r));
        strA.set_string(0, "A");
        Row strB = create_object(string_object_table(r));
        strB.set_string(0, "B");
        Row strC = create_object(string_object_table(r));
        strC.set_string(0, "C");
        Row strD = create_object(string_object_table(r));
        strD.set_string(0, "D");
        r->commit_transaction();

        REQUIRE(results.size() == 3);
        REQUIRE(results.get(0).get_string(0) == "D");
        REQUIRE(results.get(1).get_string(0) == "B");
        REQUIRE(results.get(2).get_string(0) == "A");
        auto h = r->package_for_handover({{results.get_query()}});
        std::thread t([h, config]{
            SharedRealm r = Realm::get_shared_realm(config);
            auto h_import = r->accept_handover(*h);
            Results results(r, h_import[0].get_query(), {{0}, {false}});

            REQUIRE(results.size() == 3);
            REQUIRE(results.get(0).get_string(0) == "D");
            REQUIRE(results.get(1).get_string(0) == "B");
            REQUIRE(results.get(2).get_string(0) == "A");
            r->begin_transaction();
            string_object_table(r)->move_last_over(results.get(2).get_index());
            string_object_table(r)->move_last_over(results.get(0).get_index());
            Row strE = create_object(string_object_table(r));
            strE.set_string(0, "E");
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
