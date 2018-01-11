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
#include "object_accessor.hpp"
#include "impl/realm_coordinator.hpp"
#include "impl/object_accessor_impl.hpp"
#include "util/test_file.hpp"

// This class contain tests for the interaction between Strong and Weak links.
// See Descriptor::set_link_type() in Core (descriptor.hpp) for an extended explanation of the exact semantics.

namespace realm {
class TestHelper {
public:
    static SharedRealm get_realm() {
        TestFile config;
        config.schema_version = 1;
        Schema schema = {
                // DAG Object hierarchy: (A -> Child) and (B -> Child)
                {"WeakParentA", {
                    {"weakObjectRef",  PropertyType::Object|PropertyType::Nullable, "Child", Relationship::Weak},
                    {"weakArrayRef", PropertyType::Array|PropertyType::Object, "Child", Relationship::Weak},
                }},
                {"StrongParentB", {
                    {"strongObjectRef", PropertyType::Object|PropertyType::Nullable, "Child", Relationship::Strong},
                    {"strongArrayRef", PropertyType::Array|PropertyType::Object, "Child", Relationship::Strong},
                }},
                {"Child", {
                    {"prop", PropertyType::String},
                }},

                // Cyclic Object hierarchy: (C -> D) and (D -> C)
                {"CycleC", {
                    {"weakObjectRef", PropertyType::Object|PropertyType::Nullable, "CycleD", Relationship::Weak},
                    {"strongObjectRef", PropertyType::Object|PropertyType::Nullable, "CycleD", Relationship::Strong},
                    {"weakArrayRef", PropertyType::Array|PropertyType::Object, "CycleD", Relationship::Weak},
                    {"strongArrayRef", PropertyType::Array|PropertyType::Object, "CycleD", Relationship::Strong},
                }},
                {"CycleD", {
                    {"weakObjectRef", PropertyType::Object|PropertyType::Nullable, "CycleC", Relationship::Weak},
                    {"strongObjectRef", PropertyType::Object|PropertyType::Nullable, "CycleC", Relationship::Strong},
                    {"weakArrayRef", PropertyType::Array|PropertyType::Object, "CycleC", Relationship::Weak},
                    {"strongArrayRef", PropertyType::Array|PropertyType::Object, "CycleC", Relationship::Strong},
                }},

                // Cyclic Object hierarchy: (E -> E)
                {"SingleClassCycleE", {
                    {"weakObjectRef", PropertyType::Object|PropertyType::Nullable, "SingleClassCycleE", Relationship::Weak},
                    {"strongObjectRef", PropertyType::Object|PropertyType::Nullable, "SingleClassCycleE", Relationship::Strong},
                    {"weakArrayRef", PropertyType::Array|PropertyType::Object, "SingleClassCycleE", Relationship::Weak},
                    {"strongArrayRef", PropertyType::Array|PropertyType::Object, "SingleClassCycleE", Relationship::Strong},
               }},

                // Island graphs: (F -> (C <-> D))
                {"IslandParentF", {
                   {"strongObjectRef", PropertyType::Object|PropertyType::Nullable, "CycleC", Relationship::Strong},
                }},
        };
        config.schema = schema;

        return Realm::get_shared_realm(config);
    };

    static long get_count(SharedRealm realm, ObjectSchema object_class_schema) {
        return ObjectStore::table_for_object_type(realm->read_group(), object_class_schema.name)->size();
    }
};
}

using namespace realm;
using namespace std::string_literals;

