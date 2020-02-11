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

std::shared_ptr<RealmApp> RealmApp::app(const std::string app_id,
                                        const realm::util::Optional<RealmApp::Config> config)
{
    return std::make_shared<RealmApp>(app_id, config);
}

void RealmApp::login_with_credentials(const std::shared_ptr<AppCredentials> credentials,
                                      int timeout,
                                      std::function<void(std::shared_ptr<SyncUser>, GenericNetworkError)> completion_block) {
    // construct the route
    std::stringstream route;
    route << m_auth_route << "/providers/" << provider_type_from_enum(credentials->m_provider) << "/login";

    auto handler = new std::function<void(std::vector<char> data, GenericNetworkError error)>([&](std::vector<char> data, GenericNetworkError error) {
        // if there is a already an error code, pass the error upstream
        if (error.code) {
            return completion_block(nullptr, error);
        }
        auto j = nlohmann::json::parse(data.begin(), data.end());

        realm::SyncUserIdentifier identifier {
            j["user_id"].get<std::string>(),
            m_auth_route
        };

        auto sync_user = realm::SyncManager::shared().get_user(identifier,
                                                               j["refresh_token"].get<std::string>(),
                                                               j["access_token"].get<std::string>());

        // TODO: generate real error
        if (!sync_user) {
            return completion_block(nullptr, GenericNetworkError { 4 });
        }

        std::stringstream profile_route;
        profile_route << m_base_route << "/auth/profile";

        std::stringstream bearer;
        bearer << "Bearer" << " " << sync_user->access_token();

        std::cout<<bearer.str()<<std::endl;

        GenericNetworkTransport::get()->send_request_to_server(profile_route.str(),
                                                               "GET",
                                                               {
            { "Content-Type", "application/json;charset=utf-8" },
            { "Accept", "application/json" },
            { "Authorization", bearer.str() }
        },
                                                               std::vector<char>(),
                                                               timeout,
                                                               std::function<void(std::vector<char> data, GenericNetworkError error)>([&](std::vector<char> data, GenericNetworkError error) {
            std::cout<<std::string(data.begin(), data.end())<<std::endl;
            j = nlohmann::json::parse(data);

            std::vector<SyncUserIdentity> identities;
            auto identities_json = j["identities"];

            for (size_t i = 0; i < identities_json.size(); i++)
            {
                auto identity_json = identities_json[i];
                identities.push_back(SyncUserIdentity(identity_json["id"], identity_json["provider_type"]));
            }

            sync_user->update_identities(identities);

            auto profile_data = j["data"];

            SyncUserProfile(WRAP_JSON_OPT(profile_data, "name", std::string),
                            WRAP_JSON_OPT(profile_data, "email", std::string),
                            WRAP_JSON_OPT(profile_data, "picture_url", std::string),
                            WRAP_JSON_OPT(profile_data, "first_name", std::string),
                            WRAP_JSON_OPT(profile_data, "last_name", std::string),
                            WRAP_JSON_OPT(profile_data, "gender", std::string),
                            WRAP_JSON_OPT(profile_data, "birthday", std::string),
                            WRAP_JSON_OPT(profile_data, "min_age", std::string),
                            WRAP_JSON_OPT(profile_data, "max_age", std::string));

            return completion_block(sync_user, error);
        }));
    });

    std::map<std::string, std::string> headers = {
        { "Content-Type", "application/json;charset=utf-8" },
        { "Accept", "application/json" }
    };

    GenericNetworkTransport::get()->send_request_to_server(route.str(),
                                                           "POST",
                                                           headers,
                                                           credentials->serialize(),
                                                           timeout,
                                                           *handler);
}

}
