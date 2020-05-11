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
    
    bson::BsonDocument args {
        {"registrationToken", registration_token}
    };
    
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


} // namespace app
} // namespace realm
