//
//  core_remote_mongo_read_operation.h
//  realm-object-store
//
//  NEEDS HEADER on 09/03/2020.
//

#ifndef CORE_REMOTE_MONGO_READ_OPERATION_HPP
#define CORE_REMOTE_MONGO_READ_OPERATION_HPP

#include "core_remote_mongo_client.hpp"

namespace realm {
namespace mongodb {

template<typename T>
class CoreRemoteMongoReadOperation {
    
public:
    CoreRemoteMongoReadOperation(std::string command,
                                 nlohmann::json args,
                                 CoreStitchServiceClient service) :
    m_command(command),
    m_args(args),
    m_service(service) { }
private:
    std::string m_command;
    nlohmann::json m_args;
    CoreStitchServiceClient m_service;
};

} // namespace mongodb
} // namespace realm

#endif /* core_remote_mongo_read_operation */
