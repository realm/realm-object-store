//
//  push_client.cpp
//  realm-object-store
//
//  Created by Lee Maguire on 06/05/2020.
//

#include "sync/push_client.hpp"
#include "sync/app_utils.hpp"

namespace realm {
namespace app {

void PushClient::register_device(const std::string& registration_token,
                                 std::shared_ptr<SyncUser> sync_user,
                                 std::function<void(util::Optional<AppError>)> completion_block)
{
    auto handler = [completion_block](const Response& response) {
        if (auto error = check_for_errors(response)) {
            return completion_block(error);
        } else {
            return completion_block({});
        }
    };
    auto push_route = util::format("/app/%1/push/providers/%2/registration", m_app_id, m_service_name);
    std::string route = m_auth_request_client->url_for_path(push_route);
    
    auto args = bson::BsonDocument();
    args["registrationToken"] = registration_token;
    
    std::stringstream s;
    s << bson::Bson(args);
    
    Request request {
        .method = HttpMethod::put,
        .url = route,
        .body = s.str()
    };
    
    m_auth_request_client->do_authenticated_request(request,
                                                   sync_user,
                                                   handler);
}

void PushClient::deregister_device(const std::string& registration_token,
                                   std::shared_ptr<SyncUser> sync_user,
                                   std::function<void(util::Optional<AppError>)> completion_block)
{
    auto handler = [completion_block](const Response& response) {
        if (auto error = check_for_errors(response)) {
            return completion_block(error);
        } else {
            return completion_block({});
        }
    };
    auto push_route = util::format("/app/%1/push/providers/%2/registration", m_app_id, m_service_name);
    std::string route = m_auth_request_client->url_for_path(push_route);
    
    auto args = bson::BsonDocument();
    args["registrationToken"] = registration_token;
    
    std::stringstream s;
    s << bson::Bson(args);
    
    Request request {
        .method = HttpMethod::del,
        .url = route,
        .body = s.str()
    };
    
    m_auth_request_client->do_authenticated_request(request,
                                                   sync_user,
                                                   handler);
}

static bson::BsonDocument encode_send_message_notificaation(const PushClient::SendMessageNotification& request)
{
    bson::BsonDocument document;

    if (request.title) {
        document["request"] = *request.title;
    }
    
    if (request.body) {
        document["body"] = *request.title;
    }

    if (request.sound) {
        document["sound"] = *request.sound;
    }

    if (request.click_action) {
        document["clickAction"] = *request.click_action;
    }
    
    if (request.body_loc_key) {
        document["bodyLocKey"] = *request.body_loc_key;
    }
    
    if (request.body_loc_args) {
        document["bodyLocArgs"] = *request.body_loc_args;
    }

    if (request.title_loc_key) {
        document["titleLocKey"] = *request.title_loc_key;
    }

    if (request.title_loc_args) {
        document["titleLocArgs"] = *request.title_loc_args;
    }

    if (request.icon) {
        document["icon"] = *request.icon;
    }

    if (request.tag) {
        document["tag"] = *request.tag;
    }

    if (request.color) {
        document["color"] = *request.color;
    }

    if (request.badge) {
        document["badge"] = *request.badge;
    }

    return document;
}

static bson::BsonDocument encode_send_message_request(const PushClient::SendMessageRequest& request)
{

    bson::BsonDocument document;
        
    if (request.priority == PushClient::SendMessageRequest::SendMessagePriority::normal) {
        document["priority"] = "normal";
    } else if (request.priority == PushClient::SendMessageRequest::SendMessagePriority::high) {
        document["priority"] = "high";
    }

    if (request.collapse_key) {
        document["collapseKey"] = *request.collapse_key;
    }
    
    if (request.content_available) {
        document["contentAvailable"] = *request.content_available;
    }
    
    if (request.mutable_content) {
        document["mutableContent"] = *request.mutable_content;
    }

    if (request.time_to_live) {
        document["timeToLive"] = *request.time_to_live;
    }
    
    if (request.data) {
        document["data"] = *request.data;
    }
    
    if (request.notification) {
        document["notification"] = encode_send_message_notificaation(*request.notification);
    }
    
    return document;
}

void PushClient::send_message(const bson::BsonArray& args, std::function<void(util::Optional<AppError>,
                                                                         util::Optional<SendMessageResult>)> completion_block) {
    m_app_service_client->call_function("send",
                                        bson::BsonArray({args}),
                                        m_service_name,
                                        [&](util::Optional<AppError> error, util::Optional<bson::Bson> document) {
        
    });
}

void PushClient::send_message(const std::string& target,
                              const SendMessageRequest& request,
                              std::function<void(util::Optional<AppError>,
                                                 util::Optional<SendMessageResult>)> completion_block)
{
    auto args = encode_send_message_request(request);
    args["to"] = target;
    send_message(bson::BsonArray({args}), completion_block);
}

void PushClient::send_message_to_user_ids(std::vector<std::string> user_ids,
                              const SendMessageRequest& request,
                              std::function<void(util::Optional<AppError>,
                                                 util::Optional<SendMessageResult>)> completion_block)
{
    auto args = encode_send_message_request(request);
    //args["userIds"] = user_ids;
    send_message(bson::BsonArray({args}), completion_block);
}

void PushClient::send_message_to_registration_tokens(bson::BsonArray registration_tokens,
                                                     const SendMessageRequest& request,
                                                     std::function<void(util::Optional<AppError>,
                                                                        util::Optional<SendMessageResult>)> completion_block)
{
    auto args = encode_send_message_request(request);
    args["registrationTokens"] = registration_tokens;
    send_message(bson::BsonArray({args}), completion_block);
}

} // namespace app
} // namespace realm
