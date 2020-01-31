#include "app.hpp"
#include <sstream>
#include "sync_manager.hpp"
#include "generic_network_transport.hpp"
#include "app_credentials.hpp"
#include "../external/json/json.hpp"

namespace realm {

std::shared_ptr<RealmApp> RealmApp::app(const std::string app_id)
{
    return std::make_shared<RealmApp>(app_id);
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

        realm::SyncUserIdentifier identity {
            j["user_id"].get<std::string>(),
            m_auth_route
        };

        auto sync_user = realm::SyncManager::shared().get_user(identity,
                                                               j["access_token"].get<std::string>(),
                                                               j["refresh_token"].get<std::string>());

        // TODO: generate real error
        if (!sync_user) {
            return completion_block(nullptr, GenericNetworkError { 4 });
        }

        return completion_block(sync_user, error);
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
