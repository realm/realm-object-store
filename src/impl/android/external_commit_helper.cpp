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

#include "impl/external_commit_helper.hpp"
#include "impl/realm_coordinator.hpp"
#include "util/format.hpp"

#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sstream>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

#include <realm/history.hpp>

#ifdef __ANDROID__
#include <android/log.h>
#define ANDROID_LOG __android_log_print
#else
#define ANDROID_LOG(...)
#endif

using namespace realm;
using namespace realm::_impl;

#define LOGE(fmt...) do { \
    fprintf(stderr, fmt); \
    ANDROID_LOG(ANDROID_LOG_ERROR, "REALM", fmt); \
} while (0)

namespace {
// Write a byte to a pipe to notify anyone waiting for data on the pipe
void notify_fd(int fd)
{
    while (true) {
        char c = 0;
        ssize_t ret = write(fd, &c, 1);
        if (ret == 1) {
            break;
        }

        // If the pipe's buffer is full, we need to read some of the old data in
        // it to make space. We don't just read in the code waiting for
        // notifications so that we can notify multiple waiters with a single
        // write.
        assert(ret == -1 && errno == EAGAIN);
        char buff[1024];
        read(fd, buff, sizeof buff);
    }
}
} // anonymous namespace

std::vector<ExternalCommitHelper*> ExternalCommitHelper::s_helper_list;
std::mutex ExternalCommitHelper::s_mutex;
std::unique_ptr<ExternalCommitHelper::DaemonThread> ExternalCommitHelper::s_daemon_thread;

void ExternalCommitHelper::FdHolder::close()
{
    if (m_fd != -1) {
        ::close(m_fd);
    }
    m_fd = -1;
}

ExternalCommitHelper::ExternalCommitHelper(RealmCoordinator& parent)
: m_parent(parent)
, m_history(realm::make_in_realm_history(parent.get_path()))
, m_sg(*m_history, SharedGroupOptions(
                parent.get_config().in_memory ?
                SharedGroupOptions::Durability::MemOnly : SharedGroupOptions::Durability::Full,
                parent.get_config().encryption_key.data(),
                false,
                std::function<void(int, int)>(),
                realm::get_temporary_directory()))
{
    m_sg.begin_read();
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_daemon_thread) {
        s_daemon_thread = std::make_unique<DaemonThread>();
    }
    s_helper_list.push_back(this);
}

ExternalCommitHelper::~ExternalCommitHelper()
{
    bool free_daemon = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_helper_list.erase(std::remove(s_helper_list.begin(), s_helper_list.end(), this), s_helper_list.end());
        if (s_helper_list.empty()) {
            free_daemon = true;
        }
    }
    // This destructor is locked by s_coordinator_mutex.
    // Daemon thread listener loop might hold a lock to s_mutex. Stopping thread should no be locked with s_mutex.
    if (free_daemon) {
        s_daemon_thread.reset();
    }
}

ExternalCommitHelper::DaemonThread::DaemonThread()
{
    m_epfd = epoll_create(1);
    if (m_epfd == -1) {
        throw std::system_error(errno, std::system_category());
    }

    // The fifo is always created in the temporary directory which is on the internal storage.
    // See https://github.com/realm/realm-java/issues/3140
    std::string temporary_dir = realm::get_temporary_directory();
    if (temporary_dir.empty()) {
        throw std::runtime_error("Temporary directory has not been set.");
    }
    auto path = util::format("%1realm.note", temporary_dir);

    // Create and open the named pipe
    int ret = mkfifo(path.c_str(), 0600);
    if (ret == -1) {
        int err = errno;
        // the fifo already existing isn't an error
        if (ret == -1 && err != EEXIST) {
            // Workaround for a mkfifo bug on Blackberry devices:
            // When the fifo already exists, mkfifo fails with error ENOSYS which is not correct.
            // In this case, we use stat to check if the path exists and it is a fifo.
            struct stat stat_buf;
            if (err == ENOSYS && stat(path.c_str(), &stat_buf) == 0) {
                if ((stat_buf.st_mode & S_IFMT) != S_IFIFO) {
                    throw std::runtime_error(path + " exists and it is not a fifo.");
                }
            }
            else {
                throw std::system_error(err, std::system_category());
            }
        }
    }

    m_notify_fd = open(path.c_str(), O_RDWR);
    if (m_notify_fd == -1) {
        throw std::system_error(errno, std::system_category());
    }

    // Make writing to the pipe return -1 when the pipe's buffer is full
    // rather than blocking until there's space available
    ret = fcntl(m_notify_fd, F_SETFL, O_NONBLOCK);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category());
    }

    // Create the anonymous pipe
    int pipe_fd[2];
    ret = pipe(pipe_fd);
    if (ret == -1) {
        throw std::system_error(errno, std::system_category());
    }

    m_shutdown_read_fd = pipe_fd[0];
    m_shutdown_write_fd = pipe_fd[1];

    m_thread = std::thread([=] {
        try {
            listen();
        }
        catch (std::exception const& e) {
            LOGE("uncaught exception in notifier thread: %s: %s\n", typeid(e).name(), e.what());
            throw;
        }
        catch (...) {
            LOGE("uncaught exception in notifier thread\n");
            throw;
        }
    });
}

ExternalCommitHelper::DaemonThread::~DaemonThread()
{
    notify_fd(m_shutdown_write_fd);
    m_thread.join(); // Wait for the thread to exit
}

void ExternalCommitHelper::DaemonThread::listen()
{
    pthread_setname_np(pthread_self(), "Realm notification listener");

    int ret;

    struct epoll_event event[2];

    event[0].events = EPOLLIN | EPOLLET;
    event[0].data.fd = m_notify_fd;
    ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_notify_fd, &event[0]);
    assert(ret == 0);

    event[1].events = EPOLLIN;
    event[1].data.fd = m_shutdown_read_fd;
    ret = epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_shutdown_read_fd, &event[1]);
    assert(ret == 0);

    while (true) {
        struct epoll_event ev;
        ret = epoll_wait(m_epfd, &ev, 1, -1);

        if (ret == -1 && errno == EINTR) {
            // Interrupted system call, try again.
            continue;
        }

        assert(ret >= 0);
        if (ret == 0) {
            // Spurious wakeup; just wait again
            continue;
        }

        if (ev.data.u32 == (uint32_t)m_shutdown_read_fd) {
            return;
        }
        assert(ev.data.u32 == (uint32_t)m_notify_fd);

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            for (auto it : s_helper_list) {
                if (it->m_sg.has_changed()) {
                    it->m_sg.end_read();
                    it->m_sg.begin_read();
                    it->m_parent.on_change();
                }
            }
        }
    }
}


void ExternalCommitHelper::notify_others()
{
    // Lock is not necessary here since there will always be a valid s_daemon_thread if there is more than one
    // RealmCoordinator lives.
    notify_fd(s_daemon_thread->m_notify_fd);
}
