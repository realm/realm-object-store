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

// FIXME: This class has alot of json parsing with hardcoded strings,
// this will go away with a subsequent PR that replaces JSON with BSON

static void handle_response(util::Optional<AppError> error,
                            util::Optional<std::string> value,
                            std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
    if (value && !error) {
        //response can be a http 200 and return "null" in the body
        if (value && (*value == "null" || *value == "")) {
            return completion_block(util::none, error);
        } else {
            return completion_block(*value, error);
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

        ObjectId upserted_id;
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

void RemoteMongoCollection::find(const std::string& filter_json,
                                 RemoteFindOptions options,
                                 std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
    try {
                    
        auto base_args = bson::BsonDocument(m_base_operation_args);
        base_args["query"] = bson::parse(filter_json);

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

void RemoteMongoCollection::find(const std::string& filter_json,
                                 std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
    find(filter_json, {}, completion_block);
}

void RemoteMongoCollection::find_one(const std::string& filter_json,
                                     RemoteFindOptions options,
                                     std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
        
    try {
        auto base_args = m_base_operation_args;
//        base_args.push_back({"query", nlohmann::json::parse(filter_json)});
//
//        if (options.limit) {
//            base_args.push_back({"limit", options.limit.value()});
//        }
//
//        if (options.projection_json) {
//            base_args.push_back({"project", nlohmann::json::parse(options.projection_json.value())});
//        }
//
//        if (options.projection_json) {
//            base_args.push_back({"sort", nlohmann::json::parse(options.sort_json.value())});
//        }
//
//        auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args} )}});
//
//        m_service.call_function("findOne",
//                                args.dump(),
//                                [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//            handle_response(error, value, completion_block);
//        });
    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find_one(const std::string& filter_json,
                                     std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
    find_one(filter_json, {}, completion_block);
}

void RemoteMongoCollection::insert_one(const std::string& value_json,
                                       std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = m_base_operation_args;
//        auto document_json = nlohmann::json::parse(value_json);
//        base_args.push_back({"document", document_json});
//        auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args})}});
//
//        m_service.call_function("insertOne",
//                                 args.dump(),
//                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//            handle_response(error, value, completion_block);
//        });
    } catch (const std::exception& e) {
       return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), util::format("document parse %1", e.what())));
    }
}

void RemoteMongoCollection::aggregate(std::vector<std::string> pipline,
                                      std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
    try {
        auto pipelines = nlohmann::json::array();
//        for (std::string& pipeline_json : pipline) {
//            pipelines.push_back(nlohmann::json::parse(pipeline_json));
//        }
//
//        auto base_args = m_base_operation_args;
//        base_args.push_back({"pipeline", pipelines});
//        auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args})}});
//
//        m_service.call_function("aggregate",
//                              args.dump(),
//                              [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//            handle_response(error, value, completion_block);
//        });
    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::count(const std::string& filter_json,
                                  uint64_t limit,
                                  std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = m_base_operation_args;
//        base_args.push_back({"query", nlohmann::json::parse(filter_json)});
//
//        if (limit != 0) {
//            base_args.push_back({"limit", limit});
//        }
//
//        auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args})}});
//
//        m_service.call_function("count",
//                                 args.dump(),
//                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//            handle_count_response(error, value, completion_block);
//        });
    } catch (const std::exception& e) {
        return completion_block(0, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::count(const std::string& filter_json,
                                  std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    count(filter_json, 0, completion_block);
}

void RemoteMongoCollection::insert_many(std::vector<std::string> documents,
                                        std::function<void(std::vector<std::string>, util::Optional<AppError>)> completion_block)
{
     try {

//         auto documents_array = nlohmann::json::array();
//         for (std::string& document_json : documents) {
//             documents_array.push_back(nlohmann::json::parse(document_json));
//         }
//
//         auto base_args = m_base_operation_args;
//         base_args.push_back({"documents", documents_array});
//         auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args})}});
//
//         m_service.call_function("insertMany",
//                                  args.dump(),
//                                  [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//             if (value && !error) {
//                 try {
//                     auto json = nlohmann::json::parse(*value);
//                     auto inserted_ids = json.at("insertedIds").get<std::vector<nlohmann::json>>();
//                     auto oid_array = std::vector<std::string>();
//                     for(auto& inserted_id : inserted_ids) {
//                         auto oid = inserted_id.at("$oid").get<std::string>();
//                         oid_array.push_back(oid);
//                     }
//                     return completion_block(oid_array, error);
//                 } catch (const std::exception& e) {
//                     return completion_block(std::vector<std::string>(), AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
//                 }
//             }
//
//             return completion_block(std::vector<std::string>(), error);
//         });
    } catch (const std::exception& e) {
        return completion_block(std::vector<std::string>(), AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::delete_one(const std::string& filter_json,
                                       std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    try {
//
//         auto base_args = m_base_operation_args;
//         base_args.push_back({"query", nlohmann::json::parse(filter_json)});
//         auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args})}});
//
//        m_service.call_function("deleteOne",
//                                  args.dump(),
//                                  [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//             handle_delete_count_response(error, value, completion_block);
//         });
    } catch (const std::exception& e) {
        return completion_block(0, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::delete_many(const std::string& filter_json,
                                        std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    try {
//
//         auto base_args = m_base_operation_args;
//         base_args.push_back({"query", nlohmann::json::parse(filter_json)});
//         auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args})}});
//
//        m_service.call_function("deleteMany",
//                                  args.dump(),
//                                  [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//             handle_delete_count_response(error, value, completion_block);
//         });
    } catch (const std::exception& e) {
        return completion_block(0, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::update_one(const std::string& filter_json,
                                       const std::string& update_json,
                                       bool upsert,
                                       std::function<void(RemoteMongoCollection::RemoteUpdateResult, util::Optional<AppError>)> completion_block)
{
    try {
//        auto base_args = m_base_operation_args;
//        base_args.push_back({"query", nlohmann::json::parse(filter_json)});
//        base_args.push_back({"update", nlohmann::json::parse(update_json)});
//        base_args.push_back({"upsert", upsert});
//        auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args})}});
//
//        m_service.call_function("updateOne",
//                                 args.dump(),
//                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//            handle_update_response(error, value, completion_block);
//        });
    } catch (const std::exception& e) {
        return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::update_one(const std::string& filter_json,
                                       const std::string& update_json,
                                       std::function<void(RemoteMongoCollection::RemoteUpdateResult, util::Optional<AppError>)> completion_block)
{
    update_one(filter_json, update_json, {}, completion_block);
}

void RemoteMongoCollection::update_many(const std::string& filter_json,
                                        const std::string& update_json,
                                        bool upsert,
                                        std::function<void(RemoteMongoCollection::RemoteUpdateResult, util::Optional<AppError>)> completion_block)
{
    try {
//        auto base_args = m_base_operation_args;
//        base_args.push_back({"query", nlohmann::json::parse(filter_json)});
//        base_args.push_back({"update", nlohmann::json::parse(update_json)});
//        auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args})}});
//
//        args.push_back({"upsert", upsert });
//
//        m_service.call_function("updateMany",
//                                 args.dump(),
//                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//            handle_update_response(error, value, completion_block);
//        });
    } catch (const std::exception& e) {
        return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::update_many(const std::string& filter_json,
                                        const std::string& update_json,
                                        std::function<void(RemoteMongoCollection::RemoteUpdateResult, util::Optional<AppError>)> completion_block)
{
    update_many(filter_json, update_json, {}, completion_block);
}

void RemoteMongoCollection::find_one_and_update(const std::string& filter_json,
                                                const std::string& update_json,
                                                RemoteMongoCollection::RemoteFindOneAndModifyOptions options,
                                                std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
    try {
//        auto base_args = m_base_operation_args;
//        base_args.push_back({"query", nlohmann::json::parse(filter_json)});
//        base_args.push_back({"update", nlohmann::json::parse(update_json)});
//        options.set_json(base_args);
//
//        auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args} )}});
//
//        m_service.call_function("findOneAndUpdate",
//                                 args.dump(),
//                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//            handle_response(error, value, completion_block);
//        });
    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find_one_and_update(const std::string& filter_json,
                                                const std::string& update_json,
                                                std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
    find_one_and_update(filter_json, update_json, {}, completion_block);
}

void RemoteMongoCollection::find_one_and_replace(const std::string& filter_json,
                                                 const std::string& replacement_json,
                                                 RemoteMongoCollection::RemoteFindOneAndModifyOptions options,
                                                 std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
    try {
//        auto base_args = m_base_operation_args;
//        base_args.push_back({"query", nlohmann::json::parse(filter_json)});
//        base_args.push_back({"update", nlohmann::json::parse(replacement_json)});
//        options.set_json(base_args);
//
//        auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args})}});
//
//        m_service.call_function("findOneAndReplace",
//                                 args.dump(),
//                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
//            handle_response(error, value, completion_block);
//        });
    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find_one_and_replace(const std::string& filter_json,
                                                 const std::string& replacement_json,
                                                 std::function<void(Optional<std::string>, util::Optional<AppError>)> completion_block)
{
    find_one_and_replace(filter_json, replacement_json, {}, completion_block);
}

void RemoteMongoCollection::find_one_and_delete(const std::string& filter_json,
                                                RemoteMongoCollection::RemoteFindOneAndModifyOptions options,
                                                std::function<void(util::Optional<AppError>)> completion_block)
{
    try {
//        auto base_args = m_base_operation_args;
//        base_args.push_back({"query", nlohmann::json::parse(filter_json)});
//        options.set_json(base_args);
//        auto args = nlohmann::json({{"arguments", nlohmann::json::array({base_args})}});
//
//        m_service.call_function("findOneAndDelete",
//                                 args.dump(),
//                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string>) {
//            completion_block(error);
//        });
    } catch (const std::exception& e) {
        return completion_block(AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find_one_and_delete(const std::string& filter_json,
                                                std::function<void(util::Optional<AppError>)> completion_block)
{
    find_one_and_delete(filter_json, {}, completion_block);
}

} // namespace app
} // namespace realm

