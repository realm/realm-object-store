#ifndef primitive_hpp
#define primitive_hpp

#include "accessor.hpp"
#include "type_info.hpp"
#include "property.hpp"

namespace realm::sdk {

// MARK: Primitive Accessor
template <typename T, typename E>
struct Accessor : public AccessorBase {
    using base = AccessorBase;
    using base::base;
    using type_info = TypeInfo<T>;

    Accessor(const AccessorBase& o) : base(o)
    {
    }

    typename type_info::realm_type get()
    {
        return base::get_obj().template get<typename type_info::realm_type>(base::get_column_key());
    }

    typename type_info::const_realm_type get() const
    {
        return base::get_obj().template get<typename type_info::const_realm_type>(base::get_column_key());
    }

    void set(const T& value)
    {
        base::get_obj().template set<typename type_info::realm_type>(base::get_column_key(),
                                                                     static_cast<typename type_info::realm_type>(value));
    }

    T to_value_type(const typename type_info::const_realm_type& realm_value) const
    {
        return static_cast<T>(realm_value);
    }

    typename type_info::realm_type to_realm_type(const T& value) const
    {
        return static_cast<typename type_info::realm_type>(value);
    }
};

template <typename T>
using PrimitiveAccessor = Accessor<T>;

/**
 MARK: PrimitiveBase
 Base `Property` specialization for any primitive type. "Primitive"
 is defined here as any non collection or polymorphic type, such as
 int, long, ObjectId, time_point, std::string, etc..
 */
template <typename Impl, typename T>
struct PrimitiveBase : PropertyBase
{
    using value_type = T;
    using impl = Impl;
    using PropertyBase::PropertyBase;

    PrimitiveBase(const T& default_init_value)
    : m_value(default_init_value)
    {
    }

    void attach_and_write(SharedRealm r, TableRef table, ObjKey key) override
    {
        PropertyBase::attach(r, table, key);
        static_cast<Accessor<T>&>(m_accessor).set(m_value);
    }

    operator T() const
    {
        return value();
    }

    bool is_managed() const
    {
        return m_accessor.is_valid();
    }

    T& value()
    {
        if (!is_managed()) {
            return m_value;
        }

        auto& accessor = m_accessor.as<T>();
        m_value = accessor.to_value_type(accessor.get());
        return m_value;
    }

    T value() const
    {
        if (!is_managed()) {
            return m_value;
        }

        auto& accessor = m_accessor.as<T>();
        return accessor.to_value_type(accessor.get());
    }

    Impl& operator=(const T& t)  // Throws
    {
        m_value = t;

        if (is_managed()) {
            m_accessor.as<T>().set(t);
        }
        return static_cast<Impl&>(*this);
    }

    typename std::conditional_t<util::is_complete<T>::value, T, T&> m_value;
    friend struct PropertyRealm;
    template <typename>
    friend constexpr auto unwrap_data_type();
    friend struct PropertyFriend;
    friend struct PropertyCollectionBase;
private:
    const Impl& as_derived() const
    {
        return static_cast<const Impl&>(*this);
    }
};

// MARK: `enum class` specialization
template <typename T>
struct Property<T, typename std::enable_if_t<std::is_enum_v<T>>>
: private PrimitiveBase<Property<T, typename std::enable_if_t<std::is_enum_v<T>>>, T>
{
    using base = PrimitiveBase<Property<T, typename std::enable_if_t<std::is_enum_v<T>>>, T>;
    using type_trait = util::type_traits::enum_type;
    using base::base;
    using typename base::value_type;
    using base::operator=;
    using base::value;
    using base::is_managed;

    friend struct PropertyRealm;
    friend struct PropertyFriend;

    operator T() const
    {
        return value();
    }

    friend base;
    template <typename Impl>
    friend struct ObjectBase;
};

}

#endif /* primitive_hpp */
