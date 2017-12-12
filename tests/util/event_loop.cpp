/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#include "util/event_loop.hpp"

#include <realm/util/assert.hpp>
#include <realm/util/features.h>

#include <mutex>
#include <stdexcept>
#include <vector>

#include "util/event_loop_signal.hpp"

#if REALM_USE_CF
#include <realm/util/cf_ptr.hpp>
#endif

using namespace realm::util;

struct EventLoop::Impl {
    // Returns the main event loop.
    static std::unique_ptr<Impl> main();

    // Run the event loop until the given return predicate returns true
    void run_until(std::function<bool()> predicate);

    // Schedule execution of the given function on the event loop.
    void perform(std::function<void()>);

    ~Impl();

private:
#if REALM_USE_UV
    Impl(uv_loop_t* loop);

    std::vector<std::function<void()>> m_pending_work;
    std::mutex m_mutex;
    uv_loop_t* m_loop;
    uv_async_t m_perform_work;
#elif REALM_USE_CF
    Impl(util::CFPtr<CFRunLoopRef> loop) : m_loop(std::move(loop)) { }

    util::CFPtr<CFRunLoopRef> m_loop;
#elif REALM_USE_ALOOPER
    Impl(ALooper* looper);
    void perform_work();

    std::vector<std::function<void()>> m_pending_work;
    std::mutex m_mutex;
    std::shared_ptr<util::EventLoopSignal<std::function<void()>>> m_signal;
#endif
};

EventLoop& EventLoop::main()
{
    static EventLoop main(Impl::main());
    return main;
}

EventLoop::EventLoop(std::unique_ptr<Impl> impl) : m_impl(std::move(impl))
{
}

EventLoop::~EventLoop() = default;

void EventLoop::run_until(std::function<bool()> predicate)
{
    return m_impl->run_until(std::move(predicate));
}

void EventLoop::perform(std::function<void()> function)
{
    return m_impl->perform(std::move(function));
}

#if REALM_USE_UV

bool EventLoop::has_implementation() { return true; }

std::unique_ptr<EventLoop::Impl> EventLoop::Impl::main()
{
    return std::unique_ptr<Impl>(new Impl(uv_default_loop()));
}

EventLoop::Impl::Impl(uv_loop_t* loop)
    : m_loop(loop)
{
    m_perform_work.data = this;
    uv_async_init(uv_default_loop(), &m_perform_work, [](uv_async_t* handle) {
        std::vector<std::function<void()>> pending_work;
        {
            Impl& self = *static_cast<Impl*>(handle->data);
            std::lock_guard<std::mutex> lock(self.m_mutex);
            std::swap(pending_work, self.m_pending_work);
        }

        for (auto& f : pending_work)
            f();
    });
}

EventLoop::Impl::~Impl()
{
    uv_close((uv_handle_t*)&m_perform_work, [](uv_handle_t*){});
    uv_loop_close(m_loop);
}

struct IdleHandler {
    uv_idle_t* idle = new uv_idle_t;

    IdleHandler(uv_loop_t* loop)
    {
        uv_idle_init(loop, idle);
    }
    ~IdleHandler()
    {
        uv_close(reinterpret_cast<uv_handle_t*>(idle), [](uv_handle_t* handle) {
            delete reinterpret_cast<uv_idle_t*>(handle);
        });
    }
};

void EventLoop::Impl::run_until(std::function<bool()> predicate)
{
    if (predicate())
        return;

    IdleHandler observer(m_loop);
    observer.idle->data = &predicate;

    uv_idle_start(observer.idle, [](uv_idle_t* handle) {
        auto& predicate = *static_cast<std::function<bool()>*>(handle->data);
        if (predicate()) {
            uv_stop(handle->loop);
        }
    });

    uv_run(m_loop, UV_RUN_DEFAULT);
    uv_idle_stop(observer.idle);
}

