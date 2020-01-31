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

