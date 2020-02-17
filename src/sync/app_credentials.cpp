#include "app_credentials.hpp"
#include "../external/json/json.hpp"

namespace realm {

std::string const kAppProviderKey = "provider";

IdentityProvider const IdentityProviderAnonymous = "anon-user";
IdentityProvider const IdentityProviderFacebook  = "oauth2-facebook";
IdentityProvider const IdentityProviderApple     = "oauth2-apple";

std::vector<char> AppCredentials::serialize() const
{
    nlohmann::json j;
    switch (m_provider)
    {
        case AuthProvider::ANONYMOUS:
            j = {
                {kAppProviderKey, IdentityProviderAnonymous},
            };
            break;
        case AuthProvider::APPLE:
            j = {
                {kAppProviderKey, IdentityProviderApple},
                {"id_token", m_token}
            };
            break;
        case AuthProvider::FACEBOOK:
            j = {
                {kAppProviderKey, IdentityProviderApple},
                {"access_token", m_token}
            };
            break;
    }
    
    std::string raw = j.dump();
    return std::vector<char>(raw.begin(), raw.end());
}

IdentityProvider provider_type_from_enum(AuthProvider provider)
{
    switch (provider)
    {
        case AuthProvider::ANONYMOUS:
            return IdentityProviderAnonymous;
        case AuthProvider::APPLE:
            return IdentityProviderApple;
        case AuthProvider::FACEBOOK:
            return IdentityProviderFacebook;
    }
}

AuthProvider AppCredentials::provider() const
{
    return m_provider;
}

AppCredentialsToken AppCredentials::token() const
{
    return m_token;
}

std::shared_ptr<AppCredentials> AppCredentials::anonymous()
{
    auto credentials = std::make_shared<AppCredentials>();
    credentials->m_provider = AuthProvider::ANONYMOUS;
    return credentials;
}

std::shared_ptr<AppCredentials> AppCredentials::apple(AppCredentialsToken id_token)
{
    auto credentials = std::make_shared<AppCredentials>();
    credentials->m_token = id_token;
    credentials->m_provider = AuthProvider::APPLE;
    return credentials;
}

std::shared_ptr<AppCredentials> AppCredentials::facebook(AppCredentialsToken access_token)
{
    auto credentials = std::make_shared<AppCredentials>();
    credentials->m_token = access_token;
    credentials->m_provider = AuthProvider::FACEBOOK;
    return credentials;
}

//std::shared_ptr<AppCredentials> AppCredentials::user_password(const std::string username,
//                                                              const std::string password)
//{
//    auto credentials = std::make_shared<AppCredentials>();
//    credentials->m_token = access_token;
//    credentials->m_provider = AuthProvider::FACEBOOK;
//    return credentials;
//}

}
