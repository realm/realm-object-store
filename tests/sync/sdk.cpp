
#include "catch2/catch.hpp"
#include "sdk.hpp"
#include "util/test_utils.hpp"
#include "util/test_file.hpp"
#include <realm/util/scope_exit.hpp>
#include "util/scheduler.hpp"
#include "binding_context.hpp"
#include <map>

using namespace realm::sdk;

struct Foo;

struct Bar : public Object<Bar> {
    REALM_PRIMARY_KEY(realm::ObjectId) oid = realm::ObjectId::gen();
    REALM(Foo*) foo;

    REALM_EXPORT(oid, foo);
};

struct Foo : public Object<Foo> {
    struct Baz : public EmbeddedObject<Baz>
    {
        REALM(realm::util::Optional<std::string>) opt_str_col;

        REALM_EXPORT(opt_str_col);
    };

    REALM_PRIMARY_KEY(int) int_col;
    REALM(float) float_col;
    REALM(double) double_col;
    REALM(bool) bool_col;
    REALM(std::chrono::time_point<std::chrono::system_clock>) date_col;
    REALM(std::string) str_col;
    REALM(realm::ObjectId) oid_col;

    REALM(std::vector<Bar*>) link_list_col;
    REALM(std::vector<Baz>) embedded_list_col;
    REALM(std::vector<int>) int_list_col;

    REALM(realm::util::Optional<int>) opt_int_col;
    REALM(realm::util::Optional<float>) opt_float_col;
    REALM(realm::util::Optional<double>) opt_double_col;
    REALM(realm::util::Optional<std::chrono::time_point<std::chrono::system_clock>>) opt_date_col;
    REALM(realm::util::Optional<std::string>) opt_str_col;
    REALM(realm::util::Optional<realm::ObjectId>) opt_oid_col;

    REALM_EXPORT(int_col, float_col, double_col, bool_col, date_col,
                 str_col, oid_col, link_list_col, embedded_list_col,
                 opt_int_col, opt_float_col, opt_double_col,
                 opt_date_col, opt_str_col, opt_oid_col);
};


// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

class TestScheduler: public realm::util::Scheduler {
public:
    ~TestScheduler() {}
    // Trigger a call to the registered notify callback on the scheduler's event loop.
    //
    // This function can be called from any thread.
    virtual void notify() {
        m_callback();
    }
    // Check if the caller is currently running on the scheduler's thread.
    //
    // This function can be called from any thread.
    virtual bool is_on_thread() const noexcept { return true; }

    // Checks if this scheduler instance wraps the same underlying instance.
    // This is up to the platforms to define, but if this method returns true,
    // caching may occur.
    virtual bool is_same_as(const Scheduler*) const noexcept {
        return true;
    }

    // Check if this scehduler actually can support notify(). Notify may be
    // either not implemented, not applicable to a scheduler type, or simply not
    // be possible currently (e.g. if the associated event loop is not actually
    // running).
    //
    // This function is not thread-safe.
    virtual bool can_deliver_notifications() const noexcept {
        return true;
    }

    // Set the callback function which will be called by notify().
    //
    // This function is not thread-safe.
    virtual void set_notify_callback(std::function<void()> cb) {
        m_callback = std::move(cb);
    };
private:
    std::function<void()> m_callback;
};

