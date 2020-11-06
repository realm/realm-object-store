#ifndef realm_sdk_object_id_hpp
#define realm_sdk_object_id_hpp

#include "primitive.hpp"
#include "util.hpp"

#include <realm/object_id.hpp>

namespace realm::sdk {

// MARK: `realm::ObjectId` specialization
template <typename E>
struct Property<realm::ObjectId, E>
: private PrimitiveBase<Property<ObjectId>, ObjectId>
{
    using base = PrimitiveBase<Property<ObjectId>, ObjectId>;
    using base::base;
    using typename base::value_type;
    using base::operator=;
    using base::value;
    using base::is_managed;
    static constexpr bool is_primary_key = std::is_same_v<E, util::type_traits::primary_key>;

    template <typename Impl>
    friend struct ObjectBase;
    friend base;
};

}

#endif /* object_id_hpp */
