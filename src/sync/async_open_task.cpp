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

#include "impl/realm_coordinator.hpp"
#include "sync/async_open_task.hpp"
#include "sync/sync_manager.hpp"
#include "sync/sync_session.hpp"

namespace realm {

AsyncOpenTask::AsyncOpenTask(std::string realm_path):
 m_coordinator(_impl::RealmCoordinator::get_coordinator(realm_path)),
 m_session(SyncManager::shared().get_existing_session(realm_path))
{
}

void AsyncOpenTask::start(std::function<void(std::shared_ptr<Realm>, std::exception_ptr)> callback) {
    m_session->wait_for_download_completion([callback, this](std::error_code ec) {
        if (this->m_canceled)
            return; // Swallow all events if the task as been canceled.

        if (ec)
            callback(nullptr, std::make_exception_ptr(std::system_error(ec)));
        else {
            std::shared_ptr<Realm> realm;
            try {
                realm = this->m_coordinator->get_realm();
            }
            catch (...) {
                return callback(nullptr, std::current_exception());
            }
            callback(realm, nullptr);
        }
    });
}

void AsyncOpenTask::cancel() {
    if (m_session) {
        // How to correctly cancel the download?
        m_canceled = true;
        m_session->log_out();
        m_session = nullptr;
        m_coordinator = nullptr;
    }
}

uint64_t AsyncOpenTask::register_download_progress_notifier(std::function<SyncProgressNotifierCallback> callback) {
    if (m_session) {
        m_session->register_progress_notifier(callback, realm::SyncSession::NotifierType::download, false);
    }
}

void AsyncOpenTask::unregister_download_progress_notifier(uint64_t token) {
    if (m_session) {
        m_session->unregister_progress_notifier(token);
    }
}

}
