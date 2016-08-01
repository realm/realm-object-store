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
#ifndef EXTERNAL_COMMIT_HELPER
#define EXTERNAL_COMMIT_HELPER

#include <realm/group_shared.hpp>

#include <future>

namespace realm {
class Replication;

namespace _impl {

class RealmCoordinator;
class ExternalCommitHelperImpl {
public:
    ExternalCommitHelperImpl(RealmCoordinator& parent):m_parent(parent) {};
    virtual ~ExternalCommitHelperImpl() {};

    virtual void notify_others() = 0;

protected:
    RealmCoordinator& m_parent;
};

// Implement this function to return the implementation instance.
extern ExternalCommitHelperImpl* get_external_commit_helper(RealmCoordinator& parent);

class ExternalCommitHelper {
public:
    ExternalCommitHelper(_impl::RealmCoordinator& parent) : impl(get_external_commit_helper(parent)) {}
    ~ExternalCommitHelper() {};

    void notify_others() { impl-> notify_others(); }

private:
    const std::unique_ptr<ExternalCommitHelperImpl> impl;
};

} // namespace _impl
} // namespace realm

#endif // EXTERNAL_COMMIT_HELPER

