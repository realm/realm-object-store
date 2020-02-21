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

#include <stdio.h>
#include <string>
#include <memory>
#include <map>
#include <vector>

namespace realm {
namespace app {
namespace error {

#pragma mark Errors
struct None {
    constexpr explicit None(int)
    {
    }
};
static constexpr None none{0};

struct AppError
{
    AppError()
    {
    };
    constexpr AppError(None)
    {
    };
};

struct ClientError : public AppError, std::runtime_error
{
public:
    enum class code {
        bad_token,
    };

private:
    static const std::string error_string_for_code(const ClientError::code code)
    {
        switch (code)
        {
            case ClientError::code::bad_token:
                return "Bad Token";
        }
    }


public:
    ClientError(ClientError::code c) :
    AppError(),
    std::runtime_error(error_string_for_code(c)),
    m_msg(error_string_for_code(c)) {};

private:
    const std::string m_msg;
};

static inline const ClientError client(ClientError::code code) {
    return ClientError(code);
};

enum class service_error_code {
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
private:
    static service_error_code error_code_for_string(const std::string code)
    {
        if (code == "MissingAuthReq")
            return service_error_code::missing_auth_req;
        else if (code == "InvalidSession")
            return service_error_code::invalid_session;
        else if (code == "UserAppDomainMismatch")
            return service_error_code::user_app_domain_mismatch;
        else if (code == "DomainNotAllowed")
            return service_error_code::domain_not_allowed;
        else if (code == "ReadSizeLimitExceeded")
            return service_error_code::read_size_limit_exceeded;
        else if (code == "InvalidParameter")
            return service_error_code::invalid_parameter;
        else if (code == "MissingParameter")
            return service_error_code::missing_parameter;
        else if (code == "TwilioError")
            return service_error_code::twilio_error;
        else if (code == "GCMError")
            return service_error_code::gcm_error;
        else if (code == "HTTPError")
            return service_error_code::http_error;
        else if (code == "AWSError")
            return service_error_code::aws_error;
        else if (code == "MongoDBError")
            return service_error_code::mongodb_error;
        else if (code == "ArgumentsNotAllowed")
            return service_error_code::arguments_not_allowed;
        else if (code == "FunctionExecutionError")
            return service_error_code::function_execution_error;
        else if (code == "NoMatchingRule")
            return service_error_code::no_matching_rule_found;
        else if (code == "InternalServerError")
            return service_error_code::internal_server_error;
        else if (code == "AuthProviderNotFound")
            return service_error_code::auth_provider_not_found;
        else if (code == "AuthProviderAlreadyExists")
            return service_error_code::auth_provider_already_exists;
        else if (code == "ServiceNotFound")
            return service_error_code::service_not_found;
        else if (code == "ServiceTypeNotFound")
            return service_error_code::service_type_not_found;
        else if (code == "ServiceAlreadyExists")
            return service_error_code::service_already_exists;
        else if (code == "ServiceCommandNotFound")
            return service_error_code::service_command_not_found;
        else if (code == "ValueNotFound")
            return service_error_code::value_not_found;
        else if (code == "ValueAlreadyExists")
            return service_error_code::value_already_exists;
        else if (code == "ValueDuplicateName")
            return service_error_code::value_duplicate_name;
        else if (code == "FunctionNotFound")
            return service_error_code::function_not_found;
        else if (code == "FunctionAlreadyExists")
            return service_error_code::function_already_exists;
        else if (code == "FunctionDuplicateName")
            return service_error_code::function_duplicate_name;
        else if (code == "FunctionSyntaxError")
            return service_error_code::function_syntax_error;
        else if (code == "FunctionInvalid")
            return service_error_code::function_invalid;
        else if (code == "IncomingWebhookNotFound")
            return service_error_code::incoming_webhook_not_found;
        else if (code == "IncomingWebhookAlreadyExists")
            return service_error_code::incoming_webhook_already_exists;
        else if (code == "IncomingWebhookDuplicateName")
            return service_error_code::incoming_webhook_duplicate_name;
        else if (code == "RuleNotFound")
            return service_error_code::rule_not_found;
        else if (code == "APIKeyNotFound")
            return service_error_code::api_key_not_found;
        else if (code == "RuleAlreadyExists")
            return service_error_code::rule_already_exists;
        else if (code == "AuthProviderDuplicateName")
            return service_error_code::auth_provider_duplicate_name;
        else if (code == "RestrictedHost")
            return service_error_code::restricted_host;
        else if (code == "APIKeyAlreadyExists")
            return service_error_code::api_key_already_exists;
        else if (code == "IncomingWebhookAuthFailed")
            return service_error_code::incoming_webhook_auth_failed;
        else if (code == "ExecutionTimeLimitExceeded")
            return service_error_code::execution_time_limit_exceeded;
        else if (code == "NotCallable")
            return service_error_code::not_callable;
        else if (code == "UserAlreadyConfirmed")
            return service_error_code::user_already_confirmed;
        else if (code == "UserNotFound")
            return service_error_code::user_not_found;
        else if (code == "UserDisabled")
            return service_error_code::user_disabled;
        else
            return service_error_code::unknown;
    }

public:
    const std::string message() const { return m_message; };

    service_error_code code() const { return m_code; };

    const std::string raw_code() const { return m_raw_code; }

    ServiceError(std::string raw_code, std::string message) :
    AppError(),
    m_code(error_code_for_string(raw_code)),
    m_message(message),
    m_raw_code(raw_code) {};

private:
    const service_error_code m_code;
    const std::string m_message;
    const std::string m_raw_code;
};

static inline const ServiceError service(const std::string code, const std::string message)
{
    return error::ServiceError(code, message);
}
}



enum class Method {
    get, post, patch
};

/**
 * An HTTP request that can be made to an arbitrary server.
 */
struct Request {
public:
    /**
     * The HTTP method of this request.
     */
    Method method;

    /**
     * The URL to which this request will be made.
     */
    std::string url;

    /**
     * The number of seconds that the underlying transport should spend on an HTTP round trip before failing with an
     * error.
     */
    int timeout_ms;

    /**
     * The HTTP headers of this request.
     */
    std::map<std::string, std::string> headers;

    /**
     * The body of the request.
     */
    std::vector<char> body;
};

/**
 * The contents of an HTTP response.
 */
struct Response {
public:
    /**
     * The status code of the HTTP response.
     */
    int status_code;

    /**
     * The headers of the HTTP response.
     */
    std::map<std::string, std::string> headers;

    /**
     * The body of the HTTP response.
     */
    std::vector<char> body;
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
}
}

#endif /* REALM_GENERIC_NETWORK_TRANSPORT_HPP */