TEST_CASE("If no more strong links exist, objects are deleted") {
    SharedRealm realm = TestHelper::get_realm();
    CppContext context(realm);
    auto child_schema = *realm->schema().find("Child");
    auto weak_parent_schema = *realm->schema().find("WeakParentA");
    auto strong_parent_schema = *realm->schema().find("StrongParentB");
    auto cyclic_schema_c = *realm->schema().find("CycleC");
    auto cyclic_schema_d = *realm->schema().find("CycleD");
    auto cyclic_schema_e = *realm->schema().find("SingleClassCycleE");
    auto island_parent_schema = *realm->schema().find("IslandParentF");

    SECTION("DAG with only strong links and they are all removed") {
        // Create data with links
        realm->begin_transaction();
        auto child = Object::create<Any>(context, realm, child_schema, AnyDict{{ "prop", "childA"s }}, false);
        auto parent = Object::create<Any>(context, realm, strong_parent_schema, AnyDict{
                { "strongObjectRef", child },
                { "strongArrayRef", AnyVector{ child }}
        });
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, child_schema));

        // Remove first direct Strong link
        realm->begin_transaction();
        parent.set_property_value(context, "strongObjectRef", Any(), true);
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, child_schema));

        // Remove second Strong link through a List = no more strong links
        realm->begin_transaction();
        parent.set_property_value(context, "strongArrayRef", Any(), true);
        realm->commit_transaction();
        REQUIRE(0 == TestHelper::get_count(realm, child_schema));
    }

    SECTION("DAG with mix of weak and strong links and all Strong links are removed") {
        // Create mix of links to same object
        realm->begin_transaction();
        auto child = Object::create<Any>(context, realm, child_schema, AnyDict{{ "prop", "childA"s }}, false);
        auto weak_parent = Object::create<Any>(context, realm, weak_parent_schema, AnyDict{ {"weakObjectRef", child} }, false);
        auto strong_parent = Object::create<Any>(context, realm, strong_parent_schema, AnyDict{ {"strongObjectRef", child} }, false);
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, child_schema));

        // Removing Strong link should delete object
        realm->begin_transaction();
        strong_parent.set_property_value(context, "strongObjectRef", Any(), true);
        realm->commit_transaction();
        REQUIRE(0 == TestHelper::get_count(realm, child_schema));
    }

    // Special case described in descriptor.hpp: (A <-> B) -> (A -> B) = Both objects are deleted.
    SECTION("Breaking an isolated strong cycle deletes both objects") {
        // Create isolated strong cyclic graph
        realm->begin_transaction();
        auto obj_c = Object::create<Any>(context, realm, cyclic_schema_c, AnyDict{ {"strongObjectRef", Any()} }, false);
        auto obj_d = Object::create<Any>(context, realm, cyclic_schema_d, AnyDict{ {"strongObjectRef", obj_c} }, false);
        obj_c.set_property_value(context, "strongObjectRef", Any(obj_d), true);
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_c));
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_d));

        // Breaking the strong cycle deletes both objects (see details in descriptor.hpp)
        realm->begin_transaction();
        obj_c.set_property_value(context, "strongObjectRef", Any(), true);
        realm->commit_transaction();
        REQUIRE(0 == TestHelper::get_count(realm, cyclic_schema_c));
        REQUIRE(0 == TestHelper::get_count(realm, cyclic_schema_d));

    }

    // (F -> (C <-> D)) -> (F -> C) = D is deleted
    SECTION("Breaking a strong cycle with outside links only delete leftover object") {
        // Create strong cyclic graph with outside link
        realm->begin_transaction();
        auto obj_c = Object::create<Any>(context, realm, cyclic_schema_c, AnyDict{ {"strongObjectRef", Any()} }, false);
        auto obj_d = Object::create<Any>(context, realm, cyclic_schema_d, AnyDict{ {"strongObjectRef", obj_c} }, false);
        obj_c.set_property_value(context, "strongObjectRef", Any(obj_d), true);
        Object::create<Any>(context, realm, island_parent_schema, AnyDict{ {"strongObjectRef", obj_c} }, false);
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_c));
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_d));
        REQUIRE(1 == TestHelper::get_count(realm, island_parent_schema));

        // Breaking the strong cycle only deletes the now single isolated object
        realm->begin_transaction();
        obj_c.set_property_value(context, "strongObjectRef", Any(), true);
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_c));
        REQUIRE(0 == TestHelper::get_count(realm, cyclic_schema_d));
        REQUIRE(1 == TestHelper::get_count(realm, island_parent_schema));
    }

    // (F -> (C <-> D)) -> (C <-> D) = No objects are deleted
    SECTION("Removing reference to Strong island of multiple classes do not remove the island") {
        // Create strong cyclic graph with outside link
        realm->begin_transaction();
        auto obj_c = Object::create<Any>(context, realm, cyclic_schema_c, AnyDict{ {"strongObjectRef", Any()} }, false);
        auto obj_d = Object::create<Any>(context, realm, cyclic_schema_d, AnyDict{ {"strongObjectRef", obj_c} }, false);
        obj_c.set_property_value(context, "strongObjectRef", Any(obj_d), true);
        auto obj_f = Object::create<Any>(context, realm, island_parent_schema, AnyDict{ {"objectRef", obj_c } }, false);
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_c));
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_d));
        REQUIRE(1 == TestHelper::get_count(realm, island_parent_schema));

        // Breaking the reference to the island does not delete any objects.
        realm->begin_transaction();
        obj_f.set_property_value(context, "strongObjectRef", Any(), true);
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_c));
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_d));
        REQUIRE(1 == TestHelper::get_count(realm, island_parent_schema));
    }

    // (E1 -> (E2 <-> E3)) -> (E2 <-> E3) = No objects are deleted
    SECTION("Removing reference to Strong island of a single class do not remove the island") {
        // Create strong cyclic graph with outside link consisting of only a single object type
        realm->begin_transaction();
        auto obj_e2 = Object::create<Any>(context, realm, cyclic_schema_e, AnyDict{ {"strongObjectRef", Any()} }, false);
        auto obj_e3 = Object::create<Any>(context, realm, cyclic_schema_e, AnyDict{ {"strongObjectRef", obj_e2} }, false);
        obj_e2.set_property_value(context, "strongObjectRef", Any(obj_e3), true);
        auto obj_e1 = Object::create<Any>(context, realm, cyclic_schema_e, AnyDict{ {"strongObjectRef", obj_e2 } }, false);
        realm->commit_transaction();
        REQUIRE(3 == TestHelper::get_count(realm, cyclic_schema_e));

        // Breaking the reference to the island does not delete any objects.
        realm->begin_transaction();
        obj_e1.set_property_value(context, "strongObjectRef", Any(), true);
        realm->commit_transaction();
        REQUIRE(3 == TestHelper::get_count(realm, cyclic_schema_e));
    }

}

