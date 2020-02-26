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


#ifndef REALM_GENERIC_NETWORK_TRANSPORT_HPP
#define REALM_GENERIC_NETWORK_TRANSPORT_HPP

#include <functional>
#include <string>
#include <memory>
#include <map>
#include <vector>

#include <realm/util/to_string.hpp>

namespace realm {
namespace app {
namespace error {

#pragma mark Errors

struct AppError : public std::runtime_error
{
    enum class Type {
        Generic,
        Client,
        Session,
    };

    AppError()
    : std::runtime_error("AppError")
    , type(Type::Generic)
    {
    };
    AppError(std::string msg, Type classification = Type::Generic)
    : std::runtime_error(msg)
    , type(classification)
    {
    }
    const Type type;
};

struct ClientError : public AppError
{
    enum class Code {
        bad_token,
        bad_response
    };

    ClientError(ClientError::Code c)
    : AppError(error_string_for_code(c), AppError::Type::Client)
    , code(c)
    {
    }

    const Code code;

private:
    static const std::string error_string_for_code(const ClientError::Code& code)
    {
        switch (code)
        {
            case ClientError::Code::bad_token:
                return "Bad Token";
            case ClientError::Code::bad_response:
                return "Bad Response";
        }
        return util::format("unknown error code: %1", static_cast<int>(code));
    }
};

enum class ServiceErrorCode {
    missing_auth_req,
    /// Invalid session, expired, no associated user, or app domain mismatch
    invalid_session,
    user_app_domain_mismatch,
    domain_not_allowed,
    read_size_limit_exceeded,
    invalid_parameter,
    missing_parameter,
    twilio_error,
    gcm_error,
    http_error,
    aws_error,
    mongodb_error,
    arguments_not_allowed,
    function_execution_error,
    no_matching_rule_found,
    internal_server_error,
    auth_provider_not_found,
    auth_provider_already_exists,
    service_not_found,
    service_type_not_found,
    service_already_exists,
    service_command_not_found,
    value_not_found,
    value_already_exists,
    value_duplicate_name,
    function_not_found,
    function_already_exists,
    function_duplicate_name,
    function_syntax_error,
    function_invalid,
    incoming_webhook_not_found,
    incoming_webhook_already_exists,
    incoming_webhook_duplicate_name,
    rule_not_found,
    api_key_not_found,
    rule_already_exists,
    rule_duplicate_name,
    auth_provider_duplicate_name,
    restricted_host,
    api_key_already_exists,
    incoming_webhook_auth_failed,
    execution_time_limit_exceeded,
    not_callable,
    user_already_confirmed,
    user_not_found,
    user_disabled,
    unknown,
    none
};

/// Struct allowing for generic error data.
struct ServiceError : public AppError
{
    ServiceError(std::string raw_code, std::string message)
    : AppError("ServiceError: " + raw_code + ": " + message, AppError::Type::Session)
    , m_code(error_code_for_string(raw_code))
    , m_raw_code(raw_code)
    {
    }
    const std::string message() const { return std::runtime_error::what(); };

    ServiceErrorCode code() const { return m_code; };

