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

#ifndef REMOTE_MONGO_CLIENT_HPP
#define REMOTE_MONGO_CLIENT_HPP

#include "sync/app_service_client.hpp"
#include <string>

namespace realm {
class SyncUser;

namespace app {

class MongoDatabase;

/// A client responsible for communication with the Stitch API
class MongoClient {
public:
    ~MongoClient() = default;
    MongoClient(const MongoClient&) = default;
    MongoClient(MongoClient&&) = default;
    MongoClient& operator=(const MongoClient&) = default;
    MongoClient& operator=(MongoClient&&) = default;

    /// Gets a `RemoteMongoDatabase` instance for the given database name.
    /// @param name the name of the database to retrieve
    MongoDatabase operator[](const std::string& name);

    /// Gets a `RemoteMongoDatabase` instance for the given database name.
    /// @param name the name of the database to retrieve
    MongoDatabase db(const std::string& name);

private:
    friend class realm::SyncUser;

    MongoClient(std::shared_ptr<SyncUser> user, std::shared_ptr<AppServiceClient> service, std::string service_name)
        : m_user(std::move(user))
        , m_service(std::move(service))
        , m_service_name(std::move(service_name))
    {
    }

    std::shared_ptr<SyncUser> m_user;
    std::shared_ptr<AppServiceClient> m_service;
    std::string m_service_name;
};

} // namespace app
} // namespace realm

#endif /* remote_mongo_client_hpp */