TEST_CASE("Removal of children is immediate inside transactions.") {
    SharedRealm realm = TestHelper::get_realm();
    CppContext context(realm);
    auto child_schema = *realm->schema().find("Child");
    auto weak_parent_schema = *realm->schema().find("WeakParentA");
    auto strong_parent_schema = *realm->schema().find("StrongParentB");
    auto cyclic_schema_c = *realm->schema().find("CycleC");
    auto cyclic_schema_d = *realm->schema().find("CycleD");
    auto cyclic_schema_e = *realm->schema().find("SingleClassCycleE");

    // (wP -> Child) and (sP -> Child) -> (wP -> Child) = Child is deleted
    SECTION("DAG with mix of weak and strong links. Removing references causes immediate deletion.") {
        // Create mix of links to same object
        realm->begin_transaction();
        auto child = Object::create<Any>(context, realm, child_schema, AnyDict{{ "prop", "childA"s }}, false);
        auto weak_parent = Object::create<Any>(context, realm, weak_parent_schema, AnyDict{ {"weakObjectRef", child} }, false);
        auto strong_parent = Object::create<Any>(context, realm, strong_parent_schema, AnyDict{ {"strongObjectRef", child} }, false);
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, child_schema));

        // Removing Strong link should delete object
        realm->begin_transaction();
        strong_parent.set_property_value(context, "strongObjectRef", Any(), true);
        REQUIRE(0 == TestHelper::get_count(realm, child_schema));

        // Canceling transaction restores objects again
        realm->cancel_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, child_schema));
    }

    // Special case described in descriptor.hpp: (A <-> B) -> (A -> B) = Both objects are deleted.
    // See descriptor.hpp for details.
    SECTION("Breaking an island deletes objects immediately") {
        // Create isolated strong cyclic graph
        realm->begin_transaction();
        auto obj_c = Object::create<Any>(context, realm, cyclic_schema_c, AnyDict{ {"strongObjectRef", Any()} }, false);
        auto obj_d = Object::create<Any>(context, realm, cyclic_schema_d, AnyDict{ {"strongObjectRef", obj_c} }, false);
        obj_c.set_property_value(context, "strongObjectRef", Any(obj_d), true);
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_c));
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_d));

        // Breaking the island deletes both objects.
        realm->begin_transaction();
        obj_c.set_property_value(context, "strongObjectRef", Any(), true);
        REQUIRE(0 == TestHelper::get_count(realm, cyclic_schema_c));
        REQUIRE(0 == TestHelper::get_count(realm, cyclic_schema_d));

        // Canceling transaction restores objects again
        realm->cancel_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_c));
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_d));
    }
}

TEST_CASE("Objects created with no parents are not deleted") {
    SharedRealm realm = TestHelper::get_realm();
    CppContext context(realm);
    auto cyclic_schema_e = *realm->schema().find("SingleClassCycleE");

    SECTION("Object with a strong incoming reference is not deleted if no parent exist when it is created") {
        realm->begin_transaction();
        auto obj = Object::create<Any>(context, realm, cyclic_schema_e, AnyDict{ {"strongObjectRef", Any()} }, false);
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_e));
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_e));
    }

    SECTION("Cascading behaviour is only triggered when references are updated") {
        realm->begin_transaction();
        auto obj_e1 = Object::create<Any>(context, realm, cyclic_schema_e, AnyDict{ {"strongObjectRef", Any()} }, false);
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_e));
        auto obj_e2 = Object::create<Any>(context, realm, cyclic_schema_e, AnyDict{ {"strongObjectRef", Any(obj_e1)} }, false);
        REQUIRE(2 == TestHelper::get_count(realm, cyclic_schema_e));
        obj_e2.set_property_value(context, "strongObjectRef", Any(), true);
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_e));
        realm->commit_transaction();
        REQUIRE(1 == TestHelper::get_count(realm, cyclic_schema_e));
    }
}