TEST_CASE("sdk") {
    const std::string realm_dir = realm::tmp_dir() + realm::ObjectId::gen().to_string();
    auto defer = realm::util::make_scope_exit([realm_dir]() noexcept {
        realm::util::try_remove_dir_recursive(realm_dir);
    });
    realm::util::make_dir(realm_dir);

    realm::Realm::Config config;
    config.path = realm_dir + "/default.realm";
    config.schema_version = 1;
    config.scheduler = static_cast<std::shared_ptr<realm::util::Scheduler>>(std::make_shared<TestScheduler>());
    auto realm = Realm(config);

    SECTION("add pre-baked object") {
        auto foo = Foo();
        foo.int_col = 42;
        foo.float_col = 42.42;
        foo.double_col = 84.84;
        foo.bool_col = true;
        foo.str_col = "foo";
        foo.int_list_col.push_back(1);
        foo.int_list_col.push_back(2);
        foo.int_list_col.push_back(3);
        auto now = std::chrono::system_clock::now();
        foo.date_col = now;

        auto bar0 = Bar();
        auto bar1 = Bar();

        bar0.foo = &foo;

        CHECK(bar0.foo == &foo);
        CHECK(!bar1.foo);

        foo.link_list_col.push_back(&bar0);
        foo.link_list_col.push_back(&bar1);

        foo.embedded_list_col.push_back(Foo::Baz());
        foo.opt_str_col = "wah";
        foo.embedded_list_col[0].opt_str_col = "hello world";
        CHECK(foo.embedded_list_col[0].opt_str_col == "hello world");
        CHECK(foo.opt_str_col == "wah");
        CHECK(bar0.foo == &foo);
        CHECK(!bar1.foo);

        CHECK(!foo.is_managed());
        CHECK(!foo.int_col.is_managed());
        CHECK(!foo.float_col.is_managed());
        CHECK(!foo.double_col.is_managed());
        CHECK(!foo.bool_col.is_managed());
        CHECK(!foo.str_col.is_managed());
        CHECK(!foo.int_list_col.is_managed());
        CHECK(!bar0.foo.is_managed());

        realm.write([&] {
            realm.add_object(foo);
        });

        CHECK(bar0.foo);
        CHECK(!bar1.foo);

        CHECK(foo.opt_str_col == "wah");
        CHECK(foo.is_managed());
        CHECK(foo.int_col.is_managed());
        CHECK(foo.float_col.is_managed());
        CHECK(foo.double_col.is_managed());
        CHECK(foo.bool_col.is_managed());
        CHECK(foo.str_col.is_managed());
        CHECK(foo.link_list_col.is_managed());
        CHECK(foo.link_list_col[0]->is_managed());
        CHECK(foo.link_list_col[0]->foo.is_managed());
        CHECK(foo.link_list_col[0]->foo);
        CHECK(foo.embedded_list_col[0].opt_str_col == "hello world");

        CHECK(bar0.is_managed());
        CHECK(bar0.foo.is_managed());
        CHECK(bar0.foo);

        CHECK(bar1.is_managed());
        CHECK(!bar1.foo);

        CHECK(!foo.link_list_col[1]->foo);
        CHECK(bar0.foo.is_managed());

        CHECK(bar0.foo->int_col == foo.int_col);
        CHECK(bar0.foo->float_col == foo.float_col);
        CHECK(bar0.foo->int_col == foo.int_col);

        CHECK(*bar0.foo == foo);
        CHECK(foo.link_list_col[0]->oid.is_managed());
        CHECK(foo.link_list_col[0]->oid == bar0.oid);
        CHECK(foo.link_list_col[1]->oid == bar1.oid);
        CHECK(foo.int_list_col == std::vector<int>{1,2,3});

//        CHECK(foo.bars[0].list_int_col.is_managed());

        CHECK(foo.int_col == 42);
        CHECK(foo.float_col == 42.42f);
        CHECK(foo.double_col == 84.84);
        CHECK(foo.bool_col == true);
        CHECK(foo.str_col == "foo");
        CHECK(foo.date_col == now);

        realm.write([&] {
            foo.int_col += 42;
        });

        auto foos = realm.get_objects<Foo>();

        REQUIRE(foos.size() == 1);

        auto result_foo = foos.get(0);
        CHECK(result_foo == foo);
        CHECK(result_foo.int_col == 84);
        CHECK(result_foo.float_col == 42.42f);
        CHECK(result_foo.double_col == 84.84);
        CHECK(result_foo.bool_col == true);
        CHECK(result_foo.str_col == "foo");
        CHECK(result_foo.date_col == now);
        CHECK(result_foo.embedded_list_col[0].opt_str_col == "hello world");
        CHECK(result_foo.opt_str_col == "wah");
        CHECK(result_foo.link_list_col[0]->oid == bar0.oid);
//        CHECK(foos.get(0).bars.get(0).foo->int_col == 42);
//        CHECK(foos.get(0).bars.get(0).foo->float_col == 42.42f);
//        CHECK(foos.get(0).bars.get(0).foo->double_col == 84.84);
//        CHECK(foos.get(0).bars.get(0).foo->bool_col == true);
//        CHECK(foos.get(0).bars.get(0).foo->str_col == "foo");
//        CHECK(foos.get(0).bars.get(0).foo->date_col == now);

//        CHECK(foos.get(0).bars.get(0) == foo.bars.get(0));
//        CHECK(foos.get(0).bars.get(0) == foo.bars.get(0));
//        CHECK(foos.get(0).bars[0].list_int_col[0] == 42);
//        CHECK(foos.get(0).bars[0].list_int_col[1] == 84);
//        CHECK(foos.get(0).bars[1].list_int_col[0] == 168);
//        CHECK(foos.get(0).bars[1].list_int_col[1] == 336);
    }

    SECTION("get foo by pk") {
        auto foo1 = Foo();
        foo1.int_col = 1;
        foo1.str_col = "foo";

        auto foo2 = Foo();
        foo2.int_col = 2;
        foo2.str_col = "bar";

        realm.write([&] {
            realm.add_object(foo1);
            realm.add_object(foo2);
        });

        auto looked_up_foo1 = realm.get_object<Foo>(1); // Foo's pk is int type
        auto looked_up_foo2 = realm.get_object<Foo>(2);

        REQUIRE(static_cast<bool>(looked_up_foo1));
        REQUIRE(static_cast<bool>(looked_up_foo2));

        REQUIRE(foo1.str_col == (*looked_up_foo1).str_col);
        REQUIRE(looked_up_foo1->str_col == "foo");
        REQUIRE(looked_up_foo2->str_col == "bar");
        REQUIRE(foo2.str_col == (*looked_up_foo2).str_col);

        bool processed = false;
        std::mutex mtx;
        std::unique_lock<std::mutex> lck(mtx);
        std::condition_variable cv;

        auto token = foo1.observe([&](auto&& change, auto err) {
            REQUIRE(!err);
            switch (change.property_changed) {
                case Foo::Property::str_col:
                    CHECK(change.old_value == "baz");
                    CHECK(change.new_value == "baz");
                    processed = true;
                    cv.notify_all();
                    break;
                default:
                    break;
            }
        });

        realm.write([&] {
            foo1.str_col = "baz";
        });
        cv.wait(lck);
        REQUIRE(processed);
    }
}
