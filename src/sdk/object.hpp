#ifndef realm_sdk_object_h
#define realm_sdk_object_h

#include "accessor.hpp"
#include "util.hpp"
#include "property.hpp"
#include "type_info.hpp"
#include "realm.hpp"

#include "../object.hpp"
#include "../collection_notifications.hpp"
#include "../object_store.hpp"
#include <queue>
#include <realm/mixed.hpp>
#include <realm/util/any.hpp>

namespace realm::sdk {

/**
 A change event for a given `Object`'s property.
 */
template <typename ChangeEnum>
struct PropertyChange {
    /// The property that has a changed (a named, integral property referring to an Object's property)
    ChangeEnum property_changed;
    /// The new value of the changed property.
    realm::Mixed new_value;
    /// The old value of the changed property.
    realm::util::Optional<realm::Mixed> old_value;
};

/**
 A notification token to be stored on observation.
 TODO: make fields private
 */
struct NotificationToken {
    realm::Object m_obj;
    realm::NotificationToken m_token;
};

/// MARK: ObjectBase
/// Base type for Objects and EmbeddedObjects.
/// Objects are their own accessors.
template <typename Impl>
struct ObjectBase : AccessorBase {
    virtual ~ObjectBase() {}

    void attach(SharedRealm r, const TableRef& table, const ObjKey& key) override
    {
        // if this is already managed, reattaching means this is being
        // referenced elsewhere
        AccessorBase::attach(r, table, key);
        std::apply([&](auto&&... properties) {
            (properties.attach(r, table, key), ...);
        }, static_cast<Impl&>(*this)._properties());
    }

    virtual void attach_and_write(SharedRealm r, TableRef table, ObjKey key)
    {
        attach(r, table, key);
        std::apply([&](auto&&... properties) {
            (properties.attach_and_write(r, table, key), ...);
        }, static_cast<Impl&>(*this)._properties());
    }

    void set_column_name(const std::string& name)
    {
        column_name = name;
    }

    bool is_managed() const
    {
        return AccessorBase::is_valid();
    }

    Realm realm() const;
protected:
    template <typename Change>
    NotificationToken observe(std::function<void(PropertyChange<Change>, std::exception_ptr)>&& callback);

    template <typename Property>
    void assign_name(Property& property, std::queue<std::string>& names)
    {
        auto name = util::trim_copy(names.front());
        names.pop();

        property.set_column_name(name);
    }

