#include "string.hpp"

namespace realm::sdk {

template<>
String& String::operator=(const char* t)  // Throws
{
    return base::operator=(std::string(t));
}

template <>
bool Property<std::string>::empty() const
{
    return base::value().empty();
}

template <>
bool Property<std::string, util::type_traits::primary_key>::empty() const
{
    return base::value().empty();
}

template<>
size_t String::size() const
{
    return base::value().size();
}

}
