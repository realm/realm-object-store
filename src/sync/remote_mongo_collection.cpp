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
#include "realm/util/uri.hpp"

namespace realm {
namespace app {

static void handle_delete_count_response(util::Optional<AppError> error,
                                         util::Optional<bson::Bson> value,
                                         std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    if (value && !error) {
        try {
            auto document = static_cast<bson::BsonDocument>(*value);
            auto count = static_cast<int32_t>(document["deletedCount"]);
            return completion_block(count, error);
        } catch (const std::exception& e) {
            return completion_block(0, AppError(make_error_code(JSONErrorCode::bad_bson_parse), e.what()));
        }
    }
    
    return completion_block(0, error);
}

static void handle_update_response(util::Optional<AppError> error,
                                   util::Optional<bson::Bson> value,
                                   std::function<void(RemoteMongoCollection::RemoteUpdateResult,
                                                      util::Optional<AppError>)> completion_block)
{
    
    if (error) {
        return completion_block({}, error);
    }
    
    try {
        auto document = static_cast<bson::BsonDocument>(*value);
        auto matched_count = static_cast<int32_t>(document["matchedCount"]);
        auto modified_count = static_cast<int32_t>(document["modifiedCount"]);

        util::Optional<ObjectId> upserted_id;
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
        return completion_block({}, AppError(make_error_code(JSONErrorCode::bad_bson_parse), e.what()));
    }
}

static void handle_document_response(util::Optional<AppError> error,
                                     util::Optional<bson::Bson> value,
                                     std::function<void(util::Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    if (error) {
        return completion_block(util::none, error);
    }
    
    if (!value) {
        // no docs were found
        return completion_block(util::none, util::none);
    }
    
    if (bson::holds_alternative<util::None>(*value)) {
        // no docs were found
        return completion_block(util::none, util::none);
    }
    
    return completion_block(static_cast<bson::BsonDocument>(*value),
                            util::none);
}

void RemoteMongoCollection::find(const bson::BsonDocument& filter_bson,
                                 RemoteFindOptions options,
                                 std::function<void(util::Optional<bson::BsonArray>, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = m_base_operation_args;
        base_args["query"] = filter_bson;

        if (options.limit) {
            base_args["limit"] = *options.limit;
        }

        if (options.projection_bson) {
            base_args["project"] = *options.projection_bson;
        }

        if (options.sort_bson) {
            base_args["sort"] = *options.sort_bson;
        }
        
        m_service->call_function("find",
                                 bson::BsonArray({base_args}),
                                 m_service_name,
                                 [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
            if (error) {
                return completion_block(util::none, error);
            }

            return completion_block(util::some<bson::BsonArray>(static_cast<bson::BsonArray>(*value)), util::none);
        });
    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find(const bson::BsonDocument& filter_bson,
                                 std::function<void(util::Optional<bson::BsonArray>, util::Optional<AppError>)> completion_block)
{
    find(filter_bson, {}, completion_block);
}

void RemoteMongoCollection::find_one(const bson::BsonDocument& filter_bson,
                                     RemoteFindOptions options,
                                     std::function<void(util::Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    try {
        auto base_args = m_base_operation_args;
        base_args["query"] = filter_bson;

        if (options.limit) {
            base_args["limit"] = *options.limit;
        }

        if (options.projection_bson) {
            base_args["project"] = *options.projection_bson;
        }

        if (options.sort_bson) {
            base_args["sort"] = *options.sort_bson;
        }
        
        m_service->call_function("findOne",
                                 bson::BsonArray({base_args}),
                                 m_service_name,
                                 [completion_block](util::Optional<AppError> error,
                                                    util::Optional<bson::Bson> value) {
            handle_document_response(error, value, completion_block);
        });

    } catch (const std::exception& e) {
        return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
    }
}

void RemoteMongoCollection::find_one(const bson::BsonDocument& filter_bson,
                                     std::function<void(util::Optional<bson::BsonDocument>,
                                                        util::Optional<AppError>)> completion_block)
{
    find_one(filter_bson, {}, completion_block);
}

void RemoteMongoCollection::insert_one(const bson::BsonDocument& value_bson,
                                       std::function<void(util::Optional<ObjectId>, util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["document"] = value_bson;
    
    m_service->call_function("insertOne",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        if (error) {
            return completion_block(util::none, error);
        }

        auto document = static_cast<bson::BsonDocument>(*value);

        return completion_block(static_cast<ObjectId>(document["insertedId"]),
                                util::none);
    });
}

void RemoteMongoCollection::aggregate(const bson::BsonArray& pipline,
                                      std::function<void(util::Optional<bson::BsonArray>, util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["pipeline"] = pipline;
    
    m_service->call_function("aggregate",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        if (error) {
            return completion_block(util::none, error);
        }

        return completion_block(static_cast<bson::BsonArray>(*value), util::none);
    });
}

void RemoteMongoCollection::count(const bson::BsonDocument& filter_bson,
                                  int64_t limit,
                                  std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["query"] = filter_bson;
    
    if (limit != 0) {
        base_args["limit"] = limit;
    }
    
    m_service->call_function("count",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        if (error) {
            return completion_block(0, error);
        }

        return completion_block(static_cast<int64_t>(*value), util::none);
    });
}

void RemoteMongoCollection::count(const bson::BsonDocument& filter_bson,
                                  std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    count(filter_bson, 0, completion_block);
}

void RemoteMongoCollection::insert_many(bson::BsonArray documents,
                                        std::function<void(std::vector<ObjectId>,
                                                           util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["documents"] = documents;
    
    m_service->call_function("insertMany",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        if (error) {
            return completion_block({}, error);
        }

        auto bson = static_cast<bson::BsonDocument>(*value);
        auto inserted_ids = static_cast<bson::BsonArray>(bson["insertedIds"]);
        return completion_block(std::vector<ObjectId>(inserted_ids.begin(), inserted_ids.end()), error);
    });
}

void RemoteMongoCollection::delete_one(const bson::BsonDocument& filter_bson,
                                       std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["query"] = filter_bson;
    
    m_service->call_function("deleteOne",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        handle_delete_count_response(error, value, completion_block);
    });
}

void RemoteMongoCollection::delete_many(const bson::BsonDocument& filter_bson,
                                        std::function<void(uint64_t, util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["query"] = filter_bson;
    
    m_service->call_function("deleteMany",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        handle_delete_count_response(error, value, completion_block);
    });
}

void RemoteMongoCollection::update_one(const bson::BsonDocument& filter_bson,
                                       const bson::BsonDocument& update_bson,
                                       bool upsert,
                                       std::function<void(RemoteMongoCollection::RemoteUpdateResult,
                                                          util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["query"] = filter_bson;
    base_args["update"] = update_bson;
    base_args["upsert"] = upsert;

    m_service->call_function("updateOne",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        handle_update_response(error, value, completion_block);
    });
}

void RemoteMongoCollection::update_one(const bson::BsonDocument& filter_bson,
                                       const bson::BsonDocument& update_bson,
                                       std::function<void(RemoteMongoCollection::RemoteUpdateResult,
                                                          util::Optional<AppError>)> completion_block)
{
    update_one(filter_bson, update_bson, {}, completion_block);
}

void RemoteMongoCollection::update_many(const bson::BsonDocument& filter_bson,
                                        const bson::BsonDocument& update_bson,
                                        bool upsert,
                                        std::function<void(RemoteMongoCollection::RemoteUpdateResult,
                                                           util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["query"] = filter_bson;
    base_args["update"] = update_bson;
    base_args["upsert"] = upsert;
    
    m_service->call_function("updateMany",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        handle_update_response(error, value, completion_block);
    });
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
                                                std::function<void(util::Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["filter"] = filter_bson;
    base_args["update"] = update_bson;
    options.set_bson(base_args);
    
    m_service->call_function("findOneAndUpdate",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        handle_document_response(error, value, completion_block);
    });
}

void RemoteMongoCollection::find_one_and_update(const bson::BsonDocument& filter_bson,
                                                const bson::BsonDocument& update_bson,
                                                std::function<void(util::Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    find_one_and_update(filter_bson, update_bson, {}, completion_block);
}

void RemoteMongoCollection::find_one_and_replace(const bson::BsonDocument& filter_bson,
                                                 const bson::BsonDocument& replacement_bson,
                                                 RemoteMongoCollection::RemoteFindOneAndModifyOptions options,
                                                 std::function<void(util::Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["filter"] = filter_bson;
    base_args["update"] = replacement_bson;
    options.set_bson(base_args);

    m_service->call_function("findOneAndReplace",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        handle_document_response(error, value, completion_block);
    });
}

void RemoteMongoCollection::find_one_and_replace(const bson::BsonDocument& filter_bson,
                                                 const bson::BsonDocument& replacement_bson,
                                                 std::function<void(util::Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    find_one_and_replace(filter_bson, replacement_bson, {}, completion_block);
}

void RemoteMongoCollection::find_one_and_delete(const bson::BsonDocument& filter_bson,
                                                RemoteMongoCollection::RemoteFindOneAndModifyOptions options,
                                                std::function<void(util::Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    auto base_args = m_base_operation_args;
    base_args["filter"] = filter_bson;
    options.set_bson(base_args);

    m_service->call_function("findOneAndDelete",
                             bson::BsonArray({base_args}),
                             m_service_name,
                             [completion_block](util::Optional<AppError> error, util::Optional<bson::Bson> value) {
        handle_document_response(error, value, completion_block);
    });
}

void RemoteMongoCollection::find_one_and_delete(const bson::BsonDocument& filter_bson,
                                                std::function<void(util::Optional<bson::BsonDocument>, util::Optional<AppError>)> completion_block)
{
    find_one_and_delete(filter_bson, {}, completion_block);
}

void WatchStream::feed_buffer(std::string_view input) {
    REALM_ASSERT(m_state == NEED_DATA);
    m_buffer += input;
    advance_buffer_state();
}

void WatchStream::advance_buffer_state() {
    REALM_ASSERT(m_state == NEED_DATA);
    while (m_state == NEED_DATA) {
        if (m_buffer_offset == m_buffer.size()) {
            m_buffer.clear();
            m_buffer_offset = 0;
            return;
        }

        // NOTE not supporting CR-only newlines, just LF and CRLF.
        auto next_newline = m_buffer.find('\n', m_buffer_offset);
        if (next_newline == std::string::npos) {
            // We have a partial line.
            if (m_buffer_offset != 0) {
                // Slide the partial line down to the front of the buffer.
                m_buffer.assign(m_buffer.data() + m_buffer_offset, m_buffer.size() - m_buffer_offset);
                m_buffer_offset = 0;
            }
            return;
        }

        feed_line(std::string_view(m_buffer.data() + m_buffer_offset, next_newline - m_buffer_offset));
        m_buffer_offset = next_newline + 1; // Advance past this line, including its newline.
    }
}

void WatchStream::feed_line(std::string_view line) {
    REALM_ASSERT(m_state == NEED_DATA);
    // This is an implementation of the algorithm described at
    // https://html.spec.whatwg.org/multipage/server-sent-events.html#event-stream-interpretation.
    // Currently the server does not use id or retry lines, so that processing isn't implemented.

    // ignore trailing LF if not removed by SDK.
    if (!line.empty() && line.back() == '\n')
        line = line.substr(0, line.size() - 1);

    // ignore trailing CR from CRLF
    if (!line.empty() && line.back() == '\r')
        line = line.substr(0, line.size() - 1);

    if (line.empty()) {
        // This is the "dispatch the event" portion of the algorithm.
        if (m_data_buffer.empty()) {
            m_event_type.clear();
            return;
        }

        if (m_data_buffer.back() == '\n')
            m_data_buffer.pop_back();

        feed_sse({m_data_buffer, m_event_type});
        m_data_buffer.clear();
        m_event_type.clear();
    }

    if (line[0] == ':')
        return;

    const auto colon = line.find(':');
    const auto field = line.substr(0, colon);
    auto value = colon == std::string::npos ? std::string_view() : line.substr(colon + 1);
    if (!value.empty() && value[0] == ' ')
        value = value.substr(1);

    if (field == "event") {
        m_event_type = value;
    } else if (field == "data") {
        m_data_buffer += value;
        m_data_buffer += '\n';
    } else {
        // line is ignored (even if field is id or retry).
    }
}

void WatchStream::feed_sse(ServerSentEvent sse) {
    REALM_ASSERT(m_state == NEED_DATA);
    std::string buffer; // must outlast if-block since we bind sse.data to it.
    size_t first_percent = sse.data.find('%');
    if (first_percent != std::string::npos) {
        // For some reason, the stich server decided to add percent-encoding for '%', '\n', and '\r' to its
        // event-stream replies. But it isn't real urlencoding, since most characters pass through, so we can't use
        // uri_percent_decode() here.
        buffer.reserve(sse.data.size());
        size_t start = 0;
        while (true) {
            auto percent = start == 0 ? first_percent : sse.data.find('%', start);
            if (percent == std::string::npos) {
                buffer += sse.data.substr(start);
                break;
            }

            buffer += sse.data.substr(start, percent - start);

            auto encoded = sse.data.substr(percent, 3); // may be smaller than 3 if string ends with %
            if (encoded == "%25") {
                buffer += '%';
            } else if (encoded == "%0A") {
                buffer += '\x0A'; // '\n'
            } else if (encoded == "%0D") {
                buffer += '\x0D'; // '\r'
            } else {
                buffer += encoded; // propagate as-is
            }
            start = percent + encoded.size();
        }

        sse.data = buffer;
    }

    if (sse.eventType.empty() || sse.eventType == "message") {
        try {
            auto parsed = bson::parse(sse.data);
            if (parsed.type() == bson::Bson::Type::Document) {
                m_next_event = parsed.operator const bson::BsonDocument&();
                m_state = HAVE_EVENT;
                return;
            }
        } catch (...) {
            // fallthrough to same handling as for non-document value.
        }
        m_state = HAVE_ERROR;
        m_error.emplace(app::make_error_code(JSONErrorCode::bad_bson_parse),
                        "server returned malformed event: " + std::string(sse.data));
    } else if (sse.eventType == "error") {
        m_state = HAVE_ERROR;

        // default error message if we have issues parsing the reply.
        m_error.emplace(app::make_error_code(ServiceErrorCode::unknown), std::string(sse.data));
        try {
            auto parsed = bson::parse(sse.data);
            if (parsed.type() != bson::Bson::Type::Document) return;
            auto& obj = parsed.operator const bson::BsonDocument&();
            auto& code = obj.at("error_code");
            auto& msg = obj.at("error");
            if (code.type() != bson::Bson::Type::String) return;
            if (msg.type() != bson::Bson::Type::String) return;
            m_error.emplace(app::make_error_code(app::service_error_code_from_string(
                                code.operator const std::string&())),
                            msg.operator const std::string&());
        } catch(...) {
            return; // Use the default state.
        }
    } else {
        // Ignore other event types
    }
}
} // namespace app
} // namespace realm
