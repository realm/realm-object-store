
#ifndef realm_sdk_string_hpp
#define realm_sdk_string_hpp

#include "primitive.hpp"
#include <string>
#include "optional.hpp"

namespace realm::sdk {

// MARK: `std::string` Property specialization
template <typename E>
struct Property<std::string, E>
: public PrimitiveBase<Property<std::string>, std::string>
{
    using base = PrimitiveBase<Property<std::string>, std::string>;
    using base::operator=;
    using base::is_managed;
    using base::base;
    using base::value;
    static constexpr bool is_primary_key = std::is_same_v<E, util::type_traits::primary_key>;

    operator std::string() const
    {
        return base::value();
    }

    operator std::string&()
    {
        return base::value();
    }

    Property& operator=(const char* t);

    /**
     Checks if the string has no characters, i.e. whether begin() == end().
     @return true if the string is empty, false otherwise
     */
    bool empty() const;

    /**
     Returns the number of CharT elements in the string, i.e. std::distance(begin(), end()).
     @return The number of CharT elements in the string.
     */
    size_t size() const;
private:
    friend base;
    friend struct OptionalBase<std::string>;
    template <typename Impl>
    friend struct ObjectBase;
};

template struct Property<std::string>;
template struct Property<std::string, util::type_traits::primary_key>;

using String = Property<std::string>;

static inline std::string& operator +=(std::string& lhs, const Property<std::string>& rhs)
{
    return lhs += rhs.value();
}

static inline std::string operator+(const std::string& lhs, const Property<std::string>& rhs)
{
    return lhs + rhs.value();
}

static inline std::string operator+(const Property<std::string>& lhs, const std::string& rhs)
{
    return lhs.value() + rhs;
}

static inline std::string operator+(const Property<std::string>& lhs, const Property<std::string>& rhs)
{
    return lhs.value() + rhs.value();
}

static inline bool operator==(const Property<std::string>& lhs, const char* rhs)
{
    return lhs.value() == rhs;
}
static inline bool operator!=(const Property<std::string>& lhs, const char* rhs)
{
    return !(lhs == rhs);
}

template <class CharT, class Traits, class Allocator>
std::basic_ostream<CharT, Traits>&
    operator<<(std::basic_ostream<CharT, Traits>& os,
               const Property<std::string>& str)
{
    return os << str.value();
}

}

#endif /* string_hpp */
