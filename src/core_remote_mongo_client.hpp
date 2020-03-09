//
//  core_remote_mongo_client.hpp
//  realm-object-store
//
//  CHANGE HEADER on 09/03/2020.
//

#ifndef CORE_REMOTE_MONGO_CLIENT_HPP
#define CORE_REMOTE_MONGO_CLIENT_HPP

#include <string>
//#include "core_remote_mongo_database.hpp"

namespace realm {
namespace mongodb {

class CoreRemoteMongoDatabase;

class CoreStitchServiceClient { };

class CoreRemoteMongoClient {
public:
    // TODO: is CoreStitchServiceClient needed?
    CoreRemoteMongoClient() { }
  
    CoreRemoteMongoDatabase db(std::string name);
};

} // namespace mongodb
} // namespace realm

#endif /* core_remote_mongo_client_hpp */