    template <typename ...Properties>
    void assign_names(std::string names, Properties&& ...ps)
    {
        std::queue<std::string> result;
        std::stringstream s_stream(names); //create string stream from the string
        while (s_stream.good()) {
            std::string substr;
            std::getline(s_stream, substr, ','); //get first string delimited by comma
            result.push(substr);
        }

        (assign_name(ps, result), ...);
    }
};

template <typename T>
Realm ObjectBase<T>::realm() const
{
    return Realm(AccessorBase::realm());
}

template <typename Impl>
template <typename Change>
NotificationToken ObjectBase<Impl>::observe(std::function<void(PropertyChange<Change>, std::exception_ptr)>&& callback) // Throws
{
    if (!is_managed()) {
        throw std::runtime_error("Only objects which are managed by a Realm support change notifications");
    }

    struct {
        std::function<void(PropertyChange<Change>, std::exception_ptr)> m_block;
        ObjectBase& m_obj;

        std::vector<realm::Property> property_names;
        std::vector<size_t> columns;
        std::vector<Mixed> old_values;
        bool deleted = false;

        void populate_properties(realm::CollectionChangeSet const& c) {
            if (!property_names.empty()) {
                return;
            }
            if (!c.deletions.empty()) {
                deleted = true;
                return;
            }
            if (c.columns.empty()) {
                return;
            }

            auto schema = *m_obj.m_realm->schema().find(util::demangle(typeid(Impl).name()));
            std::vector<realm::Property> properties;
            for (auto property : schema.persisted_properties) {
                if (c.columns.count(property.column_key.value)) {
                    properties.push_back(property);
                }
            }

            if (!properties.empty()) {
                property_names = properties;
            }
        }

        std::vector<Mixed> read_values(realm::CollectionChangeSet const& c) {
            if (c.empty()) {
                return std::vector<Mixed>();
            }
            populate_properties(c);
            if (property_names.empty()) {
                return std::vector<Mixed>();
            }

            std::vector<Mixed> values;
            for (auto& property : property_names) {
                realm::util::Optional<realm::util::Any> value;
                if (m_obj.get_obj().is_null(StringData(property.name))) { // add array check
                    values.push_back(Mixed());
                } else {
                    values.push_back(m_obj.get_obj().get_any(property.column_key));
                }
            }

            return values;
        }

        void before(realm::CollectionChangeSet const& c) {
            old_values = read_values(c);
        }

        void after(realm::CollectionChangeSet const& c) {
            auto new_values = read_values(c);
            //            if (deleted) {
            //                block(nil, nil, nil, nil, nil);
            //            }
            if (!new_values.empty()) {
                for (size_t i = 0; i < property_names.size(); i++) {
                    auto property = property_names[i];
                    auto t = m_obj;
                    auto change = Impl::_change_for_name(property.name);
                    auto old_value = old_values.size() > i ? old_values[i] : new_values[i];
                    auto new_value = new_values[i];
                    m_block(PropertyChange<Change> { change, old_value, new_value }, nullptr);
                }
            }

            property_names.clear();
            old_values.clear();
        }

        void error(std::exception_ptr err) {
            m_block(PropertyChange<Change>(), err);
        }
    } callback_wrapper { std::move(callback), *this };

    auto object = realm::Object(m_realm, util::demangle(typeid(Impl).name()), get_obj_key());
    auto token = object.add_notification_callback(std::move(callback_wrapper));
    return NotificationToken{std::move(object), std::move(token)};
}

/**
 `Object` is a base class for model objects representing data stored in Realms.

 Define your model classes by subclassing `Object` and adding properties to be managed.
 Any managed property should have its type wrapped in the `REALM` macro. Properties
 should then me REALM_EXPORT'd within the class body.

 ```cpp
 // dog.hpp
 struct Dog : Object<Dog> {
    REALM(std::string) name;
    REALM(bool)      adopted;

    REALM_EXPORT(name, adopted);
 };
 ```

 ### Supported property types

 - `std::string`
 -  `int`, `float`, and `double`
 - `bool`
 - `std::chrono::time_point`
 - `enum class`
 - `realm::ObjectId`
 - `X*` where  `X` is an `Object` subclass, creating a link
 - `realm::util::Optional<X>` where `X` is a supported
 - `EmbeddedObject`
 - `std::vector<X*>`, where `X` is an `Object` subclass, to model many-to-many relationships.
*/
template <typename T>
struct Object : ObjectBase<T>
{
    using base = ObjectBase<T>;
    using self = Object<T>;
    using derived_t = T;
    using base::base;
protected:
    friend T;
    using base::observe;
};

/**
 MARK: Link
 A link is an auto-updating link to another Realm Object.

 Links are declared as a class member as pointers to
 the actual Object.

 class Foo;
 class Bar : Object<Bar> {
    REALM(Foo*) foo_link;
 };
 */
template <typename T>
struct Property<T, typename std::enable_if_t<std::is_pointer_v<T>>>
: private PropertyBase
{
    using removed_pointer_t = std::remove_pointer_t<T>;
    using base = PropertyBase;
    using base::base;
    using self = Object<T>;
    using value_type = T;

    ~Property()
    {
        m_value.reset();
    }

    Property(const Property& o)
    {
        *this = o;
    }

    Property& operator=(const Property& o)
    {
        Property::~Property();
        if (o.m_value) {
            new (&m_value) std::unique_ptr<removed_pointer_t>(new removed_pointer_t(*o.m_value));
        }
        m_unmanaged_value = o.m_unmanaged_value;
        m_accessor = o.m_accessor;
        return *this;
    }

    Property(Property&& o)
    {
        *this = std::move(o);
    }

    Property& operator=(Property&& o)
    {
        Property::~Property();
        if (o.m_value) {
            new (&m_value) std::unique_ptr<removed_pointer_t>(std::move(o.m_value));
        }
        m_unmanaged_value = o.m_unmanaged_value;
        m_accessor = o.m_accessor;
        return *this;
    }

    void attach(SharedRealm r, TableRef table, ObjKey key) override
    {
        PropertyBase::attach(r, table, key);
        if (m_accessor.as<T>().get() != null_key) {
            // if we are being attached to a nonnull link,
            // hydrate our facade with an attached object
            auto& accessor = m_accessor.as<T>();
            m_value = std::make_unique<removed_pointer_t>(std::move(accessor.to_value_type(accessor.get())));
        }
    }

    void attach_and_write(SharedRealm r, TableRef table, ObjKey key) override
    {
        attach(r, table, key);
        // if the linked object actually exists,
        // m_unmanaged_value will be set as part of
        // the attach process
        if (m_unmanaged_value) {
            auto& accessor = m_accessor.as<T>();
            accessor.set(*m_unmanaged_value);
            if (m_accessor.as<T>().get() != null_key) {
                m_value = std::make_unique<removed_pointer_t>(std::move(accessor.to_value_type(accessor.get())));
            }
        }
    }

    ObjKey get_key() const
    {
        return m_accessor.as<T>().get();
    }

    auto operator->() {
        return value();
    }

    removed_pointer_t& operator*() const {
        return *value();
    }

    operator bool() const {
        return value();
    }

    T value() const
    {
        if (!is_managed()) {
            return m_unmanaged_value;
        }

        return m_value.get();
    }

    T value()
    {
        if (!is_managed()) {
            return m_unmanaged_value;
        }

        // always hydrate our value with the latest linked object
        auto accessor = m_accessor.as<T>();
        m_value = std::make_unique<removed_pointer_t>(std::move(accessor.to_value_type(accessor.get())));
        return m_value.get();
    }

    Property& operator=(const T& other) {// Throws
        if (!is_managed()) {
            m_unmanaged_value = other;
        } else {
            // set links
            auto accessor = m_accessor.as<T>();
            m_value = std::make_unique<removed_pointer_t>(*other);
            accessor.set(*m_value);
        }
        return *this;
    }

    bool is_managed() const
    {
        return m_accessor.is_valid();
    }
private:
    std::unique_ptr<removed_pointer_t> m_value;
    T m_unmanaged_value;

    template <typename Impl>
    friend struct ObjectBase;
};

// MARK: Link Accessor
template <typename T>
struct Accessor<T, typename std::enable_if_t<std::is_pointer_v<T>>> : public AccessorBase {
    using type_info = TypeInfo<T>;
    using base = AccessorBase;
    using base::base;
    using removed_pointer_t = std::remove_pointer_t<T>;