    const std::string raw_code() const { return m_raw_code; }

private:
    static ServiceErrorCode error_code_for_string(const std::string& code)
    {
        if (code == "MissingAuthReq")
            return ServiceErrorCode::missing_auth_req;
        else if (code == "InvalidSession")
            return ServiceErrorCode::invalid_session;
        else if (code == "UserAppDomainMismatch")
            return ServiceErrorCode::user_app_domain_mismatch;
        else if (code == "DomainNotAllowed")
            return ServiceErrorCode::domain_not_allowed;
        else if (code == "ReadSizeLimitExceeded")
            return ServiceErrorCode::read_size_limit_exceeded;
        else if (code == "InvalidParameter")
            return ServiceErrorCode::invalid_parameter;
        else if (code == "MissingParameter")
            return ServiceErrorCode::missing_parameter;
        else if (code == "TwilioError")
            return ServiceErrorCode::twilio_error;
        else if (code == "GCMError")
            return ServiceErrorCode::gcm_error;
        else if (code == "HTTPError")
            return ServiceErrorCode::http_error;
        else if (code == "AWSError")
            return ServiceErrorCode::aws_error;
        else if (code == "MongoDBError")
            return ServiceErrorCode::mongodb_error;
        else if (code == "ArgumentsNotAllowed")
            return ServiceErrorCode::arguments_not_allowed;
        else if (code == "FunctionExecutionError")
            return ServiceErrorCode::function_execution_error;
        else if (code == "NoMatchingRule")
            return ServiceErrorCode::no_matching_rule_found;
        else if (code == "InternalServerError")
            return ServiceErrorCode::internal_server_error;
        else if (code == "AuthProviderNotFound")
            return ServiceErrorCode::auth_provider_not_found;
        else if (code == "AuthProviderAlreadyExists")
            return ServiceErrorCode::auth_provider_already_exists;
        else if (code == "ServiceNotFound")
            return ServiceErrorCode::service_not_found;
        else if (code == "ServiceTypeNotFound")
            return ServiceErrorCode::service_type_not_found;
        else if (code == "ServiceAlreadyExists")
            return ServiceErrorCode::service_already_exists;
        else if (code == "ServiceCommandNotFound")
            return ServiceErrorCode::service_command_not_found;
        else if (code == "ValueNotFound")
            return ServiceErrorCode::value_not_found;
        else if (code == "ValueAlreadyExists")
            return ServiceErrorCode::value_already_exists;
        else if (code == "ValueDuplicateName")
            return ServiceErrorCode::value_duplicate_name;
        else if (code == "FunctionNotFound")
            return ServiceErrorCode::function_not_found;
        else if (code == "FunctionAlreadyExists")
            return ServiceErrorCode::function_already_exists;
        else if (code == "FunctionDuplicateName")
            return ServiceErrorCode::function_duplicate_name;
        else if (code == "FunctionSyntaxError")
            return ServiceErrorCode::function_syntax_error;
        else if (code == "FunctionInvalid")
            return ServiceErrorCode::function_invalid;
        else if (code == "IncomingWebhookNotFound")
            return ServiceErrorCode::incoming_webhook_not_found;
        else if (code == "IncomingWebhookAlreadyExists")
            return ServiceErrorCode::incoming_webhook_already_exists;
        else if (code == "IncomingWebhookDuplicateName")
            return ServiceErrorCode::incoming_webhook_duplicate_name;
        else if (code == "RuleNotFound")
            return ServiceErrorCode::rule_not_found;
        else if (code == "APIKeyNotFound")
            return ServiceErrorCode::api_key_not_found;
        else if (code == "RuleAlreadyExists")
            return ServiceErrorCode::rule_already_exists;
        else if (code == "AuthProviderDuplicateName")
            return ServiceErrorCode::auth_provider_duplicate_name;
        else if (code == "RestrictedHost")
            return ServiceErrorCode::restricted_host;
        else if (code == "APIKeyAlreadyExists")
            return ServiceErrorCode::api_key_already_exists;
        else if (code == "IncomingWebhookAuthFailed")
            return ServiceErrorCode::incoming_webhook_auth_failed;
        else if (code == "ExecutionTimeLimitExceeded")
            return ServiceErrorCode::execution_time_limit_exceeded;
        else if (code == "NotCallable")
            return ServiceErrorCode::not_callable;
        else if (code == "UserAlreadyConfirmed")
            return ServiceErrorCode::user_already_confirmed;
        else if (code == "UserNotFound")
            return ServiceErrorCode::user_not_found;
        else if (code == "UserDisabled")
            return ServiceErrorCode::user_disabled;
        else
            return ServiceErrorCode::unknown;
    }

    const ServiceErrorCode m_code;
    const std::string m_raw_code;
};

} // namespace error


enum class Method {
    get, post, patch
};

/**
 * An HTTP request that can be made to an arbitrary server.
 */
struct Request {
    /**
     * The HTTP method of this request.
     */
    Method method;

    /**
     * The URL to which this request will be made.
     */
    std::string url;

    /**
     * The number of milliseconds that the underlying transport should spend on an HTTP round trip before failing with an
     * error.
     */
    uint64_t timeout_ms;

    /**
     * The HTTP headers of this request.
     */
    std::map<std::string, std::string> headers;

    /**
     * The body of the request.
     */
    std::string body;
};

/**
 * The contents of an HTTP response.
 */
struct Response {
    /**
     * The status code of the HTTP response.
     */
    int http_status_code;

    /**
     * A custom status code provided by the language binding.
     */
    int binding_status_code;

    /**
     * The headers of the HTTP response.
     */
    std::map<std::string, std::string> headers;

    /**
     * The body of the HTTP response.
     */
    std::string body;
};

#pragma mark GenericNetworkTransport

/// Generic network transport for foreign interfaces.
struct GenericNetworkTransport {
    typedef std::unique_ptr<GenericNetworkTransport> (*network_transport_factory)();

public:
    virtual void send_request_to_server(const Request request,
                                        std::function<void(const Response)> completionBlock) = 0;
    virtual ~GenericNetworkTransport() = default;
    static void set_network_transport_factory(network_transport_factory);
    static std::unique_ptr<GenericNetworkTransport> get();
};

} // namespace app
} // namespace realm

#endif /* REALM_GENERIC_NETWORK_TRANSPORT_HPP */
