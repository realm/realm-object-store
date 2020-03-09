//
//  core_remote_mongo_tests.cpp
//  realm-object-store
//
//  Created by INSERT HEADER on 09/03/2020.
//

#include "catch2/catch.hpp"
#include "core_remote_mongo_client.hpp"

#ifndef ENABLE_MONGO_CLIENT_TESTS
#define ENABLE_MONGO_CLIENT_TESTS 1
#endif


using namespace realm;
using namespace realm::mongodb;

#if ENABLE_MONGO_CLIENT_TESTS

TEST_CASE("test stubs") {

    auto client = CoreRemoteMongoClient();
    auto db = client.db("test-db");
    auto collection = db.collection("sample-collection");
    auto many_results = collection.find({{"name" , "John"}}, {});
}

#endif
