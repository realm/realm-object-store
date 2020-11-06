#ifndef realm_sdk_arithmetic_hpp
#define realm_sdk_arithmetic_hpp

#include "primitive.hpp"

namespace realm::sdk {

template <typename ArithmeticT>
struct ArithmeticBase
: private PrimitiveBase<Property<ArithmeticT>, ArithmeticT>
{
    using base = PrimitiveBase<Property<ArithmeticT>, ArithmeticT>;
    using typename base::value_type;
    using base::operator=;
    using base::is_managed;
    using base::value;

    friend base;
    template <typename Impl>
    friend struct ObjectBase;

    ArithmeticBase& operator+=(ArithmeticT rhs) {
        *this = base::value() + rhs;
        return *this;
    }
};

template <typename E>
struct Property<int, E>
: public ArithmeticBase<int>
{
    using base = ArithmeticBase<int>;
    using typename base::value_type;
    using base::operator=;
    using base::is_managed;
    using base::value;
    static constexpr bool is_primary_key = std::is_same_v<E, util::type_traits::primary_key>;
    friend base;
    template <typename Impl>
    friend struct ObjectBase;
};

template <>
struct Property<float>
: private PrimitiveBase<Property<float>, float>
{
    using base = PrimitiveBase<Property<float>, float>;
    using base::operator=;
    using typename base::value_type;
    using base::value;
    using base::is_managed;

    friend base;
    template <typename Impl>
    friend struct ObjectBase;
};

// MARK: Double

template <>
struct Property<double>
: private PrimitiveBase<Property<double>, double>
{
    using base = PrimitiveBase<Property<double>, double>;
    using base::operator=;
    using typename base::value_type;
    using base::value;
    using base::is_managed;

    friend base;
    template <typename Impl>
    friend struct ObjectBase;
};

// MARK: Bool

template <>
struct Property<bool>
: private PrimitiveBase<Property<bool>, bool>
{
    using base =  PrimitiveBase<Property<bool>, bool>;
    using typename base::value_type;
    using base::operator=;
    using base::value;
    using base::is_managed;

    template <typename Impl>
    friend struct ObjectBase;
    friend base;
};

}

#endif /* int_hpp */
