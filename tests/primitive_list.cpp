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

#include "util/index_helpers.hpp"
#include "util/templated_test_case.hpp"
#include "util/test_file.hpp"

#include "binding_context.hpp"
#include "list.hpp"
#include "object.hpp"
#include "object_schema.hpp"
#include "property.hpp"
#include "results.hpp"
#include "schema.hpp"
#include "thread_safe_reference.hpp"

#include "impl/realm_coordinator.hpp"
#include "impl/object_accessor_impl.hpp"

#include <realm/group_shared.hpp>
#include <realm/link_view.hpp>
#include <realm/version.hpp>

#include <numeric>

using namespace realm;

template<typename> constexpr PropertyType property_type();
template<> constexpr PropertyType property_type<int64_t>() { return PropertyType::Int; }
template<> constexpr PropertyType property_type<bool>() { return PropertyType::Bool; }
template<> constexpr PropertyType property_type<float>() { return PropertyType::Float; }
template<> constexpr PropertyType property_type<double>() { return PropertyType::Double; }
template<> constexpr PropertyType property_type<StringData>() { return PropertyType::String; }
template<> constexpr PropertyType property_type<BinaryData>() { return PropertyType::Data; }
template<> constexpr PropertyType property_type<Timestamp>() { return PropertyType::Date; }

template<> constexpr PropertyType property_type<util::Optional<int64_t>>() { return PropertyType::Int|PropertyType::Nullable; }

template<typename T> std::vector<T> values();
template<> std::vector<int64_t> values<int64_t>() { return {3, 1, 2}; }
template<> std::vector<bool> values<bool>() { return {true, false}; }
template<> std::vector<float> values<float>() { return {3.3f, 1.1f, 2.2f}; }
template<> std::vector<double> values<double>() { return {3.3, 1.1, 2.2}; }
template<> std::vector<StringData> values<StringData>() { return {"c", "a", "b"}; }
template<> std::vector<BinaryData> values<BinaryData>() { return {BinaryData("a", 1)}; }
template<> std::vector<Timestamp> values<Timestamp>() { return {Timestamp(1, 1)}; }

template<> std::vector<util::Optional<int64_t>> values<util::Optional<int64_t>>() { return {3, 1, 2, util::none}; }

template<typename T, std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
static auto sum(std::vector<T> const& value) {
    return Approx(std::accumulate(begin(value), end(value), T{}));
}

template<typename T, std::enable_if_t<!std::is_arithmetic<T>::value, int> = 0>
static T sum(std::vector<T> const&) {
    return T();
}

