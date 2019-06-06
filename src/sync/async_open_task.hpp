////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019 Realm Inc.
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

#pragma once

namespace realm {
class Realm;
class SyncSession;
namespace _impl {
class RealmCoordinator;
}


// Class used to wrap the intent of opening a new Realm or fully synchronize it before returning it to the user
// Timeouts are not handled by this class but must be handled by each binding.
class AsyncOpenTask {
public:
    AsyncOpenTask(std::string realmPath);
    // Starts downloading the Realm. The callback will be triggered either when the download completes
    // or an error is encountered.
    void start(std::function<void(std::shared_ptr<Realm>, std::exception_ptr)> callback);

    // Cancels the download and stops the session. No further functions can be called on this class.
    // All downloads shares the same session, so canceling the download will effectively cancel all current
    // downloads of the same file. This will be reported as a "Canceled" exception thrown by the sync layer.
    void cancel();

    uint64_t register_download_progress_notifier(std::function<void(uint64_t transferred_bytes, uint64_t transferrable_bytes)> callback);
    void unregister_download_progress_notifier(uint64_t token);

private:
    std::shared_ptr<_impl::RealmCoordinator> m_coordinator;
    std::shared_ptr<SyncSession> m_session;
    bool m_canceled = false;
};

}
