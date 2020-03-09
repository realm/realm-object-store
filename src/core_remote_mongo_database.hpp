//
//  core_remote_mongo_database.h
//  realm-object-store
//
//  REPLACE HEADERS
//

#ifndef CORE_REMOTE_MONGO_DATABASE_HPP
#define CORE_REMOTE_MONGO_DATABASE_HPP

#include <string>
#include <json.hpp>
#include "core_remote_mongo_collection.hpp"
//#include "core_remote_mongo_client.hpp"

namespace realm {
namespace mongodb {

class CoreStitchServiceClient;

class CoreRemoteMongoDatabase {
    
public:
    
    using Document = nlohmann::json;

    /**
    * The name of this database.
    */
    const std::string name;
    
    CoreRemoteMongoDatabase(std::string name, CoreStitchServiceClient service) :
    name(name),
    m_service(service) { };
    
    /**
    * Gets a collection with a specific default document type.
    *
    * - parameter name: the name of the collection to return
    * - parameter withCollectionType: the default class to cast any documents returned from the database into.
    * - returns: the collection
    */
    template<typename CollectionType>
    CoreRemoteMongoCollection<CollectionType> collection(std::string collection_name);
    
    /**
    * Gets a collection.
    *
    * - parameter name: the name of the collection to return
    * - returns: the collection
    */
    CoreRemoteMongoCollection<Document> collection(std::string collection_name);

    
private:
    CoreStitchServiceClient m_service;
    //CoreRemoteMongoClient m_client;
    
    
};

} // namespace realm
} // namespace mongodb

#endif /* core_remote_mongo_database_h */