TEMPLATE_TEST_CASE("primitive list", int64_t, bool, float, double, StringData, BinaryData, Timestamp/*, util::Optional<int64_t>*/) {
    const constexpr auto type = property_type<TestType>();
    auto values = ::values<TestType>();

    InMemoryTestFile config;
    config.automatic_change_notifications = false;
    config.cache = false;
    auto r = Realm::get_shared_realm(config);
    r->update_schema({
        {"object", {
            {"value", PropertyType::Array|type}
        }},
    });

    auto table = r->read_group().get_table("class_object");
    r->begin_transaction();
    table->add_empty_row();

    List list(r, *table, 0, 0);
    CppContext ctx(r);

    SECTION("get_realm()") {
        REQUIRE(list.get_realm() == r);
    }

    SECTION("get_query()") {
        REQUIRE(list.get_query().count() == 0);
        list.add(static_cast<TestType>(values[0]));
        REQUIRE(list.get_query().count() == 1);
    }

    SECTION("get_origin_row_index()") {
        REQUIRE(list.get_origin_row_index() == 0);
        table->insert_empty_row(0);
        REQUIRE(list.get_origin_row_index() == 1);
    }

    SECTION("get_type()") {
        REQUIRE(list.get_type() == property_type<TestType>());
    }

    SECTION("is_optional()") {
        REQUIRE_FALSE(list.is_optional());
    }

    SECTION("is_valid()") {
        REQUIRE(list.is_valid());

        SECTION("invalidate") {
            r->invalidate();
            REQUIRE_FALSE(list.is_valid());
        }

        SECTION("close") {
            r->close();
            REQUIRE_FALSE(list.is_valid());
        }

        SECTION("delete row") {
            table->move_last_over(0);
            REQUIRE_FALSE(list.is_valid());
        }

        SECTION("rollback transaction creating list") {
            r->cancel_transaction();
            REQUIRE_THROWS(list.verify_attached());
        }
    }

    SECTION("verify_attached()") {
        REQUIRE_NOTHROW(list.verify_attached());

        SECTION("invalidate") {
            r->invalidate();
            REQUIRE_THROWS(list.verify_attached());
        }

        SECTION("close") {
            r->close();
            REQUIRE_THROWS(list.verify_attached());
        }

        SECTION("delete row") {
            table->move_last_over(0);
            REQUIRE_THROWS(list.verify_attached());
        }

        SECTION("rollback transaction creating list") {
            r->cancel_transaction();
            REQUIRE_THROWS(list.verify_attached());
        }
    }

    SECTION("verify_in_transaction()") {
        REQUIRE_NOTHROW(list.verify_in_transaction());

        SECTION("invalidate") {
            r->invalidate();
            REQUIRE_THROWS(list.verify_in_transaction());
        }

        SECTION("close") {
            r->close();
            REQUIRE_THROWS(list.verify_in_transaction());
        }

        SECTION("delete row") {
            table->move_last_over(0);
            REQUIRE_THROWS(list.verify_in_transaction());
        }

        SECTION("end write") {
            r->commit_transaction();
            REQUIRE_THROWS(list.verify_in_transaction());
        }
    }

    if (!list.is_valid() || !r->is_in_transaction())
        return;

    for (TestType value : values)
        list.add(value);

    SECTION("move()") {
        if (list.size() < 3)
            return;

        list.move(1, 2);
        REQUIRE(list.size() == values.size());
        REQUIRE(list.get<TestType>(1) == values[2]);
        REQUIRE(list.get<TestType>(2) == values[1]);
    }

    SECTION("remove()") {
        if (list.size() < 3)
            return;

        list.remove(1);
        REQUIRE(list.size() == values.size() - 1);
        REQUIRE(list.get<TestType>(0) == values[0]);
        REQUIRE(list.get<TestType>(1) == values[2]);
    }

    SECTION("remove_all()") {
        list.remove_all();
        REQUIRE(list.size() == 0);
    }

    SECTION("swap()") {
        if (list.size() < 3)
            return;

        list.swap(0, 2);
        REQUIRE(list.size() == values.size());
        REQUIRE(list.get<TestType>(0) == values[2]);
        REQUIRE(list.get<TestType>(1) == values[1]);
        REQUIRE(list.get<TestType>(2) == values[0]);
    }

    SECTION("delete_all()") {
        list.delete_all();
        REQUIRE(list.size() == 0);
    }

    SECTION("get()") {
        for (size_t i = 0; i < values.size(); ++i) {
            REQUIRE(list.get<TestType>(i) == values[i]);
        }
    }

    SECTION("set()") {
        for (size_t i = 0; i < values.size(); ++i) {
            auto rev = values.size() - i - 1;
            list.set(i, static_cast<TestType>(values[rev]));
            REQUIRE(list.get<TestType>(i) == values[rev]);
        }
        REQUIRE_THROWS(list.set(list.size(), static_cast<TestType>(values[0])));
    }

    SECTION("find()") {
        for (size_t i = 0; i < values.size(); ++i)
            REQUIRE(list.find((TestType)values[i]) == i);
        list.remove(0);
        REQUIRE(list.find((TestType)values[0]) == npos);
    }

    SECTION("sort()") {
        auto subtable = table->get_subtable(0, 0);

        auto sorted = list.sort(SortDescriptor(*subtable, {{0}}, {true}));
        std::sort(begin(values), end(values));
        for (size_t i = 0; i < values.size(); ++i)
            REQUIRE(sorted.get<TestType>(i) == values[i]);

        sorted = list.sort(SortDescriptor(*subtable, {{0}}, {false}));
        std::sort(begin(values), end(values), std::greater<>());
        for (size_t i = 0; i < values.size(); ++i)
            REQUIRE(sorted.get<TestType>(i) == values[i]);
    }

    SECTION("filter()") {
    }

    SECTION("min()") {
        switch (type) {
            case PropertyType::Int:
            case PropertyType::Float:
            case PropertyType::Double:
            case PropertyType::Date:
                break;
            default:
                REQUIRE_THROWS(list.min<TestType>());
                return;
        }

        REQUIRE(list.min<TestType>() == *std::min_element(begin(values), end(values)));
        list.remove_all();
        REQUIRE(list.min<TestType>() == util::none);
    }

    SECTION("max()") {
        switch (type) {
            case PropertyType::Int:
            case PropertyType::Float:
            case PropertyType::Double:
            case PropertyType::Date:
                break;
            default:
                REQUIRE_THROWS(list.max<TestType>());
                return;
        }

        REQUIRE(list.max<TestType>() == *std::max_element(begin(values), end(values)));
        list.remove_all();
        REQUIRE(list.max<TestType>() == util::none);
    }

    SECTION("sum()") {
        switch (type) {
            case PropertyType::Int:
            case PropertyType::Float:
            case PropertyType::Double:
                break;
            default:
                REQUIRE_THROWS(list.sum<TestType>());
                return;
        }

        REQUIRE(list.sum<TestType>() == sum(values));
        list.remove_all();
        REQUIRE(list.sum<TestType>() == TestType{});
    }

    SECTION("average()") {
        switch (type) {
            case PropertyType::Int:
            case PropertyType::Float:
            case PropertyType::Double:
                break;
            default:
                REQUIRE_THROWS(list.average<TestType>());
                return;
        }

        REQUIRE(list.average<TestType>() == sum(values));
        list.remove_all();
        REQUIRE(list.average<TestType>() == util::none);
    }

    SECTION("operator==()") {
        table->add_empty_row();
        REQUIRE(list == List(r, *table, 0, 0));
        REQUIRE_FALSE(list == List(r, *table, 0, 1));
    }

    SECTION("hash") {
        table->add_empty_row();
        std::hash<List> h;
        REQUIRE(h(list) == h(List(r, *table, 0, 0)));
        REQUIRE_FALSE(h(list) == h(List(r, *table, 0, 1)));
    }

    SECTION("handover") {
        r->commit_transaction();

        auto handover = r->obtain_thread_safe_reference(list);
        auto list2 = r->resolve_thread_safe_reference(std::move(handover));
        REQUIRE(list == list2);
    }

    SECTION("notifications") {
        r->commit_transaction();

        CollectionChangeSet change;
        SECTION("add value to list") {
            auto token = list.add_notification_callback([&](CollectionChangeSet c, std::exception_ptr) {
                change = c;
            });
            advance_and_notify(*r);

            r->begin_transaction();
            list.insert(0, static_cast<TestType>(values[0]));
            r->commit_transaction();
            advance_and_notify(*r);
            REQUIRE_INDICES(change.insertions, 0);
        }

        SECTION("delete containing row") {
            size_t calls = 0;
            auto token = list.add_notification_callback([&](CollectionChangeSet c, std::exception_ptr) {
                change = c;
                ++calls;
            });
            advance_and_notify(*r);
            REQUIRE(calls == 1);

            r->begin_transaction();
            table->move_last_over(0);
            r->commit_transaction();
            advance_and_notify(*r);
            REQUIRE(calls == 2);
            REQUIRE(change.deletions.count() == values.size());

            r->begin_transaction();
            table->add_empty_row();
            r->commit_transaction();
            advance_and_notify(*r);
            REQUIRE(calls == 2);
        }
    }
}