    Accessor(const AccessorBase& o) : base(o)
    {
    }

    typename type_info::realm_type get()
    {
        return base::get_obj().get_linked_object(base::get_column_key()).get_key();
    }
    typename type_info::const_realm_type get() const
    {
        return base::get_obj().get_linked_object(base::get_column_key()).get_key();
    }

    void create_object(removed_pointer_t& value)
    {
        auto name = util::demangle(typeid(removed_pointer_t).name());
        auto table = ObjectStore::table_for_object_type(realm()->read_group(), name);
        Obj obj;
        if constexpr(removed_pointer_t::has_primary_key::value) {
            obj = table->create_object_with_primary_key(value.primary_key_value());
        } else {
            obj = table->create_object();
        }
        value.attach_and_write(realm(), table, obj.get_key());
    }

    void set(removed_pointer_t& value)
    {
        if (value.get_obj_key() == null_key) {
            // if a link is being set via the accessor, it means the
            // unmanaged value points to an unmanaged object. so
            // add the unmanaged object to the realm
            create_object(value);
        } else {
            // else, set the link to the ObjKey
            base::get_obj().template set<ObjKey>(base::get_column_key(), value.get_obj_key());
        }
    }

    removed_pointer_t to_value_type(const typename type_info::const_realm_type& realm_value) const
    {
        auto name = util::demangle(typeid(removed_pointer_t).name());
        auto table = ObjectStore::table_for_object_type(realm()->read_group(), name);
        auto value = removed_pointer_t();
        value.attach(realm(), table, realm_value);
        return value;
    }

