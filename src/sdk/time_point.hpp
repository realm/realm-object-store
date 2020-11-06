////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020 Realm Inc.
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

#ifndef time_point_hpp
#define time_point_hpp

#include "primitive.hpp"

namespace realm::sdk {

// MARK: `std::chrono::time_point` specialization
template <typename C, typename D>
struct Property<std::chrono::time_point<C, D>>
: private PrimitiveBase<Property<std::chrono::time_point<C, D>>, std::chrono::time_point<C, D>>
{
    using base = PrimitiveBase<Property<std::chrono::time_point<C, D>>, std::chrono::time_point<C, D>>;
    using typename base::value_type;
    using base::operator=;
    using base::value;
    using base::is_managed;

    template <typename Impl>
    friend struct ObjectBase;
    friend base;
};


}

#endif /* time_point_hpp */
