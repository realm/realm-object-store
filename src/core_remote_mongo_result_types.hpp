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

#ifndef CORE_REMOTE_MONGO_RESULT_TYPES_HPP
#define CORE_REMOTE_MONGO_RESULT_TYPES_HPP

#include <json.hpp>

namespace realm {
namespace mongodb {

// MARK: - Coding keys

const static std::string inserted_id_key = "insertedId";
const static std::string inserted_ids_key = "insertedIds";
const static std::string deleted_count_key = "deletedCount";
const static std::string matched_count_key = "matchedCount";
const static std::string modified_count_key = "modifiedCount";
const static std::string upserted_id_key = "upsertedId";

// TODO: Place using statement here for now until types exists
using BSONValue = std::string;
using Document = nlohmann::json;

// MARK: - Result types

struct RemoteFindOptions {
    /// The maximum number of documents to return.
    long int limit;

    /// Limits the fields to return for all matching documents.
    Document projection;

    /// The order in which to return matching documents.
    Document sort;
};

struct RemoteInsertOneResult {
    BSONValue inserted_id;
};

struct RemoteInsertManyResult {
    
    /// Map of the index of the inserted document to the id of the inserted document.
    std::map<long int, BSONValue> inserted_ids;
};

struct RemoteDeleteResult {
    int deleted_count;
};

struct RemoteUpdateResult {
    int matched_count;
    int modified_count;
    BSONValue upserted_id;
};

struct RemoteFindOneAndModifyOptions {
    util::Optional<Document> projection;
    util::Optional<Document> sort;
    util::Optional<bool> upsert;
    util::Optional<bool> return_new_document;
};

} // namespace mongodb
} // namespace realm

#endif /* CORE_REMOTE_MONGO_RESULT_TYPES_HPP */
