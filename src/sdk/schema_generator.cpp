//
//  schema_generator.cpp
//  realm-object-store
//
//  Created by Jason Flax on 05/11/2020.
//

#include "schema_generator.hpp"

namespace realm::sdk::util {

std::vector<ObjectSchema> &get_schemas()
{
    static std::vector<ObjectSchema> schemas;
    return schemas;
}

}
