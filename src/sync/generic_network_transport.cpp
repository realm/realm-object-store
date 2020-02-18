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

#include "generic_network_transport.hpp"
#include <string>

namespace realm {

struct DummyTransport : public GenericNetworkTransport {
public:
    void send_request_to_server(std::string,
                                std::string,
                                std::map<std::string, std::string>,
                                std::vector<char>,
                                int,
                                std::function<void(std::vector<char>, GenericNetworkError)>) override {}
};

static DummyTransport::network_transport_factory s_factory = [] {
    return std::unique_ptr<GenericNetworkTransport>(new DummyTransport);
};


void GenericNetworkTransport::set_network_transport_factory(GenericNetworkTransport::network_transport_factory factory)
{
    s_factory = std::move(factory);
}

std::unique_ptr<GenericNetworkTransport> GenericNetworkTransport::get()
{
    return s_factory();
}

}

