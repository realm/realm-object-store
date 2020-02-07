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


#ifndef generic_network_transport_hpp
#define generic_network_transport_hpp

#include <stdio.h>

#include <memory>
#include <map>
#include <vector>

namespace realm {

#pragma mark GenericNetworkError

/// Struct allowing for generic error data.
struct GenericNetworkError {
    int code;
    std::map<std::string, std::string> data;
};

#pragma mark GenericNetworkTransport

/// Generic network transport for foreign interfaces.
struct GenericNetworkTransport {
    typedef std::unique_ptr<GenericNetworkTransport> (*network_transport_factory)();

public:
    virtual void send_request_to_server(std::string url,
                                        std::string httpMethod,
                                        std::map<std::string, std::string> headers,
                                        std::vector<char> data,
                                        int timeout,
                                        std::function<void(std::vector<char>, GenericNetworkError)> completionBlock) = 0;
    virtual ~GenericNetworkTransport() = default;
    static void set_network_transport_factory(network_transport_factory);
    static std::unique_ptr<GenericNetworkTransport> get();
};

}

#endif /* generic_network_transport_hpp */