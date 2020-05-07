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

void PushClient::send_message(const std::string& target,
                  const FCMSendMessageRequest& request,
                  std::function<void(util::Optional<AppError>,
                                     util::Optional<FCMSendMessageResult>)> completion_block)
{
    
}

void PushClient::send_message_to_user_ids(std::vector<std::string> user_ids,
                              const FCMSendMessageRequest& request,
                              std::function<void(util::Optional<AppError>,
                                                 util::Optional<FCMSendMessageResult>)> completion_block)
{
    
}

void PushClient::send_message_to_registration_tokens(std::vector<std::string> user_ids,
                                         const FCMSendMessageRequest& request,
                                         std::function<void(util::Optional<AppError>,
                                                            util::Optional<FCMSendMessageResult>)> completion_block)
{
    
}

} // namespace app
} // namespace realm
