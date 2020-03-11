//
//  core_remote_mongo_tests.cpp
//  realm-object-store
//
//  Created by INSERT HEADER on 09/03/2020.
//

#include "catch2/catch.hpp"
#include "core_remote_mongo_client.hpp"
#include "sync/app.hpp"

#ifndef ENABLE_MONGO_CLIENT_TESTS
#define ENABLE_MONGO_CLIENT_TESTS 1
#endif


using namespace realm;
using namespace realm::app;

#if ENABLE_MONGO_CLIENT_TESTS

TEST_CASE("test stubs") {
    
//    App app({ });
//    auto client = app.service_client("test-service");

//    client.db()
//    auto db = client.db("test-db");
//
//    AuthRequestClient auth_request_client;
//    ServiceRoutes service_routes("test-app-id");
//
//    auto core_service_client = CoreStitchServiceClient(auth_request_client, service_routes, { });
//    auto core_service = CoreRemoteMongoClientFactory::shared().client(core_service_client);
//    auto client = CoreRemoteMongoClient(core_service);
//
//    auto db = client.db("test-db");
//    auto db = client["test-db"];
//
//    auto collection = db.collection("sample-collection");
//    auto collection = db["sample-collection"];
//
//    auto many_results = collection.find({{"name" , "John"}}, { });
}

#endif
