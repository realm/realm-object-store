#ifndef realm_sdk_optional_hpp
#define realm_sdk_optional_hpp

#include "accessor.hpp"
#include "type_info.hpp"
#include "primitive.hpp"

#include <realm/util/optional.hpp>

template <typename T>
using optional = realm::util::Optional<T>;

namespace realm::sdk {

template <typename T>
struct Accessor<optional<T>> : public AccessorBase {
    using base = AccessorBase;
    using base::base;
    using type_info = TypeInfo<optional<T>>;
    using non_optional_accessor = Accessor<T>;

    typename type_info::realm_type get()
    {
        if (base::get_obj().is_null(base::get_column_key())) {
            return realm::null();
        }
        return non_optional_accessor(static_cast<base&>(*this)).get();
    }

    typename type_info::const_realm_type get() const
    {
        if (base::get_obj().is_null(base::get_column_key())) {
            return realm::null();
        }
        return non_optional_accessor(static_cast<const base&>(*this)).get();
    }

    void set(const optional<T>& value)
    {
        if (!value) {
            base::get_obj().set_null(base::get_column_key());
        } else {
            non_optional_accessor(static_cast<base&>(*this)).set(*value);
        }
    }

    optional<T> to_value_type(const typename type_info::const_realm_type& realm_value) const
    {
        if (!realm_value) {
            return realm::util::none;
        }
        auto value = non_optional_accessor(static_cast<const base&>(*this)).to_value_type(*realm_value);
        return optional<T>(std::move(value));
    }

    typename type_info::realm_type to_realm_type(const T& value) const
    {
        return non_optional_accessor(static_cast<const base&>(*this)).to_realm_type(value);
    }
};

template <typename T>
struct OptionalBase : public PrimitiveBase<Property<optional<T>>, optional<T>>
{
    using base = PrimitiveBase<Property<optional<T>>, optional<T>>;
    using base::base;
    using base::operator=;
    operator T() const {
        return *base::value();
    }

    operator T&() {
        return *base::value();
    }

    auto operator->() const {
        return base::value();
    }

    auto operator*() const {
        return *base::value();
    }

    explicit operator bool() const {
        return base::value().operator bool();
    }

private:
    template <typename Impl>
    friend struct ObjectBase;
};

template <typename T>
struct Property<optional<T>> : OptionalBase<T>
{
    using base = OptionalBase<T>;
    using base::base;
};

template <>
struct Property<optional<std::string>>
: OptionalBase<std::string>
{
    using base = OptionalBase<std::string>;
    using base::base;
    using base::operator=;

    Property& operator=(const char* value)
    {
        base::operator=(std::string(value));
        return *this;
    }
};

template <typename T, typename E>
static inline bool operator==(const Property<optional<T>, E>& lhs, const std::decay_t<T>& rhs)
{
    return lhs.value() == rhs;
}
template <typename T, typename E>
static inline bool operator!=(const Property<optional<T>, E>& lhs, const std::decay_t<T>& rhs)
{
    return !(lhs == rhs);
}

}
#endif /* optional_hpp */
