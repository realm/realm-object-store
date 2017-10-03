////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
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

#include "impl/external_commit_helper.hpp"
#include "impl/realm_coordinator.hpp"

#include <algorithm>

using namespace realm;
using namespace realm::_impl;

ExternalCommitHelper::ExternalCommitHelper(RealmCoordinator& parent)
: m_parent(parent)
{
    std::string path = parent.get_path();
    std::replace(path.begin(), path.end(), '\\', '/');
    std::wstring shared_memory_name = L"Local\\Realm_ObjectStore_ExternalCommitHelper_SharedCondVar_" + std::wstring(path.begin(), path.end());
#if REALM_WINDOWS
    m_shared_memory = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(*m_condvar_shared), shared_memory_name.c_str());
    auto error = GetLastError();
    m_condvar_shared = reinterpret_cast<InterprocessCondVar::SharedPart*>(MapViewOfFile(m_shared_memory, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*m_condvar_shared)));
#elif REALM_UWP
    m_shared_memory = CreateFileMappingFromApp(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, sizeof(*m_condvar_shared), shared_memory_name.c_str());
    auto error = GetLastError();
    m_condvar_shared = reinterpret_cast<InterprocessCondVar::SharedPart*>(MapViewOfFileFromApp(m_shared_memory, FILE_MAP_ALL_ACCESS, 0, sizeof(*m_condvar_shared)));
#endif
    if (error == 0) {
        InterprocessCondVar::init_shared_part(*m_condvar_shared);
    } else if (error != ERROR_ALREADY_EXISTS) {
        throw std::system_error(error, std::system_category());
    }

    m_mutex.set_shared_part(InterprocessMutex::SharedPart(), parent.get_path(), "ExternalCommitHelper_ControlMutex");
    m_commit_available.set_shared_part(*m_condvar_shared, parent.get_path(),
                                       "ExternalCommitHelper_CommitCondVar",
                                       std::filesystem::temp_directory_path().u8string());
    m_thread = std::async(std::launch::async, [this]() { listen(); });
}

ExternalCommitHelper::~ExternalCommitHelper()
{
    {
        std::lock_guard<InterprocessMutex> lock(m_mutex);
        m_keep_listening = false;
        m_commit_available.notify_all();
    }
    m_thread.wait();

    m_commit_available.release_shared_part();
    UnmapViewOfFile(m_condvar_shared);
    CloseHandle(m_shared_memory);
}

void ExternalCommitHelper::notify_others()
{
    m_commit_available.notify_all();
}

void ExternalCommitHelper::listen()
{
    std::lock_guard<InterprocessMutex> lock(m_mutex);
    while (m_keep_listening) {
        m_commit_available.wait(m_mutex, nullptr);
        m_parent.on_change();
    }
}
