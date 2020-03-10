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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or utilied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef CORE_REMOTE_MONGO_DATABASE_HPP
#define CORE_REMOTE_MONGO_DATABASE_HPP

#include <string>
#include <json.hpp>
#include "core_remote_mongo_collection.hpp"

namespace realm {
namespace mongodb {

class CoreStitchServiceClient;

class CoreRemoteMongoDatabase {
    
public:
    //
    using Document = nlohmann::json;

    /**
    * The name of this database.
    */
    const std::string name;
    
    CoreRemoteMongoDatabase(std::string name, MongoRealmServiceClient service, CoreRemoteMongoClient client) :
    name(name),
    m_service(service),
    m_client(client) { };
    
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
    
    /**
    * Gets a collection.
    *
    * - Overload method for convenience
    * - parameter name: the name of the collection to return
    * - returns: the collection
    */
    CoreRemoteMongoCollection<Document> operator[](std::string collection_name);
    
private:
    MongoRealmServiceClient m_service;
    CoreRemoteMongoClient m_client;
    
};

} // namespace realm
} // namespace mongodb

#endif /* core_remote_mongo_database_h */
