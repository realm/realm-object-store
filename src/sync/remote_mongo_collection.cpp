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

#include "remote_mongo_collection.hpp"

namespace realm {
namespace app {

/*
 public func findOne(_ filter: Document = [:], options: RemoteFindOptions? = nil) throws -> T? {
     var args = baseOperationArgs

     args["query"] = filter
     if let options = options {
         if let projection = options.projection {
             args[RemoteFindOptionsKeys.projection.rawValue] = projection
         }
         if let sort = options.sort {
             args[RemoteFindOptionsKeys.sort.rawValue] = sort
         }
     }

     return try self.service.callFunctionOptionalResult(withName: "findOne",
                                                        withArgs: [args],
                                                        withRequestTimeout: nil)
 }
 */

static void handle_response(util::Optional<AppError> error,
                            util::Optional<std::string> value,
                            std::function<void(std::string, util::Optional<AppError>)> completion_block)
{
    if (value && !error) {
        return completion_block(value.value(), error);
    }
    
    return completion_block("", error);
}

void RemoteMongoCollection::find(const std::string& filter_json,
                                 RemoteFindOptions options,
                                 std::function<void(std::string, util::Optional<AppError>)> completion_block)
{
        
    try {
        auto base_args = m_base_operation_args;
        base_args.push_back({ "query", nlohmann::json::parse(filter_json) });
        auto args = nlohmann::json( {{"arguments", nlohmann::json::array({base_args} ) }} );
        
        if (options.limit) {
            args.push_back({ "limit", options.limit.value() });
        }
        
        if (options.projection_json) {
            args.push_back({ "project", nlohmann::json::parse(options.projection_json.value()) });
        }
        
        if (options.projection_json) {
            args.push_back({ "sort", nlohmann::json::parse(options.sort_json.value()) });
        }
                
        m_service->call_function("find",
                                 args.dump(),
                                 util::Optional<std::string>("BackingDB"),
                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find(const std::string& filter_json,
                                 std::function<void(std::string, util::Optional<AppError>)> completion_block)
{
    find(filter_json, {}, completion_block);
}

void RemoteMongoCollection::find_one(const std::string& filter_json,
                                 RemoteFindOptions options,
                                 std::function<void(std::string, util::Optional<AppError>)> completion_block)
{
        
    try {
        auto base_args = m_base_operation_args;
        base_args.push_back({ "query", nlohmann::json::parse(filter_json) });
        auto args = nlohmann::json( {{"arguments", nlohmann::json::array({base_args} ) }} );
        
        if (options.limit) {
            args.push_back({ "limit", options.limit.value() });
        }
        
        if (options.projection_json) {
            args.push_back({ "project", nlohmann::json::parse(options.projection_json.value()) });
        }
        
        if (options.projection_json) {
            args.push_back({ "sort", nlohmann::json::parse(options.sort_json.value()) });
        }
                
        m_service->call_function("findOne",
                                 args.dump(),
                                 util::Optional<std::string>("BackingDB"),
                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
        return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find_one(const std::string& filter_json,
                                 std::function<void(std::string, util::Optional<AppError>)> completion_block)
{
    find(filter_json, {}, completion_block);
}

void RemoteMongoCollection::insert_one(const std::string& value_json,
                                       std::function<void(std::string, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = m_base_operation_args;
        base_args.push_back({ "document", nlohmann::json::parse(value_json) });
        auto args = nlohmann::json( {{"arguments", nlohmann::json::array({base_args} ) }} );
            
        m_service->call_function("insertOne",
                                 args.dump(),
                                 util::Optional<std::string>("BackingDB"),
                                 [completion_block](util::Optional<AppError> error, util::Optional<std::string> value) {
            handle_response(error, value, completion_block);
        });
    } catch (const std::exception& e) {
       return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
   }
    
}

}
}

