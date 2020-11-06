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

#include "sync/app.hpp"

#include "realm/util/base64.hpp"
#include "realm/util/uri.hpp"

#include "thread_safe_reference.hpp"
#include "sync/app_credentials.hpp"
#include "sync/app_utils.hpp"
#include "sync/generic_network_transport.hpp"
#include "sync/sync_manager.hpp"
#include "sync/remote_mongo_client.hpp"

#include "sync/impl/sync_client.hpp"
#include "sync/impl/sync_file.hpp"

#include <json.hpp>
#include <string>

namespace realm {
namespace app {

using util::Optional;

// MARK: - Helpers
// wrap an optional json key into the Optional type
template <typename T>
Optional<T> get_optional(const nlohmann::json& json, const std::string& key)
{
    auto it = json.find(key);
    return it != json.end() ? Optional<T>(it->get<T>()) : realm::util::none;
}

enum class RequestTokenType {
    NoAuth,
    AccessToken,
    RefreshToken
};

// generate the request headers for a HTTP call, by default it will generate headers with a refresh token if a user is passed
static std::map<std::string, std::string> get_request_headers(util::Optional<SyncUser> with_user_authorization = util::none,
                                                              RequestTokenType token_type = RequestTokenType::RefreshToken)
{
    std::map<std::string, std::string> headers {
        { "Content-Type", "application/json;charset=utf-8" },
        { "Accept", "application/json" }
    };

    if (with_user_authorization) {
        switch (token_type) {
            case RequestTokenType::NoAuth:
                break;
            case RequestTokenType::AccessToken:
                headers.insert({ "Authorization",
                    util::format("Bearer %1", with_user_authorization->access_token)
                });
                break;
            case RequestTokenType::RefreshToken:
                headers.insert({ "Authorization",
                    util::format("Bearer %1", with_user_authorization->refresh_token)
                });
                break;
        }
    }
    return headers;
}

const static std::string default_base_url = "https://realm.mongodb.com";
const static std::string base_path = "/api/client/v2.0";
const static std::string app_path = "/app";
const static std::string auth_path = "/auth";
const static std::string sync_path = "/realm-sync";
const static uint64_t    default_timeout_ms = 60000;
const static std::string username_password_provider_key = "local-userpass";
const static std::string user_api_key_provider_key_path = "api_keys";
static std::unordered_map<std::string, std::shared_ptr<App>> s_apps_cache;
std::mutex s_apps_mutex;

SharedApp App::get_shared_app(const Config& config, const SyncClientConfig& sync_client_config)
{
    std::lock_guard<std::mutex> lock(s_apps_mutex);
    auto& app = s_apps_cache[config.app_id];
    if (!app) {
        app = std::make_shared<App>();
        app->configure(config, sync_client_config);
        auto realm = app->m_sync_manager->open_app_realm();
        realm.write([&realm, &app] { realm.add_object(*app); });
    }
    return app;
}

std::shared_ptr<App> App::get_cached_app(const std::string& app_id)
{
    std::lock_guard<std::mutex> lock(s_apps_mutex);
    if (auto it = s_apps_cache.find(app_id); it != s_apps_cache.end()) {
        return it->second;
    }

    return nullptr;
}

void App::clear_cached_apps()
{
    std::lock_guard<std::mutex> lock(s_apps_mutex);
    s_apps_cache.clear();
}

//App::App(const Config& config)
//: m_config(std::move(config))
//, m_base_url(config.base_url.value_or(default_base_url))
//, m_base_route(m_base_url + base_path)
//, m_app_route(m_base_route + app_path + "/" + config.app_id)
//, m_auth_route(m_app_route + auth_path)
//, m_request_timeout_ms(config.default_request_timeout_ms.value_or(default_timeout_ms))
//{
//    REALM_ASSERT(m_config.transport_generator);
//
//    if (m_config.platform.empty()) {
//        throw std::runtime_error("You must specify the Platform in App::Config");
//    }
//
//    if (m_config.platform_version.empty()) {
//        throw std::runtime_error("You must specify the Platform Version in App::Config");
//    }
//
//    if (m_config.sdk_version.empty()) {
//        throw std::runtime_error("You must specify the SDK Version in App::Config");
//    }
//
//    // change the scheme in the base url to ws from http to satisfy the sync client
//    auto sync_route = m_app_route + sync_path;
//    size_t uri_scheme_start = sync_route.find("http");
//    if (uri_scheme_start == 0)
//        sync_route.replace(uri_scheme_start, 4, "ws");
//
//    m_sync_manager = std::make_shared<SyncManager>();
//}

void App::configure(const Config& config, const SyncClientConfig& sync_client_config)
{
    m_config = (std::move(config));
    m_base_url = (config.base_url.value_or(default_base_url));
    m_base_route = (m_base_url + base_path);
    m_app_route = (m_base_route + app_path + "/" + config.app_id);
    m_auth_route = (m_app_route + auth_path);
    m_request_timeout_ms = (config.default_request_timeout_ms.value_or(default_timeout_ms));
    REALM_ASSERT(m_config.transport_generator);

    if (m_config.platform.empty()) {
        throw std::runtime_error("You must specify the Platform in App::Config");
    }

    if (m_config.platform_version.empty()) {
        throw std::runtime_error("You must specify the Platform Version in App::Config");
    }

    if (m_config.sdk_version.empty()) {
        throw std::runtime_error("You must specify the SDK Version in App::Config");
    }

    // change the scheme in the base url to ws from http to satisfy the sync client
    auto sync_route = m_app_route + sync_path;
    size_t uri_scheme_start = sync_route.find("http");
    if (uri_scheme_start == 0)
        sync_route.replace(uri_scheme_start, 4, "ws");

    m_sync_manager = std::make_shared<SyncManager>();
    m_sync_manager->configure(shared_from_this(), sync_route, sync_client_config);
    auto realm = m_sync_manager->open_app_realm();
    if (auto app_metadata = realm.get_objects<Metadata>().first()) {
        if (app_metadata) {
            const auto metadata = *app_metadata;
            m_base_route = metadata.hostname + base_path;
            std::string this_app_path = app_path + "/" + m_config.app_id;
            m_app_route = m_base_route + this_app_path;
            m_auth_route = m_app_route + auth_path;
            m_sync_manager->set_sync_route(metadata.ws_hostname + base_path + this_app_path + sync_path);
        }
    }
}

static void handle_default_response(const Response& response,
                                    std::function<void (Optional<AppError>)> completion_block)
{
    if (auto error = check_for_errors(response)) {
        return completion_block(error);
    } else {
        return completion_block({});
    }
};

//MARK: - Template specializations

template<>
App::UsernamePasswordProviderClient App::provider_client<App::UsernamePasswordProviderClient>()
{
    return App::UsernamePasswordProviderClient(shared_from_this());
}

template<>
App::UserAPIKeyProviderClient App::provider_client<App::UserAPIKeyProviderClient>()
{
    return App::UserAPIKeyProviderClient(*this);
}

// MARK: - UsernamePasswordProviderClient

void App::UsernamePasswordProviderClient::register_email(const std::string &email,
                                                         const std::string &password,
                                                         std::function<void (Optional<AppError>)> completion_block)
{
    REALM_ASSERT(m_parent);
    std::string route = util::format("%1/providers/%2/register", m_parent->m_auth_route, username_password_provider_key);

    auto handler = [completion_block](const Response& response) {
        handle_default_response(response, completion_block);
    };

    nlohmann::json body = {
        { "email", email },
        { "password", password }
    };

    m_parent->do_request(Request {
        HttpMethod::post,
        route,
        m_parent->m_request_timeout_ms,
        get_request_headers(),
        body.dump()
    }, handler);
}

void App::UsernamePasswordProviderClient::confirm_user(const std::string& token,
                                                             const std::string& token_id,
                                                             std::function<void(Optional<AppError>)> completion_block)
{
    REALM_ASSERT(m_parent);
    std::string route = util::format("%1/providers/%2/confirm", m_parent->m_auth_route, username_password_provider_key);

    auto handler = [completion_block](const Response& response) {
        handle_default_response(response, completion_block);
    };

    nlohmann::json body = {
        { "token", token },
        { "tokenId", token_id }
    };

    m_parent->do_request(Request {
        HttpMethod::post,
        route,
        m_parent->m_request_timeout_ms,
        get_request_headers(),
        body.dump()
    }, handler);
}

void App::UsernamePasswordProviderClient::resend_confirmation_email(const std::string& email,
                                                                    std::function<void(Optional<AppError>)> completion_block)
{
    REALM_ASSERT(m_parent);
    std::string route = util::format("%1/providers/%2/confirm/send", m_parent->m_auth_route, username_password_provider_key);

    auto handler = [completion_block](const Response& response) {
        handle_default_response(response, completion_block);
    };

    nlohmann::json body {
        { "email", email }
    };

    m_parent->do_request(Request {
        HttpMethod::post,
        route,
        m_parent->m_request_timeout_ms,
        get_request_headers(),
        body.dump()
    }, handler);
}

void App::UsernamePasswordProviderClient::retry_custom_confirmation(const std::string& email,
                                                                    std::function<void(Optional<AppError>)> completion_block)
{
    REALM_ASSERT(m_parent);
    std::string route = util::format("%1/providers/%2/confirm/call", m_parent->m_auth_route, username_password_provider_key);

    auto handler = [completion_block](const Response& response) {
        handle_default_response(response, completion_block);
    };

    nlohmann::json body {
        { "email", email }
    };

    m_parent->do_request(Request {
        HttpMethod::post,
        route,
        m_parent->m_request_timeout_ms,
        get_request_headers(),
        body.dump()
    }, handler);
}

void App::UsernamePasswordProviderClient::send_reset_password_email(const std::string& email,
                                                                    std::function<void(Optional<AppError>)> completion_block)
{
    REALM_ASSERT(m_parent);
    std::string route = util::format("%1/providers/%2/reset/send", m_parent->m_auth_route, username_password_provider_key);

    auto handler = [completion_block](const Response& response) {
        handle_default_response(response, completion_block);
    };

    nlohmann::json body = {
        { "email", email }
    };

    m_parent->do_request(Request {
        HttpMethod::post,
        route,
        m_parent->m_request_timeout_ms,
        get_request_headers(),
        body.dump()
    }, handler);
}

void App::UsernamePasswordProviderClient::reset_password(const std::string& password,
                                                         const std::string& token,
                                                         const std::string& token_id,
                                                         std::function<void(Optional<AppError>)> completion_block)
{
    REALM_ASSERT(m_parent);
    std::string route = util::format("%1/providers/%2/reset", m_parent->m_auth_route, username_password_provider_key);

    auto handler = [completion_block](const Response& response) {
        handle_default_response(response, completion_block);
    };

    nlohmann::json body = {
        { "password", password },
        { "token", token },
        { "tokenId", token_id }
    };

    m_parent->do_request(Request {
        HttpMethod::post,
        route,
        m_parent->m_request_timeout_ms,
        get_request_headers(),
        body.dump()
    }, handler);
}

void App::UsernamePasswordProviderClient::call_reset_password_function(const std::string& email,
                                                                       const std::string& password,
                                                                       const bson::BsonArray& args,
                                                                       std::function<void(Optional<AppError>)> completion_block)
{
    REALM_ASSERT(m_parent);
    std::string route = util::format("%1/providers/%2/reset/call", m_parent->m_auth_route, username_password_provider_key);

    auto handler = [completion_block](const Response& response) {
        handle_default_response(response, completion_block);
    };

    bson::BsonDocument arg = {
        { "email", email },
        { "password", password },
        { "arguments", args }
    };

    std::stringstream body;
    body << bson::Bson(arg);

    m_parent->do_request(Request {
        HttpMethod::post,
        route,
        m_parent->m_request_timeout_ms,
        get_request_headers(),
        body.str()
    }, handler);
}

// MARK: - UserAPIKeyProviderClient

std::string App::UserAPIKeyProviderClient::url_for_path(const std::string &path="") const
{
    if (!path.empty()) {
        return m_auth_request_client.url_for_path(util::format("%1/%2/%3",
                                                               auth_path,
                                                               user_api_key_provider_key_path,
                                                               path));
    }

    return m_auth_request_client.url_for_path(util::format("%1/%2",
                                                           auth_path,
                                                           user_api_key_provider_key_path));
}

void App::UserAPIKeyProviderClient::create_api_key(const std::string &name,
                                                   const SyncUser& user,
                                                   std::function<void (UserAPIKey, Optional<AppError>)> completion_block)
{
    std::string route = url_for_path();

    nlohmann::json body = {
        { "name", name }
    };
    Request req;
    req.method = HttpMethod::post;
    req.url = route;
    req.body = body.dump();
    req.uses_refresh_token = true;

    m_auth_request_client.do_authenticated_request(req, user, [completion_block](const Response& response) {

        if (auto error = check_for_errors(response)) {
            return completion_block({}, error);
        }

        nlohmann::json json;
        try {
            json = nlohmann::json::parse(response.body);
        } catch (const std::exception& e) {
            return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
        }

        try {
            auto user_api_key = App::UserAPIKey {
                    ObjectId(value_from_json<std::string>(json, "_id").c_str()),
                    get_optional<std::string>(json, "key"),
                    value_from_json<std::string>(json, "name"),
                    value_from_json<bool>(json, "disabled")
                };
            return completion_block(user_api_key, {});
        } catch (const std::exception& e) {
            return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
        }
    });
}

void App::UserAPIKeyProviderClient::fetch_api_key(const realm::ObjectId& id,
                                                  const SyncUser& user,
                                                  std::function<void (UserAPIKey, Optional<AppError>)> completion_block)
{
    std::string route = url_for_path(id.to_string());

    Request req;
    req.method = HttpMethod::get;
    req.url = route;
    req.uses_refresh_token = true;

    m_auth_request_client.do_authenticated_request(req, user, [completion_block](const Response& response) {

        if (auto error = check_for_errors(response)) {
            return completion_block({}, error);
        }

        nlohmann::json json;
        try {
            json = nlohmann::json::parse(response.body);
        } catch (const std::exception& e) {
            return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
        }

        try {
            auto user_api_key = App::UserAPIKey {
                    ObjectId(value_from_json<std::string>(json, "_id").c_str()),
                    get_optional<std::string>(json, "key"),
                    value_from_json<std::string>(json, "name"),
                    value_from_json<bool>(json, "disabled")
                };
            return completion_block(user_api_key, {});
        } catch (const std::exception& e) {
            return completion_block({}, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
        }
    });
}

void App::UserAPIKeyProviderClient::fetch_api_keys(const SyncUser& user,
                                                   std::function<void(std::vector<UserAPIKey>, Optional<AppError>)> completion_block)
{
    std::string route = url_for_path();

    Request req;
    req.method = HttpMethod::get;
    req.url = route;
    req.uses_refresh_token = true;

    m_auth_request_client.do_authenticated_request(req, user, [completion_block](const Response& response) {
        if (auto error = check_for_errors(response)) {
            return completion_block(std::vector<UserAPIKey>(), error);
        }

        nlohmann::json json;
        try {
            json = nlohmann::json::parse(response.body);
        } catch (const std::exception& e) {
            return completion_block(std::vector<UserAPIKey>(), AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
        }

        try {
            auto api_key_array = std::vector<UserAPIKey>();
            auto json_array = json.get<std::vector<nlohmann::json>>();
            for (nlohmann::json& api_key_json : json_array) {
                api_key_array.push_back(
                    App::UserAPIKey {
                        ObjectId(value_from_json<std::string>(api_key_json, "_id").c_str()),
                        get_optional<std::string>(api_key_json, "key"),
                        value_from_json<std::string>(api_key_json, "name"),
                        value_from_json<bool>(api_key_json, "disabled")
                });
            }
            return completion_block(api_key_array, {});
        } catch (const std::exception& e) {
            return completion_block(std::vector<UserAPIKey>(), AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
        }
    });
}


void App::UserAPIKeyProviderClient::delete_api_key(const realm::ObjectId& id,
                                                   const SyncUser& user,
                                                   std::function<void(util::Optional<AppError>)> completion_block)
{
    std::string route = url_for_path(id.to_string());

    Request req;
    req.method = HttpMethod::del;
    req.url = route;
    req.uses_refresh_token = true;

    m_auth_request_client.do_authenticated_request(req, user, [completion_block](const Response& response) {
        if (auto error = check_for_errors(response)) {
            return completion_block(error);
        } else {
            return completion_block({});
        }
    });
}

void App::UserAPIKeyProviderClient::enable_api_key(const realm::ObjectId& id,
                                                   const SyncUser& user,
                                                   std::function<void(Optional<AppError> error)> completion_block)
{
    std::string route = url_for_path(util::format("%1/enable", id.to_string()));

    Request req;
    req.method = HttpMethod::put;
    req.url = route;
    req.uses_refresh_token = true;

    m_auth_request_client.do_authenticated_request(req, user, [completion_block](const Response& response) {
        if (auto error = check_for_errors(response)) {
            return completion_block(error);
        } else {
            return completion_block({});
        }
    });
}

void App::UserAPIKeyProviderClient::disable_api_key(const realm::ObjectId& id,
                                                    const SyncUser& user,
                                                    std::function<void(Optional<AppError> error)> completion_block)
{
    std::string route = url_for_path(util::format("%1/disable", id.to_string()));

    Request req;
    req.method = HttpMethod::put;
    req.url = route;
    req.uses_refresh_token = true;

    m_auth_request_client.do_authenticated_request(req, user, [completion_block](const Response& response) {
        if (auto error = check_for_errors(response)) {
            return completion_block(error);
        } else {
            return completion_block({});
        }
    });
}

// MARK: - App

//util::Optional<SyncUser> App::current_user() const
//{
//    return util::none;//m_sync_manager->get_current_user();
//}

std::vector<SyncUser> App::all_users() const
{
    auto realm = m_sync_manager->open_app_realm();
    auto results = realm.get_objects<SyncUser>();
    std::vector<SyncUser> users;
    for (size_t i = 0; i < results.size(); i++) {
        users.push_back(results.get(i));
    }
    return users;
}

void App::get_profile(SyncUser user,
                      std::function<void(util::Optional<SyncUser>,
                                         util::Optional<AppError>)>&& completion_block)
{
    std::string profile_route = util::format("%1/auth/profile", m_base_route);

    Request req;
    req.method = HttpMethod::get;
    req.url = profile_route;
    req.timeout_ms = m_request_timeout_ms;
    req.uses_refresh_token = false;
    auto sm = m_sync_manager;
    do_authenticated_request(req, user, [completion_block = std::move(completion_block), sm, &user, this](Response profile_response) mutable {
        if (auto error = check_for_errors(profile_response)) {
            return completion_block(util::none, error);
        }

        nlohmann::json profile_json;
        nlohmann::json identities_json;
        try {
            profile_json = nlohmann::json::parse(profile_response.body);
            identities_json = value_from_json<nlohmann::json>(profile_json, "identities");
        } catch (const std::domain_error& e) {
            return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
        }

        try {
            realm().write([this, &user, &profile_json, &identities_json] {
                auto profile_data = value_from_json<nlohmann::json>(profile_json, "data");
                user.profile.name = get_optional<std::string>(profile_data, "name");
                user.profile.email = get_optional<std::string>(profile_data, "email");
                user.profile.picture_url = get_optional<std::string>(profile_data, "picture_url");
                user.profile.first_name = get_optional<std::string>(profile_data, "first_name");
                user.profile.last_name = get_optional<std::string>(profile_data, "last_name");
                user.profile.gender = get_optional<std::string>(profile_data, "gender");
                user.profile.birthday = get_optional<std::string>(profile_data, "birthday");
                user.profile.min_age = get_optional<std::string>(profile_data, "min_age");
                user.profile.max_age = get_optional<std::string>(profile_data, "max_age");
                current_user = &user;
                for (size_t i = 0; i < identities_json.size(); i++)
                {
                    auto identity_json = identities_json[i];
                    auto identity = std::find_if(user.identities.begin(),
                                 user.identities.end(), [&identity_json](auto& identity) {
                        return identity.id == value_from_json<std::string>(identity_json, "id");
                    });
                    if (identity == user.identities.end()) {
                        auto identity = SyncUserIdentity();
                        identity.provider_type = value_from_json<std::string>(identity_json, "provider_type");
                        user.identities.push_back(identity);
                    }
                }
            });
        } catch (const AppError& err) {
            return completion_block(util::none, err);
        }

        return completion_block(user, {});
    });
}

void App::attach_auth_options(bson::BsonDocument& body)
{
    bson::BsonDocument options;

    if (m_config.local_app_version) {
        options["appVersion"] = *m_config.local_app_version;
    }

    options["appId"] = m_config.app_id;
    options["platform"] = m_config.platform;
    options["platformVersion"] = m_config.platform_version;
    options["sdkVersion"] = m_config.sdk_version;

    body["options"] = bson::BsonDocument({{"device", options}});
}

void App::log_in_with_credentials(const AppCredentials& credentials,
                                  Optional<SyncUser> linking_user,
                                  std::function<void(Optional<SyncUser>, Optional<AppError>)> completion_block)
{
    // construct the route
    std::string route = util::format("%1/providers/%2/login%3",
                                     m_auth_route,
                                     credentials.provider_as_string(),
                                     linking_user ? "?link=true" : "");

    bson::Bson credentials_as_bson = bson::parse(credentials.serialize_as_json());
    bson::BsonDocument body = static_cast<bson::BsonDocument>(credentials_as_bson);
    attach_auth_options(body);

    std::stringstream s;
    s << bson::Bson(body);

    // if we try logging in with an anonymous user while there
    // is already an anonymous session active, reuse it
    if (credentials.provider() == AuthProvider::ANONYMOUS) {
        auto users = all_users();
        for (size_t i = 0; i < users.size(); i++) {
            auto realm = m_sync_manager->open_app_realm();
//            auto user = SyncUser(realm, all_users().get(i));
//            if (user.provider_type() == credentials.provider_as_string() && user.is_logged_in()) {
//                completion_block(switch_user(user), util::none);
//                return;
//            }
        }
    }

    do_request({
        HttpMethod::post,
        route,
        m_request_timeout_ms,
        get_request_headers(linking_user, RequestTokenType::AccessToken),
        s.str()
    }, [completion_block = std::move(completion_block), credentials, linking_user, this](const Response& response) mutable {
        if (auto error = check_for_errors(response)) {
            return completion_block(util::none, error);
        }

        nlohmann::json json;
        try {
            json = nlohmann::json::parse(response.body);
        } catch (const std::exception& e) {
            return completion_block(util::none, AppError(make_error_code(JSONErrorCode::malformed_json), e.what()));
        }

        SyncUser user;
        user.app_id = m_config.app_id;
        try {
            if (linking_user) {
                // if there is a user to link with, re-set the access token
                // and assign the user for return
                user = *linking_user;
                realm().write([&user, &json]{
                    user.access_token = util::some<std::string>(value_from_json<std::string>(json, "access_token"));
                });
            } else {
                // else if there is an existing user for the returned `id`
                // (e.g., if a user was logged out but not removed), fetch
                // that user and set its state to logged in.
                // else create a new user
                user.id = value_from_json<std::string>(json, "user_id");
                realm().write([this, &json, &user] {
                    if (auto existing_user = realm().get_object<SyncUser>(user.id)) {
                        (*existing_user).set_logged_in_state(value_from_json<std::string>(json, "refresh_token"),
                                                             value_from_json<std::string>(json, "access_token"),
                                                             value_from_json<std::string>(json, "device_id"));
                        user = *existing_user;
                    } else {
                        realm().add_object(user);
                        user.set_logged_in_state(value_from_json<std::string>(json, "refresh_token"),
                                                 value_from_json<std::string>(json, "access_token"),
                                                 value_from_json<std::string>(json, "device_id"));
                    }
                });
            }
        } catch (const AppError& err) {
            return completion_block(util::none, err);
        }

        App::get_profile(user, std::move(completion_block));
    });
}

void App::log_in_with_credentials(const AppCredentials& credentials,
                                  std::function<void(util::Optional<SyncUser>, Optional<AppError>)> completion_block)
{
    App::log_in_with_credentials(credentials, util::none, std::move(completion_block));
}

void App::log_out(SyncUser user, std::function<void (Optional<AppError>)> completion_block)
{
    if (user.state != SyncUser::State::LoggedIn) {
        return completion_block(util::none);
    }

    auto handler = [completion_block, user](const Response& response) {
        if (auto error = check_for_errors(response)) {
            return completion_block(error);
        }
        return completion_block(util::none);
    };

    auto refresh_token = user.refresh_token;
    realm().write([&user] {
        user.log_out();
    });

    std::string route = util::format("%1/auth/session", m_base_route);

    Request req;
    req.method = HttpMethod::del;
    req.url = route;
    req.timeout_ms = m_request_timeout_ms;
    req.uses_refresh_token = true;
    req.headers = get_request_headers();
    req.headers.insert({ "Authorization",
        util::format("Bearer %1", refresh_token)
    });

    do_request(req, [completion_block, req](Response response) {
        if (auto error = check_for_errors(response)) {
            // We do not care about handling auth errors on log out
            completion_block(error);
        } else {
            completion_block(util::none);
        }
    });
}

void App::log_out(std::function<void (Optional<AppError>)> completion_block) {
    if (auto user = current_user) {
        return log_out(*user, completion_block);
    }

    completion_block(AppError(make_client_error_code(ClientErrorCode::user_not_logged_in),
                              "unknown"));
}

SyncUser App::switch_user(const SyncUser& user)
{
    if (user.state != SyncUser::State::LoggedIn) {
        throw AppError(make_client_error_code(ClientErrorCode::user_not_logged_in),
                       "User is no longer valid or is logged out");
    }

//    auto users = m_sync_manager->all_users();
//    auto it = std::find(users.begin(),
//                        users.end(),
//                        user);
//
//    if (it == users.end()) {
//        throw AppError(make_client_error_code(ClientErrorCode::user_not_found),
//                       "User does not exist");
//    }

//    m_sync_manager->set_current_user(*user.id);
    return *current_user;
}

void App::remove_user(SyncUser user,
                      std::function<void(Optional<AppError>)> completion_block)
{
    if (user.state == SyncUser::State::Removed) {
        return completion_block(AppError(make_client_error_code(ClientErrorCode::user_not_found),
                                         "User has already been removed"));
    }

//    auto users = m_sync_manager->all_users();

//    auto it = std::find(users.begin(),
//                        users.end(),
//                        user);
//
//    if (it == users.end()) {
//        return completion_block(AppError(make_client_error_code(ClientErrorCode::user_not_found),
//                                         "No user has been found"));
//    }

    if (user.is_logged_in()) {
        log_out(user, [ completion_block](const Optional<AppError>& error) {
//            m_sync_manager->remove_user(*user.id);
            return completion_block(error);
        });
    } else {
//        m_sync_manager->remove_user(*user.id);
        return completion_block({});
    }
}


void App::link_user(const SyncUser& user,
                    const AppCredentials& credentials,
                    std::function<void(util::Optional<SyncUser>, Optional<AppError>)> completion_block)
{
    if (user.state != SyncUser::State::LoggedIn) {
        return completion_block(util::none, AppError(make_client_error_code(ClientErrorCode::user_not_found),
                                                     "The specified user is not logged in"));
    }

//    auto users = m_sync_manager->all_users();
//
//    auto it = std::find(users.begin(),
//                        users.end(),
//                        user);
//
//    if (it == users.end()) {
//        return completion_block(util::none, AppError(make_client_error_code(ClientErrorCode::user_not_found),
//                                                     "The specified user was not found"));
//    }

    App::log_in_with_credentials(credentials, user, completion_block);
}

void App::refresh_custom_data(SyncUser& sync_user,
                              std::function<void(Optional<AppError>)> completion_block)
{
    refresh_access_token(sync_user, completion_block);
}

std::string App::url_for_path(const std::string& path="") const
{
    return util::format("%1%2", m_base_route, path);
}

// FIXME: This passes back the response to bubble up any potential errors, making this somewhat leaky
void App::init_app_metadata(std::function<void (util::Optional<AppError>, util::Optional<Response>)> completion_block)
{
    if (m_metadata) {
        return completion_block(util::none, util::none);
    }

    std::string route = util::format("%1/location",
                                     m_app_route);

    Request req;
    req.method = HttpMethod::get;
    req.url = route;
    req.timeout_ms = m_request_timeout_ms;

    m_config.transport_generator()->send_request_to_server(req, [this, completion_block = std::move(completion_block)](const Response& response) mutable {
        nlohmann::json json;
        try {
            json = nlohmann::json::parse(response.body);
        } catch (const std::exception& e) {
            return completion_block(AppError(make_error_code(JSONErrorCode::malformed_json), e.what()),
                                    response);
        }

        try {
//            if (!realm().is_in_write_transaction()) {
//                realm().begin_transaction();
//            }
            realm().write([this, &json] () mutable {
                auto metadata = Metadata();
                metadata.deployment_model = value_from_json<std::string>(json, "deployment_model");
                metadata.location = value_from_json<std::string>(json, "location");
                metadata.hostname = value_from_json<std::string>(json, "hostname");
                metadata.ws_hostname = value_from_json<std::string>(json, "ws_hostname");
                this->m_metadata = &realm().add_object(metadata);
            });

            auto metadata = *m_metadata;
            m_base_route = metadata.hostname + base_path;
            std::string this_app_path = app_path + "/" + m_config.app_id;
            m_app_route = m_base_route + this_app_path;
            m_auth_route = m_app_route + auth_path;
            m_sync_manager->set_sync_route(metadata.ws_hostname + base_path + this_app_path + sync_path);
        } catch (const AppError& err) {
            return completion_block(err, response);
        }

        completion_block(util::none, util::none);
    });
}

void App::do_request(Request request,
                     std::function<void (const Response&)> completion_block)
{
    request.timeout_ms = default_timeout_ms;

    // if we do not have metadata yet, we need to initialize it
    if (!m_metadata) {
        init_app_metadata([completion_block = std::move(completion_block), request, this](const util::Optional<AppError> error, const util::Optional<Response> response) mutable {
            if (error) {
                return completion_block(*response);
            }

            // if this is the first time we have received app metadata, the
            // original request will not have the correct URL hostname for
            // non global deployments.
            auto app_metadata = *m_metadata;
            if (app_metadata.deployment_model != "GLOBAL"
                && request.url.rfind(m_base_url, 0) != std::string::npos) {
                request.url.replace(0, m_base_url.size(), app_metadata.hostname);
            }

            m_config.transport_generator()->send_request_to_server(request, std::move(completion_block));
        });
    } else {
        m_config.transport_generator()->send_request_to_server(request, std::move(completion_block));
    }
}

void App::do_authenticated_request(Request request,
                                   SyncUser sync_user,
                                   std::function<void (const Response&)> completion_block)
{
    request.headers = get_request_headers(sync_user,
                                          request.uses_refresh_token ?
                                          RequestTokenType::RefreshToken : RequestTokenType::AccessToken);

    do_request(request, [completion_block = std::move(completion_block), request, sync_user, this](Response response) mutable {
        if (auto error = check_for_errors(response)) {
            App::handle_auth_failure(error.value(), response, request, sync_user, std::move(completion_block));
        } else {
            completion_block(response);
        }
    });
}

void App::handle_auth_failure(const AppError& error,
                              const Response& response,
                              Request request,
                              SyncUser sync_user,
                              std::function<void (Response)> completion_block)
{

    // Only handle auth failures
    if (error.is_http_error() && error.error_code.value() == 401) {
        if (request.uses_refresh_token) {
            if (sync_user.is_logged_in()) {
                sync_user.log_out();
            }
            completion_block(response);
            return;
        }

        App::refresh_access_token(sync_user, [this,
                                              request,
                                              completion_block = std::move(completion_block),
                                              response,
                                              sync_user](const Optional<AppError>& error) mutable {
                     if (!error) {
                         // assign the new access_token to the auth header
                         Request newRequest = request;
                         newRequest.headers = get_request_headers(sync_user, RequestTokenType::AccessToken);
                         m_config.transport_generator()->send_request_to_server(newRequest, std::move(completion_block));
                     } else {
                         // pass the error back up the chain
                         completion_block(response);
                     }
                 });
    } else {
        completion_block(response);
    }
}

/// MARK: - refresh access token
void App::refresh_access_token(SyncUser& sync_user,
                               std::function<void(Optional<AppError>)> completion_block)
{
    if (!sync_user.is_logged_in()) {
        completion_block(AppError(make_client_error_code(ClientErrorCode::user_not_logged_in),
                                  "The user is not logged in"));
        return;
    }

    std::string route = util::format("%1/auth/session", m_base_route);

    do_request(Request {
        HttpMethod::post,
        route,
        m_request_timeout_ms,
        get_request_headers(sync_user, RequestTokenType::RefreshToken)
    }, [completion_block = std::move(completion_block), &sync_user](const Response& response) mutable {
        if (auto error = check_for_errors(response)) {
            return completion_block(error);
        }

        try {
            nlohmann::json json = nlohmann::json::parse(response.body);
            sync_user.realm().write([&json, &sync_user] {
                sync_user.access_token = util::some<std::string>(value_from_json<std::string>(json, "access_token"));
            });
        } catch (const AppError& err) {
            return completion_block(err);
        }

        return completion_block(util::none);
    });
}

std::string App::function_call_url_path() const {
    return util::format("%1/app/%2/functions/call", m_base_route, m_config.app_id);
}
void App::call_function(SyncUser user,
                        const std::string& name,
                        const bson::BsonArray& args_bson,
                        const util::Optional<std::string>& service_name,
                        std::function<void (util::Optional<AppError>,
                                            util::Optional<bson::Bson>)> completion_block)
{
    bson::BsonDocument args {
        { "arguments", args_bson },
        { "name", name }
    };

    if (service_name) {
        args["service"] = *service_name;
    }

    do_authenticated_request(Request{
        HttpMethod::post,
        function_call_url_path(),
        m_request_timeout_ms,
        {},
        bson::Bson(args).toJson(),
        false
    },
    user,
    [completion_block](const Response& response) {
        if (auto error = check_for_errors(response)) {
            return completion_block(error, util::none);
        }
        completion_block(util::none, util::Optional<bson::Bson>(bson::parse(response.body)));
    });
}

void App::call_function(SyncUser user,
                        const std::string& name,
                        const bson::BsonArray& args_bson,
                        std::function<void (util::Optional<AppError>,
                                            util::Optional<bson::Bson>)> completion_block)
{
    call_function(user,
                  name,
                  args_bson,
                  util::none,
                  completion_block);
}

void App::call_function(const std::string& name,
                        const bson::BsonArray& args_bson,
                        const util::Optional<std::string>& service_name,
                        std::function<void (util::Optional<AppError>,
                                            util::Optional<bson::Bson>)> completion_block)
{
    call_function(*current_user,
                  name,
                  args_bson,
                  service_name,
                  completion_block);
}

void App::call_function(const std::string& name,
                        const bson::BsonArray& args_bson,
                        std::function<void (util::Optional<AppError>,
                                            util::Optional<bson::Bson>)> completion_block)
{
    call_function(*current_user,
                  name,
                  args_bson,
                  completion_block);
}

Request App::make_streaming_request(SyncUser user,
                                    const std::string &name,
                                    const bson::BsonArray &args_bson,
                                    const util::Optional<std::string> &service_name) const {
    auto args = bson::BsonDocument{
        {"arguments", args_bson},
        {"name", name},
    };
    if (service_name) {
        args["service"] = *service_name;
    }
    const auto args_json = bson::Bson(args).toJson();

    auto args_base64 = std::string(util::base64_encoded_size(args_json.size()), '\0');
    util::base64_encode(args_json.data(), args_json.size(), args_base64.data(), args_base64.size());

    auto url = function_call_url_path() + "?baas_request=" + util::uri_percent_encode(args_base64);
    url += "&baas_at=";
    url += user.access_token; // doesn't need url encoding

    return Request{
        HttpMethod::get,
        url,
        m_request_timeout_ms,
        {{"Accept", "text/event-stream"}},
    };
}

RemoteMongoClient App::remote_mongo_client(const std::string& service_name)
{
    return RemoteMongoClient(shared_from_this(), service_name);
}

PushClient App::push_notification_client(const std::string& service_name)
{
    return PushClient(service_name,
                      m_config.app_id,
                      m_request_timeout_ms,
                      shared_from_this());
}

} // namespace app
} // namespace realm
