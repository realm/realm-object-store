#ifndef realm_sdk_schema_generator_hpp
#define realm_sdk_schema_generator_hpp

#include "../object_schema.hpp"
#include "util.hpp"
#include "property.hpp"
#include <queue>
#include "type_info.hpp"

namespace realm::sdk::util {

/**
 Get runtime generated schemas from the REALM_EXPORT
 macro.
 */
std::vector<ObjectSchema> &get_schemas();
/**
 Add a schema to the runtime generated schemas.
 */
void add_schema(const ObjectSchema& schema);

namespace {
static inline std::string demangle_optional(const std::string& name) {
    auto demangled = realm::sdk::util::demangle(name.data());

    auto first = demangled.find_first_of('<') + 1;
    auto last = demangled.find_last_of('>');
    auto with_optional_removed = demangled.substr(first,
                                                  last - first);
    return with_optional_removed;
}

static inline std::string demangle_incomplete_property_type(const std::string& name) {
    // this value is the length of "realm::sdk::Property<",
    // which will prefix our object type in all cases
    static auto realm_object_prefix_length = 19;
    auto demangled = realm::sdk::util::demangle(name.data());

    // find the closing bracket of our templated Property
    auto last_bracket_pos = demangled.find_last_of('>');
    auto with_object_removed = demangled.substr(realm_object_prefix_length,
                                                last_bracket_pos - realm_object_prefix_length);
    return with_object_removed;
}
}

template <typename EnclosingObject, typename Tuple>
struct SchemaGenerator {
    template <typename T, typename E = void>
    struct remove_property;
    template <typename T>
    struct remove_property<T> { using type = T; };
    template <typename T, typename E>
    struct remove_property<Property<T, E>> {
        using type = T;
    };

    template <typename Property>
    void property_from_arg(std::queue<std::string>& names) {
        auto name = util::trim_copy(names.front());
        names.pop();

        using type_info = TypeInfo<typename remove_property<std::decay_t<Property>>::type>;
        bool is_pk = is_primary_key<std::decay_t<Property>>::value;
        auto property = realm::Property(name,
                                        type_info::property_type,
                                        is_pk);
        if (is_pk) {
            schema.primary_key = name;
        }
        if constexpr(std::is_same_v<typename type_info::realm_type, ObjKey>) {
            property.object_type = util::demangle(typeid(std::remove_pointer_t<typename type_info::value_type>).name());
        } else if constexpr(std::is_base_of_v<LstBase, typename type_info::realm_type>) {
            if constexpr(std::is_same_v<typename type_info::value_type_type_info::realm_type, ObjKey>) {
                property.object_type = util::demangle(typeid(std::remove_pointer_t<typename type_info::value_type_type_info::value_type>).name());
            }
        }

        schema.persisted_properties.push_back(property);
    }

    template <typename ...Properties>
    void property_from_arg(std::tuple<Properties...>, std::queue<std::string>& names)
    {
        (property_from_arg<Properties>(names), ...);
    }

    SchemaGenerator(const std::string& names) {
        schema = ObjectSchema(util::demangle(typeid(typename EnclosingObject::derived_t).name()),
                              util::is_embedded_v<typename EnclosingObject::derived_t>,
                              {});
        std::queue<std::string> result;
        std::stringstream s_stream(names); //create string stream from the string
        while (s_stream.good()) {
            std::string substr;
            std::getline(s_stream, substr, ','); //get first string delimited by comma
            result.push(substr);
        }

        property_from_arg(Tuple(), result);
        get_schemas().push_back(schema);
    }

    ObjectSchema schema;
};

}

#endif /* schema_generator_hpp */