void EventLoop::Impl::perform(std::function<void()> f)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending_work.push_back(std::move(f));
    }
    uv_async_send(&m_perform_work);
}

#elif REALM_USE_CF

bool EventLoop::has_implementation() { return true; }

std::unique_ptr<EventLoop::Impl> EventLoop::Impl::main()
{
    return std::unique_ptr<Impl>(new Impl(retainCF(CFRunLoopGetMain())));
}

EventLoop::Impl::~Impl() = default;

void EventLoop::Impl::run_until(std::function<bool()> predicate)
{
    REALM_ASSERT(m_loop.get() == CFRunLoopGetCurrent());

    auto callback = [](CFRunLoopObserverRef, CFRunLoopActivity, void* info) {
        if ((*static_cast<std::function<bool()>*>(info))()) {
            CFRunLoopStop(CFRunLoopGetCurrent());
        }
    };
    CFRunLoopObserverContext ctx{};
    ctx.info = &predicate;
    auto observer = adoptCF(CFRunLoopObserverCreate(kCFAllocatorDefault, kCFRunLoopAllActivities,
                                                    true, 0, callback, &ctx));
    auto timer = adoptCF(CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(), 0.0005, 0, 0,
                                                         ^(CFRunLoopTimerRef){
        // Do nothing. The timer firing is sufficient to cause our runloop observer to run.
    }));
    CFRunLoopAddObserver(CFRunLoopGetCurrent(), observer.get(), kCFRunLoopCommonModes);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer.get(), kCFRunLoopCommonModes);
    CFRunLoopRun();
    CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), timer.get(), kCFRunLoopCommonModes);
    CFRunLoopRemoveObserver(CFRunLoopGetCurrent(), observer.get(), kCFRunLoopCommonModes);
}

void EventLoop::Impl::perform(std::function<void()> f)
{
    CFRunLoopPerformBlock(m_loop.get(), kCFRunLoopDefaultMode, ^{ f(); });
    CFRunLoopWakeUp(m_loop.get());
}

#elif REALM_USE_ALOOPER

static ALooper* s_main_looper;

__attribute__((__constructor__))
static void acquire_main_looper()
{
    s_main_looper = ALooper_prepare(0);
    ALooper_acquire(s_main_looper);
}

__attribute__((__destructor__))
static void release_main_looper()
{
    ALooper_release(s_main_looper);
}

bool EventLoop::has_implementation() { return true; }

std::unique_ptr<EventLoop::Impl> EventLoop::Impl::main()
{
    return std::unique_ptr<Impl>(new Impl(s_main_looper));
}

EventLoop::Impl::Impl(ALooper* looper)
    : m_signal(std::make_shared<util::EventLoopSignal<std::function<void()>>>([this] { perform_work(); }, looper))
{
}

EventLoop::Impl::~Impl() = default;

void EventLoop::Impl::run_until(std::function<bool()> predicate)
{
    REALM_ASSERT(m_signal->looper() == ALooper_forThread());

    while (!predicate()) {
        int fd, events;
        void* data;
        ALooper_pollOnce(0.5, &fd, &events, &data);
    }
}

void EventLoop::Impl::perform(std::function<void()> f)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pending_work.push_back(std::move(f));
    }
    m_signal->notify();
}

void EventLoop::Impl::perform_work()
{
    std::vector<std::function<void()>> pending_work;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::swap(pending_work, m_pending_work);
    }

    for (auto& f : pending_work)
        f();
}

#else

bool EventLoop::has_implementation() { return false; }
std::unique_ptr<EventLoop::Impl> EventLoop::Impl::main() { return nullptr; }
EventLoop::Impl::~Impl() = default;
void EventLoop::Impl::run_until(std::function<bool()>) { printf("WARNING: there is no event loop implementation and nothing is happening.\n"); }
void EventLoop::Impl::perform(std::function<void()>) { printf("WARNING: there is no event loop implementation and nothing is happening.\n"); }

#endif
