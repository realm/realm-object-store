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


#ifndef CORE_STITCH_SERVICE_CLIENT_HPP
#define CORE_STITCH_SERVICE_CLIENT_HPP

#include <string>
#include <realm/util/optional.hpp>
#include <sync/generic_network_transport.hpp>

namespace realm {
namespace app {

class AuthRequestClient {
    /// Performs an authenticated request to the Stitch server, using the current authentication state, and should
    /// throw when not currently authenticated.
    /// @param auth_request The request to perform
    Response do_authenticated_request(const Request auth_request);
};

/// A class representing the authentication API routes on the Stitch server.
class ServiceRoutes {
public:
    ServiceRoutes(std::string client_app_id) : m_client_app_id(client_app_id) { }
    /// Returns the route on the server for discovering application metadata.
    std::string app_metadata_route();
    /// Returns the route on the server for executing a function.
    std::string function_call_route();
private:
    
    std::string m_client_app_id;
};

/// A class providing the core functionality necessary to make authenticated function call requests for a particular
/// Stitch service.
class StitchServiceClient {
public:
    const util::Optional<std::string> service_name;
    
    using BSONValue = std::string;

    /// Calls the MongoDB Stitch function with the provided name and arguments.
    /// @param name The name of the Stitch function to be called.
    /// @param args The `BSONArray` of arguments to be provided to the function.
    /// @param request_timeout The number of seconds the client should wait for a response from the server before failing with an error.
    void call_function(std::string name, std::vector<BSONValue> args, util::Optional<int> request_timeout);
    
    /// Calls the MongoDB Stitch function with the provided name and arguments,
    /// and decodes the result into a type as specified by the `T` type parameter.
    /// @param name The name of the Stitch function to be called.
    /// @param args The `BSONArray` of arguments to be provided to the function.
    /// @param request_timeout  The number of seconds the client should wait for a response from the server before failing with an error.
    template<class T>
    T call_function(std::string name, std::vector<BSONValue> args, util::Optional<int> request_timeout);

    /// Calls the MongoDB Stitch function with the provided name and arguments,
    /// and decodes the result into an optional type as specified by the `T` type parameter.
    /// @param name The name of the Stitch function to be called.
    /// @param args The `BSONArray` of arguments to be provided to the function.
    /// @param request_timeout The number of seconds the client should wait for a response from the server before failing with an error.
    template<class T>
    util::Optional<T> call_function(std::string name, std::vector<BSONValue> args, util::Optional<int> request_timeout);
    
    StitchServiceClient(AuthRequestClient request_client,
                            ServiceRoutes routes,
                            util::Optional<std::string> service_name) :
    service_name(service_name),
    m_request_client(request_client),
    m_routes(routes) { }
    
private:
    AuthRequestClient m_request_client;
    ServiceRoutes m_routes;
};

} // namespace app
} // namespace realm

#endif /* CORE_STITCH_SERVICE_CLIENT_HPP */
