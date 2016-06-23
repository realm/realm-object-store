////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef GENERIC_EXTERNAL_COMMIT_HELPER
#define GENERIC_EXTERNAL_COMMIT_HELPER

#include <realm/group_shared.hpp>

#include <future>

#include "impl/external_commit_helper.hpp"

namespace realm {
class Replication;

namespace _impl {
class RealmCoordinator;

class GenericExternalCommitHelper : public ExternalCommitHelperImpl {
public:
    GenericExternalCommitHelper(RealmCoordinator& parent);
    ~GenericExternalCommitHelper();

    // A no-op in this version, but needed for the Apple version
    void notify_others() override { }

private:

    // A shared group used to listen for changes
    std::unique_ptr<Replication> m_history;
    SharedGroup m_sg;

    // The listener thread
    std::future<void> m_thread;
};

} // namespace _impl
} // namespace realm

#endif // GENERIC_EXTERNAL_COMMIT_HELPER

