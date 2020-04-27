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

#include "sync/remote_mongo_collection.hpp"

namespace realm {
namespace app {

static void handle_response(util::Optional<AppError> error,
                            util::Optional<std::string> value,
                            std::function<void(Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    if (value && !error) {
        //response can be a http 200 and return "null" in the body
        if (value && (*value == "null" || *value == "")) {
            return completion_block(util::none, error);
        } else {
            return completion_block(static_cast<bson::BsonDocument>(bson::parse(*value)), error);
        }
    }
    
    return completion_block(util::none, error);
}

static void handle_response(util::Optional<AppError> error,
                            util::Optional<std::string> value,
                            std::function<void(Optional<ObjectId>, util::Optional<AppError>)> completion_block)
{
    if (value && !error) {
        //response can be a http 200 and return "null" in the body
        if (value && (*value == "null" || *value == "")) {
            return completion_block(util::none, error);
        } else {
            auto document = static_cast<bson::BsonDocument>(bson::parse(*value));
            return completion_block(static_cast<ObjectId>(document["insertedId"]), error);
        }
    }
    
    return completion_block(util::none, error);
}

static void handle_response(util::Optional<AppError> error,
                            util::Optional<std::string> value,
                            std::function<void(Optional<bson::BsonArray>, util::Optional<AppError>)> completion_block)
{
    if (value && !error) {
        //response can be a http 200 and return "null" in the body
        if (value && (*value == "null" || *value == "")) {
            return completion_block(util::none, error);
        } else {
            auto x =  bson::parse(*value);
            auto xx = static_cast<bson::BsonArray>(bson::parse(*value));
            return completion_block(static_cast<bson::BsonArray>(bson::parse(*value)), error);
        }
    }
    
    return completion_block(util::none, error);
}

static void handle_count_response(util::Optional<AppError> error,
                                  util::Optional<std::string> value,
                                  std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    if (error) {
        return completion_block(0, error);
    }
    
    try {
        auto bson = bson::parse(*value);
        auto count = static_cast<int64_t>(bson);
        return completion_block(count, error);
    } catch (const std::exception& e) {
        return completion_block(0, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

static void handle_delete_count_response(util::Optional<AppError> error,
                                         util::Optional<std::string> value,
                                         std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    if (value && !error) {
        try {
            auto bson = bson::parse(*value);
            auto document = static_cast<bson::BsonDocument>(bson);
            auto count = static_cast<int32_t>(document["deletedCount"]);
            return completion_block(count, error);
        } catch (const std::exception& e) {
            return completion_block(0, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
        }
    }
    
    return completion_block(0, error);
}

static void handle_update_response(util::Optional<AppError> error,
                                   util::Optional<std::string> value,
                                   std::function<void(RemoteMongoCollection::RemoteUpdateResult, util::Optional<AppError>)> completion_block)
{
    
    if (error) {
        return completion_block({}, error);
    }
    
    try {
        auto bson = bson::parse(*value);
        auto document = static_cast<bson::BsonDocument>(bson);
        auto matched_count = static_cast<int32_t>(document["matchedCount"]);
        auto modified_count = static_cast<int32_t>(document["modifiedCount"]);

        Optional<ObjectId> upserted_id;
        auto it = document.find("upsertedId");
        if (it != document.end()) {
            upserted_id = static_cast<ObjectId>(document["upsertedId"]);
        }
        
        return completion_block(RemoteMongoCollection::RemoteUpdateResult {
            matched_count,
            modified_count,
            upserted_id
        }, error);
    } catch (const std::exception& e) {
        return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

static std::string dump(const bson::BsonDocument& document) {
    bson::BsonDocument args;
    args["arguments"] = bson::BsonArray({document});
    std::stringstream s;
    s << bson::Bson(args);
    return s.str();
}

void RemoteMongoCollection::find(const bson::BsonDocument& filter_bson,
                                 RemoteFindOptions options,
                                 std::function<void(Optional<bson::BsonArray>, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["query"] = filter_bson;

        if (options.limit) {
            base_args["limit"] = static_cast<int32_t>(*options.limit);
        }

        if (options.projection_json) {
            base_args["project"] = static_cast<bson::BsonDocument>(bson::parse(*options.projection_json));
        }

        if (options.sort_json) {
            base_args["sort"] = static_cast<bson::BsonDocument>(bson::parse(*options.sort_json));
        }
        
        m_service.call_function("find",
                                dump(base_args),
                                [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find(const bson::BsonDocument& filter_bson,
                                 std::function<void(Optional<bson::BsonArray>, util::Optional<AppError>)> completion_block)
{
    find(filter_bson, {}, completion_block);
}

void RemoteMongoCollection::find_one(const bson::BsonDocument& filter_bson,
                                     RemoteFindOptions options,
                                     std::function<void(Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["query"] = filter_bson;

        if (options.limit) {
            base_args["limit"] = static_cast<int32_t>(*options.limit);
        }

        if (options.projection_json) {
            base_args["project"] = static_cast<bson::BsonDocument>(bson::parse(*options.projection_json));
        }

        if (options.sort_json) {
            base_args["sort"] = static_cast<bson::BsonDocument>(bson::parse(*options.sort_json));
        }
        
        m_service.call_function("findOne",
                                dump(base_args),
                                [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find_one(const bson::BsonDocument& filter_bson,
                                     std::function<void(Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    find_one(filter_bson, {}, completion_block);
}

void RemoteMongoCollection::insert_one(const bson::BsonDocument& value_bson,
                                       std::function<void(Optional<ObjectId>, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["document"] = value_bson;

        m_service.call_function("insertOne",
                                 dump(base_args),
                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
       return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), util::format("document parse %1", e.what())));
    }
}

void RemoteMongoCollection::aggregate(const bson::BsonArray& pipline,
                                      std::function<void(Optional<bson::BsonArray>, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["pipeline"] = pipline;

        m_service.call_function("aggregate",
                              dump(base_args),
                              [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::count(const bson::BsonDocument& filter_bson,
                                  int64_t limit,
                                  std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["document"] = filter_bson;

        if (limit != 0) {
            base_args["limit"] = limit;
        }

        m_service.call_function("count",
                                 dump(base_args),
                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_count_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block(0, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::count(const bson::BsonDocument& filter_json,
                                  std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    count(filter_json, 0, completion_block);
}

void RemoteMongoCollection::insert_many(bson::BsonArray documents,
                                        std::function<void(std::vector<ObjectId>, util::Optional<AppError>)> completion_block)
{
     try {
         auto base_args = bson::BsonDocument(m_base_operation_args);
         base_args["documents"] = documents;

         m_service.call_function("insertMany",
                                  dump(base_args),
                                  [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
             if (value && !error) {
                 try {
                     auto bson = static_cast<bson::BsonDocument>(bson::parse(*value));
                     auto inserted_ids = static_cast<bson::BsonArray>(bson["insertedIds"]);
                     auto oid_array = std::vector<ObjectId>();
                     for(auto& inserted_id : inserted_ids) {
                         auto oid = static_cast<ObjectId>(inserted_id);
                         oid_array.push_back(oid);
                     }
                     return completion_block(oid_array, error);
                 } catch (const std::exception& e) {
                     return completion_block(std::vector<ObjectId>(), AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
                 }
             }

             return completion_block(std::vector<ObjectId>(), error);
         });
    } catch (const std::exception& e) {
        return completion_block(std::vector<ObjectId>(), AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::delete_one(const bson::BsonDocument& filter_bson,
                                       std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["query"] = filter_bson;
        
        m_service.call_function("deleteOne",
                                  dump(base_args),
                                  [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
             handle_delete_count_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block(0, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::delete_many(const bson::BsonDocument& filter_bson,
                                        std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["query"] = filter_bson;
        
        m_service.call_function("deleteMany",
                                  dump(base_args),
                                  [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
             handle_delete_count_response(error, value, completion_block);
         });
    } catch (const std::exception& e) {
        return completion_block(0, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::update_one(const bson::BsonDocument& filter_bson,
                                       const bson::BsonDocument& update_bson,
                                       bool upsert,
                                       std::function<void(RemoteMongoCollection::RemoteUpdateResult, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["query"] = filter_bson;
        base_args["update"] = update_bson;
        base_args["upsert"] = upsert;

        m_service.call_function("updateOne",
                                 dump(base_args),
                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_update_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::update_one(const bson::BsonDocument& filter_json,
                                       const bson::BsonDocument& update_json,
                                       std::function<void(RemoteMongoCollection::RemoteUpdateResult, util::Optional<AppError>)> completion_block)
{
    update_one(filter_json, update_json, {}, completion_block);
}

void RemoteMongoCollection::update_many(const bson::BsonDocument& filter_bson,
                                        const bson::BsonDocument& update_bson,
                                        bool upsert,
                                        std::function<void(RemoteMongoCollection::RemoteUpdateResult, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["query"] = filter_bson;
        base_args["update"] = update_bson;
        base_args["upsert"] = upsert;
        
        m_service.call_function("updateMany",
                                 dump(base_args),
                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_update_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::update_many(const bson::BsonDocument& filter_bson,
                                        const bson::BsonDocument& update_bson,
                                        std::function<void(RemoteMongoCollection::RemoteUpdateResult, util::Optional<AppError>)> completion_block)
{
    update_many(filter_bson, update_bson, {}, completion_block);
}

void RemoteMongoCollection::find_one_and_update(const bson::BsonDocument& filter_bson,
                                                const bson::BsonDocument& update_bson,
                                                RemoteMongoCollection::RemoteFindOneAndModifyOptions options,
                                                std::function<void(Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["query"] = filter_bson;
        base_args["update"] = update_bson;
        options.set_bson(base_args);
        
        m_service.call_function("findOneAndUpdate",
                                 dump(base_args),
                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find_one_and_update(const bson::BsonDocument& filter_json,
                                                const bson::BsonDocument& update_json,
                                                std::function<void(Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    find_one_and_update(filter_json, update_json, {}, completion_block);
}

void RemoteMongoCollection::find_one_and_replace(const bson::BsonDocument& filter_bson,
                                                 const bson::BsonDocument& replacement_bson,
                                                 RemoteMongoCollection::RemoteFindOneAndModifyOptions options,
                                                 std::function<void(Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["query"] = filter_bson;
        base_args["update"] = replacement_bson;
        options.set_bson(base_args);

        m_service.call_function("findOneAndReplace",
                                 dump(base_args),
                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find_one_and_replace(const bson::BsonDocument& filter_bson,
                                                 const bson::BsonDocument& replacement_bson,
                                                 std::function<void(Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    find_one_and_replace(filter_bson, replacement_bson, {}, completion_block);
}

void RemoteMongoCollection::find_one_and_delete(const bson::BsonDocument& filter_bson,
                                                RemoteMongoCollection::RemoteFindOneAndModifyOptions options,
                                                std::function<void(util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["query"] = filter_bson;
        options.set_bson(base_args);

        m_service.call_function("findOneAndDelete",
                                 dump(base_args),
                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string>) {
            completion_block(error);
        });
    } catch (const std::exception& e) {
        return completion_block(AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find_one_and_delete(const bson::BsonDocument& filter_json,
                                                std::function<void(util::Optional<AppError>)> completion_block)
{
    find_one_and_delete(filter_json, {}, completion_block);
}

} // namespace app
} // namespace realm