    typename type_info::realm_type to_realm_type(removed_pointer_t& value)
    {
        if (value.get_obj_key() == null_key) {
            // if an unmanaged link is being converted to its core type,
            // the object exists in an unmanaged state. so preemptively
            // add the unmanaged object to the realm
            create_object(value);
        }
        return value.get_obj_key();
    }
};

/**
 MARK: Embedded Object
 `EmbeddedObject` is a base class used to define embedded Realm model objects.

 Embedded objects work similarly to normal objects, but are owned by a single
 parent Object (which itself may be embedded). Unlike normal top-level objects,
 embedded objects cannot be directly created in or added to a Realm. Instead,
 they can only be created as part of a parent object, or by assigning an
 unmanaged object to a parent object's property. Embedded objects are
 automatically deleted when the parent object is deleted or when the parent is
 modified to no longer point at the embedded object, either by reassigning an
 Object property or by removing the embedded object from the List containing it.

 Embedded objects can only ever have a single parent object which links to
 them, and attempting to link to an existing managed embedded object will throw
 an exception.

 The property types supported on `EmbeddedObject` are the same as for `Object`,
 except for that embedded objects cannot link to top-level objects, so `Object`
 and `std::vector<Object*>` properties are not supported (`EmbeddedObject` and
 `std::vector<EmbeddedObject>` *are*).

 Embedded objects cannot have primary keys or indexed properties.

 ```cpp
 struct Dog: EmbeddedObject<Dog> {
    REALM(std::string) name;
    REALM(bool) adopted;

    REALM_EXPORT(name, adopted);
 }
 struct Owner: Object<Owner> {
    REALM(std::string) name;
    REALM(std::vector<Dog>) dogs;
    REALM_EXPORT(name, dogs);
 }
 ```
 */
template <typename T>
struct Property<T, util::type_traits::embedded_object>
: public ObjectBase<T>
{
    using base = ObjectBase<T>;
    using self = Object<T>;
    using derived_t = T;
    using base::base;
    using AccessorBase::realm;

    void attach_and_write(SharedRealm r, TableRef table, ObjKey key) override
    {
        base::attach(r, table, key);
        if (AccessorBase::column_name.empty()) {
            // if there is no column name, this is either in a list
            // or a top level object
            base::attach_and_write(realm(), table, key);
        } else {
            auto obj = AccessorBase::get_obj().create_and_set_linked_object(AccessorBase::get_column_key());
            base::attach_and_write(realm(), obj.get_table(), obj.get_key());
        }
    }

    operator T() const
    {
        return static_cast<const T&>(*this);
    }

    friend struct PropertyFriend;
    friend base;
};

/// @see `Property<T, util::type_traits::embedded_object>`
template <typename T>
using EmbeddedObject = Property<T, util::type_traits::embedded_object>;

// MARK: EmbeddedObjectAccessor
template <typename T>
struct Accessor<T, typename std::enable_if_t<util::is_embedded_v<T>>> : public AccessorBase
{
    using type_info = TypeInfo<T>;
    using base = AccessorBase;
    using base::base;
    using decayed_t = std::decay_t<T>;

    Accessor(const AccessorBase& o) : base(o)
    {
    }

    typename type_info::realm_type get()
    {
        return base::get_obj().template get<ObjKey>(base::get_column_key());
    }

    typename type_info::const_realm_type get() const
    {
        return base::get_obj().template get<ObjKey>(base::get_column_key());
    }

    void create_object(EmbeddedObject<decayed_t>& value)
    {
        auto obj = base::get_obj().create_and_set_linked_object(base::get_column_key());
        value.attach_and_write(realm(), obj.get_table(), obj.get_key());
    }

    void set(EmbeddedObject<decayed_t>& value)
    {
        if (!value.is_managed()) {
            // if we are setting an embedded object to its enclosing object
            // and it is unmanaged, create it and set it to the Obj.
            // then attach and write the embedded object's properties
            auto obj = base::get_obj().create_and_set_linked_object(base::get_column_key());
            value.attach_and_write(realm(), std::move(obj));
        } else {
            // else set the ObjKey to the enclosing Obj's column
            base::get_obj().template set<ObjKey>(base::get_column_key(), value.get_key());
        }
    }

    decayed_t to_value_type(const typename type_info::const_realm_type& realm_value) const
    {
        auto name = util::demangle(typeid(decayed_t).name());
        auto table = ObjectStore::table_for_object_type(realm()->read_group(), name);
        auto value = T();
        value.attach(realm(), table, realm_value);
        return value;
    }

    typename type_info::realm_type to_realm_type(T& value)
    {
        if (static_cast<AccessorBase&>(value).get_obj_key() == null_key) {
            create_object(value);
        }
        return static_cast<AccessorBase&>(value).get_obj_key();
    }
};

template <typename T>
static bool operator==(const Object<T>& lhs, const Object<T>& rhs)
{
    if (lhs.is_managed() || rhs.is_managed()) {
        // if either object is a link, we do not want
        // to run a deep equality check as this will
        // result in a ref cycle
        return lhs.get_obj_key() == rhs.get_obj_key();
    }

    return &lhs == &rhs;
}

} // namespace realm::sdk

#endif /* realm_sdk_object_h */
