////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
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
#include "object_schema.hpp"
#include "property.hpp"
#include "results.hpp"
#include "schema.hpp"

#include "impl/realm_coordinator.hpp"

#include <realm/group_shared.hpp>
#include <realm/link_view.hpp>
#include <realm/version.hpp>

using namespace realm;

TEST_CASE("list") {
    InMemoryTestFile config;
    config.automatic_change_notifications = false;
    config.cache = false;
    auto r = Realm::get_shared_realm(config);
    r->update_schema({
        {"origin", {
            {"pk", PropertyType::Int, "", "", true},
            {"array", PropertyType::Array, "target"}
        }},
        {"target", {
            {"value", PropertyType::Int}
        }},
        {"other_origin", {
            {"array", PropertyType::Array, "other_target"}
        }},
        {"other_target", {
            {"value", PropertyType::Int}
        }},
    });

    auto& coordinator = *_impl::RealmCoordinator::get_existing_coordinator(config.path);

    auto origin = r->read_group().get_table("class_origin");
    auto target = r->read_group().get_table("class_target");
    auto other_origin = r->read_group().get_table("class_other_origin");
    auto other_target = r->read_group().get_table("class_other_target");

    r->begin_transaction();

    target->add_empty_row(10);
    for (int i = 0; i < 10; ++i)
        target->set_int(0, i, i);

    origin->add_empty_row(2);
    origin->set_int_unique(0, 0, 1);
    origin->set_int_unique(0, 1, 2);
    LinkViewRef lv = origin->get_linklist(1, 0);
    for (int i = 0; i < 10; ++i)
        lv->add(i);
    LinkViewRef lv2 = origin->get_linklist(1, 1);
    for (int i = 0; i < 10; ++i)
        lv2->add(i);

    other_origin->add_empty_row();
    other_target->add_empty_row(10);
    for (int i = 0; i < 10; ++i)
        other_target->set_int(0, i, i);
    LinkViewRef other_lv = other_origin->get_linklist(0, 0);
    for (int i = 0; i < 10; ++i)
        other_lv->add(i);

    r->commit_transaction();

    auto r2 = coordinator.get_realm();
    auto r2_lv = r2->read_group().get_table("class_origin")->get_linklist(1, 0);

    SECTION("add_notification_block()") {
        CollectionChangeSet change;
        List lst(r, lv);

        auto write = [&](auto&& f) {
            r->begin_transaction();
            f();
            r->commit_transaction();

            advance_and_notify(*r);
        };

        auto require_change = [&] {
            auto token = lst.add_notification_callback([&](CollectionChangeSet c, std::exception_ptr) {
                change = c;
            });
            advance_and_notify(*r);
            return token;
        };

        auto require_no_change = [&] {
            bool first = true;
            auto token = lst.add_notification_callback([&, first](CollectionChangeSet, std::exception_ptr) mutable {
                REQUIRE(first);
                first = false;
            });
            advance_and_notify(*r);
            return token;
        };

        SECTION("modifying the list sends a change notifications") {
            auto token = require_change();
            write([&] { lst.remove(5); });
            REQUIRE_INDICES(change.deletions, 5);
        }

        SECTION("modifying a different list doesn't send a change notification") {
            auto token = require_no_change();
            write([&] { lv2->remove(5); });
        }

        SECTION("deleting the list sends a change notification") {
            auto token = require_change();
            write([&] { origin->move_last_over(0); });
            REQUIRE_INDICES(change.deletions, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);

            // Should not resend delete all notification after another commit
            change = {};
            write([&] { target->add_empty_row(); });
            REQUIRE(change.empty());
        }

        SECTION("modifying one of the target rows sends a change notification") {
            auto token = require_change();
            write([&] { lst.get(5).set_int(0, 6); });
            REQUIRE_INDICES(change.modifications, 5);
        }

        SECTION("deleting a target row sends a change notification") {
            auto token = require_change();
            write([&] { target->move_last_over(5); });
            REQUIRE_INDICES(change.deletions, 5);
        }

        SECTION("adding a row and then modifying the target row does not mark the row as modified") {
            auto token = require_change();
            write([&] {
                lst.add(5);
                target->set_int(0, 5, 10);
            });
            REQUIRE_INDICES(change.insertions, 10);
            REQUIRE_INDICES(change.modifications, 5);
        }

        SECTION("modifying and then moving a row reports move/insert but not modification") {
            auto token = require_change();
            write([&] {
                target->set_int(0, 5, 10);
                lst.move(5, 8);
            });
            REQUIRE_INDICES(change.insertions, 8);
            REQUIRE_INDICES(change.deletions, 5);
            REQUIRE_MOVES(change, {5, 8});
            REQUIRE(change.modifications.empty());
        }

        SECTION("modifying a row which appears multiple times in a list marks them all as modified") {
            r->begin_transaction();
            lst.add(5);
            r->commit_transaction();

            auto token = require_change();
            write([&] { target->set_int(0, 5, 10); });
            REQUIRE_INDICES(change.modifications, 5, 10);
        }

        SECTION("deleting a row which appears multiple times in a list marks them all as modified") {
            r->begin_transaction();
            lst.add(5);
            r->commit_transaction();

            auto token = require_change();
            write([&] { target->move_last_over(5); });
            REQUIRE_INDICES(change.deletions, 5, 10);
        }

        SECTION("clearing the target table sends a change notification") {
            auto token = require_change();
            write([&] { target->clear(); });
            REQUIRE_INDICES(change.deletions, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
        }

        SECTION("moving a target row does not send a change notification") {
            // Remove a row from the LV so that we have one to delete that's not in the list
            r->begin_transaction();
            lv->remove(2);
            r->commit_transaction();

            auto token = require_no_change();
            write([&] { target->move_last_over(2); });
        }

        SECTION("multiple LinkViws for the same LinkList can get notifications") {
            r->begin_transaction();
            target->clear();
            target->add_empty_row(5);
            r->commit_transaction();

            auto get_list = [&] {
                auto r = Realm::get_shared_realm(config);
                auto lv = r->read_group().get_table("class_origin")->get_linklist(1, 0);
                return List(r, lv);
            };
            auto change_list = [&] {
                r->begin_transaction();
                if (lv->size()) {
                    target->set_int(0, lv->size() - 1, lv->size());
                }
                lv->add(lv->size());
                r->commit_transaction();
            };

            List lists[3];
            NotificationToken tokens[3];
            CollectionChangeSet changes[3];

            for (int i = 0; i < 3; ++i) {
                lists[i] = get_list();
                tokens[i] = lists[i].add_notification_callback([i, &changes](CollectionChangeSet c, std::exception_ptr) {
                    changes[i] = std::move(c);
                });
                change_list();
            }

            // Each of the Lists now has a different source version and state at
            // that version, so they should all see different changes despite
            // being for the same LinkList
            for (auto& list : lists)
                advance_and_notify(*list.get_realm());

            REQUIRE_INDICES(changes[0].insertions, 0, 1, 2);
            REQUIRE(changes[0].modifications.empty());

            REQUIRE_INDICES(changes[1].insertions, 1, 2);
            REQUIRE_INDICES(changes[1].modifications, 0);

            REQUIRE_INDICES(changes[2].insertions, 2);
            REQUIRE_INDICES(changes[2].modifications, 1);

            // After making another change, they should all get the same notification
            change_list();
            for (auto& list : lists)
                advance_and_notify(*list.get_realm());

            for (int i = 0; i < 3; ++i) {
                REQUIRE_INDICES(changes[i].insertions, 3);
                REQUIRE_INDICES(changes[i].modifications, 2);
            }
        }

        SECTION("multiple callbacks for the same Lists can be skipped individually") {
            auto token = require_no_change();
            auto token2 = require_change();

            r->begin_transaction();
            lv->add(0);
            token.suppress_next();
            r->commit_transaction();

            advance_and_notify(*r);
            REQUIRE_INDICES(change.insertions, 10);
        }

        SECTION("multiple Lists for the same LinkView can be skipped individually") {
            auto token = require_no_change();

            List list2(r, lv);
            auto token2 = list2.add_notification_callback([&](CollectionChangeSet c, std::exception_ptr) {
                change = c;
            });
            advance_and_notify(*r);

            r->begin_transaction();
            lv->add(0);
            token.suppress_next();
            r->commit_transaction();

            advance_and_notify(*r);
            REQUIRE_INDICES(change.insertions, 10);
        }

        SECTION("modifying a different table does not send a change notification") {
            auto token = require_no_change();
            write([&] { other_lv->add(0); });
        }

        SECTION("changes are reported correctly for multiple tables") {
            List lst2(r, other_lv);
            CollectionChangeSet other_changes;
            auto token1 = lst2.add_notification_callback([&](CollectionChangeSet c, std::exception_ptr) {
                other_changes = std::move(c);
            });
            auto token2 = require_change();

            write([&] {
                lv->add(1);

                other_origin->insert_empty_row(0);
                other_lv->insert(1, 0);

                lv->add(2);
            });
            REQUIRE_INDICES(change.insertions, 10, 11);
            REQUIRE_INDICES(other_changes.insertions, 1);

            write([&] {
                lv->add(3);
                other_origin->move_last_over(1);
                lv->add(4);
            });
            REQUIRE_INDICES(change.insertions, 12, 13);
            REQUIRE_INDICES(other_changes.deletions, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

            write([&] {
                lv->add(5);
                other_origin->clear();
                lv->add(6);
            });
            REQUIRE_INDICES(change.insertions, 14, 15);
        }

        SECTION("tables-of-interest are tracked properly for multiple source versions") {
            // Add notifiers for different tables at different versions to verify
            // that the tables of interest are updated correctly as we process
            // new notifiers
            CollectionChangeSet changes1, changes2;
            auto token1 = lst.add_notification_callback([&](CollectionChangeSet c, std::exception_ptr) {
                changes1 = std::move(c);
            });

            r2->begin_transaction();
            r2->read_group().get_table("class_target")->set_int(0, 0, 10);
            r2->read_group().get_table("class_other_target")->set_int(0, 1, 10);
            r2->commit_transaction();

            List lst2(r2, r2->read_group().get_table("class_other_origin")->get_linklist(0, 0));
            auto token2 = lst2.add_notification_callback([&](CollectionChangeSet c, std::exception_ptr) {
                changes2 = std::move(c);
            });

            auto r3 = coordinator.get_realm();
            r3->begin_transaction();
            r3->read_group().get_table("class_target")->set_int(0, 2, 10);
            r3->read_group().get_table("class_other_target")->set_int(0, 3, 10);
            r3->commit_transaction();

            advance_and_notify(*r);
            advance_and_notify(*r2);

            REQUIRE_INDICES(changes1.modifications, 0, 2);
            REQUIRE_INDICES(changes2.modifications, 3);
        }

        SECTION("modifications are reported for rows that are moved and then moved back in a second transaction") {
            auto token = require_change();

            r2->begin_transaction();
            r2_lv->get(5).set_int(0, 10);
            r2_lv->get(1).set_int(0, 10);
            r2_lv->move(5, 8);
            r2_lv->move(1, 2);
            r2->commit_transaction();

            coordinator.on_change();

            r2->begin_transaction();
            r2_lv->move(8, 5);
            r2->commit_transaction();
            advance_and_notify(*r);

            REQUIRE_INDICES(change.deletions, 1);
            REQUIRE_INDICES(change.insertions, 2);
            REQUIRE_INDICES(change.modifications, 5);
            REQUIRE_MOVES(change, {1, 2});
        }

        SECTION("moving the list's containing row does not break notifications") {
            auto token = require_change();

            // insert rows before it
            write([&] {
                origin->insert_empty_row(0, 2);
                lv->add(1);
            });
            REQUIRE_INDICES(change.insertions, 10);
            REQUIRE(lst.size() == 11);
            REQUIRE(lst.get(10).get_index() == 1);

            // swap the row containing it with another row
            write([&] {
                origin->swap_rows(2, 3);
                lv->add(2);
            });
            REQUIRE_INDICES(change.insertions, 11);
            REQUIRE(lst.size() == 12);
            REQUIRE(lst.get(11).get_index() == 2);

            // swap it back to verify both of the rows in the swap are handled
            write([&] {
                origin->swap_rows(2, 3);
                lv->add(3);
            });
            REQUIRE_INDICES(change.insertions, 12);
            REQUIRE(lst.size() == 13);
            REQUIRE(lst.get(12).get_index() == 3);

            // delete a row so that it's moved (as it's at the end)
            write([&] {
                origin->move_last_over(0);
                lv->add(4);
            });
            REQUIRE_INDICES(change.insertions, 13);
            REQUIRE(lst.size() == 14);
            REQUIRE(lst.get(13).get_index() == 4);
        }

        SECTION("changes are sent in initial notification") {
            auto token = lst.add_notification_callback([&](CollectionChangeSet c, std::exception_ptr) {
                change = c;
            });
            r2->begin_transaction();
            r2_lv->remove(5);
            r2->commit_transaction();
            advance_and_notify(*r);
            REQUIRE_INDICES(change.deletions, 5);
        }
    }

    SECTION("sorted add_notification_block()") {
        List lst(r, lv);
        Results results = lst.sort({*target, {{0}}, {false}});

        int notification_calls = 0;
        CollectionChangeSet change;
        auto token = results.add_notification_callback([&](CollectionChangeSet c, std::exception_ptr err) {
            REQUIRE_FALSE(err);
            change = c;
            ++notification_calls;
        });

        advance_and_notify(*r);

        auto write = [&](auto&& f) {
            r->begin_transaction();
            f();
            r->commit_transaction();

            advance_and_notify(*r);
        };

        SECTION("add duplicates") {
            write([&] {
                lst.add(5);
                lst.add(5);
                lst.add(5);
            });
            REQUIRE(notification_calls == 2);
            REQUIRE_INDICES(change.insertions, 5, 6, 7);
        }

        SECTION("change order by modifying target") {
            write([&] {
                lst.get(5).set_int(0, 15);
            });
            REQUIRE(notification_calls == 2);
            REQUIRE_INDICES(change.deletions, 4);
            REQUIRE_INDICES(change.insertions, 0);
        }

        SECTION("swap") {
            write([&] {
                lst.swap(1, 2);
            });
            REQUIRE(notification_calls == 1);
        }

        SECTION("move") {
            write([&] {
                lst.move(5, 3);
            });
            REQUIRE(notification_calls == 1);
        }
    }

    SECTION("filtered add_notification_block()") {
        List lst(r, lv);
        Results results = lst.filter(target->where().less(0, 9));

        int notification_calls = 0;
        CollectionChangeSet change;
        auto token = results.add_notification_callback([&](CollectionChangeSet c, std::exception_ptr err) {
            REQUIRE_FALSE(err);
            change = c;
            ++notification_calls;
        });

        advance_and_notify(*r);

        auto write = [&](auto&& f) {
            r->begin_transaction();
            f();
            r->commit_transaction();

            advance_and_notify(*r);
        };

        SECTION("add duplicates") {
            write([&] {
                lst.add(5);
                lst.add(5);
                lst.add(5);
            });
            REQUIRE(notification_calls == 2);
            REQUIRE_INDICES(change.insertions, 9, 10, 11);
        }

        SECTION("swap") {
            write([&] {
                lst.swap(1, 2);
            });
            REQUIRE(notification_calls == 2);
            REQUIRE_INDICES(change.deletions, 2);
            REQUIRE_INDICES(change.insertions, 1);

            write([&] {
                lst.swap(5, 8);
            });
            REQUIRE(notification_calls == 3);
            REQUIRE_INDICES(change.deletions, 5, 8);
            REQUIRE_INDICES(change.insertions, 5, 8);
        }

        SECTION("move") {
            write([&] {
                lst.move(5, 3);
            });
            REQUIRE(notification_calls == 2);
            REQUIRE_INDICES(change.deletions, 5);
            REQUIRE_INDICES(change.insertions, 3);
        }

        SECTION("move non-matching entry") {
            write([&] {
                lst.move(9, 3);
            });
            REQUIRE(notification_calls == 1);
        }
    }

    SECTION("sort()") {
        auto objectschema = &*r->schema().find("target");
        List list(r, lv);
        auto results = list.sort({*target, {{0}}, {false}});

        REQUIRE(&results.get_object_schema() == objectschema);
        REQUIRE(results.get_mode() == Results::Mode::LinkView);
        REQUIRE(results.size() == 10);
        REQUIRE(results.sum(0) == 45);

        for (size_t i = 0; i < 10; ++i) {
            REQUIRE(results.get(i).get_index() == 9 - i);
        }
    }

    SECTION("filter()") {
        auto objectschema = &*r->schema().find("target");
        List list(r, lv);
        auto results = list.filter(target->where().greater(0, 5));

        REQUIRE(&results.get_object_schema() == objectschema);
        REQUIRE(results.get_mode() == Results::Mode::Query);
        REQUIRE(results.size() == 4);

        for (size_t i = 0; i < 4; ++i) {
            REQUIRE(results.get(i).get_index() == i + 6);
        }
    }

    SECTION("snapshot()") {
        auto objectschema = &*r->schema().find("target");
        List list(r, lv);

        auto snapshot = list.snapshot();
        REQUIRE(&snapshot.get_object_schema() == objectschema);
        REQUIRE(snapshot.get_mode() == Results::Mode::TableView);
        REQUIRE(snapshot.size() == 10);

        r->begin_transaction();
        for (size_t i = 0; i < 5; ++i) {
            list.remove(0);
        }
        REQUIRE(snapshot.size() == 10);
        for (size_t i = 0; i < snapshot.size(); ++i) {
            REQUIRE(snapshot.get(i).is_attached());
        }
        for (size_t i = 0; i < 5; ++i) {
            target->move_last_over(i);
        }
        REQUIRE(snapshot.size() == 10);
        for (size_t i = 0; i < 5; ++i) {
            REQUIRE(!snapshot.get(i).is_attached());
        }
        for (size_t i = 5; i < 10; ++i) {
            REQUIRE(snapshot.get(i).is_attached());
        }
        list.add(0);
        REQUIRE(snapshot.size() == 10);
    }

    SECTION("get_object_schema()") {
        List list(r, lv);
        auto objectschema = &*r->schema().find("target");
        REQUIRE(&list.get_object_schema() == objectschema);
    }
}
