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

#include "app.hpp"
#include <sstream>
#include "sync_manager.hpp"
#include "generic_network_transport.hpp"
#include "app_credentials.hpp"
#include "../external/json/json.hpp"

// wrap an optional json key into the Optional type
#define WRAP_JSON_OPT(JSON, KEY, RET_TYPE) \
JSON.find(KEY) != JSON.end() ? Optional<RET_TYPE>(JSON[KEY].get<RET_TYPE>()) : realm::util::none

namespace realm {
namespace app {

std::shared_ptr<App> App::app(const std::string app_id,
                              const realm::util::Optional<App::Config> config)
{
    return std::make_shared<App>(app_id, config);
}



static error::AppError handle_error(const Response response) {
    if (response.body.empty()) {
        return error::service("Unknown", "Unknown");
    }

    if (response.headers.find("Content-Type") != response.headers.end()) {
        if (response.headers.at("Content-Type") == "application/json") {
            auto body = nlohmann::json::parse(response.body);
            return error::service(body["errorCode"].get<std::string>(),
                                  body["error"].get<std::string>());
        } else {
            goto default_error;
        }
    } else {
        goto default_error;
    }

default_error:
    return error::service(std::string(response.body.begin(),
                                      response.body.end()),
                          "Unknown");

}

void App::login_with_credentials(const std::shared_ptr<AppCredentials> credentials,
                                 int timeout_ms,
                                 std::function<void(std::shared_ptr<SyncUser>, error::AppError)> completion_block) {
    // construct the route
    std::stringstream route;
    route << m_auth_route << "/providers/" << provider_type_from_enum(credentials->m_provider) << "/login";

    auto handler = [&](const Response response) {
        // if there is a already an error code, pass the error upstream
        if (response.status_code < 200 ||
            response.status_code >= 300) {
            return completion_block(nullptr, handle_error(response));
        }

        auto j = nlohmann::json::parse(response.body.begin(), response.body.end());

        realm::SyncUserIdentifier identifier {
            j["user_id"].get<std::string>(),
            m_auth_route
        };

        std::shared_ptr<realm::SyncUser> sync_user;
        try {
            sync_user = realm::SyncManager::shared().get_user(identifier,
                                                              j["refresh_token"].get<std::string>(),
                                                              j["access_token"].get<std::string>());
        } catch (error::AppError e) {
            return completion_block(nullptr, e);
        }

        std::stringstream profile_route;
        profile_route << m_base_route << "/auth/profile";

        std::stringstream bearer;
        bearer << "Bearer" << " " << sync_user->access_token();

        std::cout<<bearer.str()<<std::endl;

        GenericNetworkTransport::get()->send_request_to_server({
            Method::get,
            profile_route.str(),
            timeout_ms,
            {
                { "Content-Type", "application/json;charset=utf-8" },
                { "Accept", "application/json" },
                { "Authorization", bearer.str() }
            },
            std::vector<char>()
        }, [&](const Response) {
            // if there is a already an error code, pass the error upstream
            if (response.status_code < 200 ||
                response.status_code >= 300) {
                return completion_block(nullptr, handle_error(response));
            }

            j = nlohmann::json::parse(response.body);

            std::vector<SyncUserIdentity> identities;
            auto identities_json = j["identities"];

            for (size_t i = 0; i < identities_json.size(); i++)
            {
                auto identity_json = identities_json[i];
                identities.push_back(SyncUserIdentity(identity_json["id"], identity_json["provider_type"]));
            }

            sync_user->update_identities(identities);

            auto profile_data = j["data"];

            sync_user->update_user_profile(std::make_shared<SyncUserProfile>(WRAP_JSON_OPT(profile_data, "name", std::string),
                                                                             WRAP_JSON_OPT(profile_data, "email", std::string),
                                                                             WRAP_JSON_OPT(profile_data, "picture_url", std::string),
                                                                             WRAP_JSON_OPT(profile_data, "first_name", std::string),
                                                                             WRAP_JSON_OPT(profile_data, "last_name", std::string),
                                                                             WRAP_JSON_OPT(profile_data, "gender", std::string),
                                                                             WRAP_JSON_OPT(profile_data, "birthday", std::string),
                                                                             WRAP_JSON_OPT(profile_data, "min_age", std::string),
                                                                             WRAP_JSON_OPT(profile_data, "max_age", std::string)));

            return completion_block(sync_user, error::none);
        });
    };

    std::map<std::string, std::string> headers = {
        { "Content-Type", "application/json;charset=utf-8" },
        { "Accept", "application/json" }
    };

    GenericNetworkTransport::get()->send_request_to_server({
        Method::post,
        route.str(),
        timeout_ms,
        headers,
        credentials->serialize()
    }, handler);
}

}
}
