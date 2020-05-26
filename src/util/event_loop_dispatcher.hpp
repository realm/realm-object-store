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

#ifndef REALM_OS_UTIL_EVENT_LOOP_DISPATCHER_HPP
#define REALM_OS_UTIL_EVENT_LOOP_DISPATCHER_HPP

#include "util/scheduler.hpp"

#include <mutex>
#include <queue>
#include <tuple>
#include <thread>

namespace realm {

namespace util {
template <class F>
class EventLoopDispatcher;

template <typename... Args>
class EventLoopDispatcher<void(Args...)> {
    using Tuple = std::tuple<typename std::remove_reference<Args>::type...>;
private:
    struct State {
        State(std::function<void(Args...)> func)
        : func(std::move(func))
        {
        }

        const std::function<void(Args...)> func;
        std::queue<Tuple> invocations;
        std::mutex mutex;
        std::shared_ptr<util::Scheduler> scheduler;
    };
    const std::shared_ptr<State> m_state;
    const std::shared_ptr<util::Scheduler> m_scheduler = util::Scheduler::make_default();

public:
    EventLoopDispatcher(std::function<void(Args...)> func)
    : m_state(std::make_shared<State>(std::move(func)))
    {
        m_scheduler->set_notify_callback([state = m_state] {
            std::unique_lock<std::mutex> lock(state->mutex);
            while (!state->invocations.empty()) {
                auto& tuple = state->invocations.front();
                std::apply(state->func, std::move(tuple));
                state->invocations.pop();
            }

            // scheduler retains state, so state needs to only retain scheduler
            // while it has pending work or neither will ever be destroyed
            state->scheduler.reset();
        });
    }

    const std::function<void(Args...)>& func() const { return m_state->func; }

    void operator()(Args... args)
    {
        if (m_scheduler->is_on_thread()) {
            m_state->func(std::forward<Args>(args)...);
            return;
        }

        {
            std::unique_lock<std::mutex> lock(m_state->mutex);
            m_state->scheduler = m_scheduler;
            m_state->invocations.push(std::make_tuple(std::forward<Args>(args)...));
        }
        m_scheduler->notify();
    }
};

namespace detail {
template <typename T>
struct extract_signature_impl {};
template <typename Sig>
struct extract_signature_impl<std::function<Sig>> {
    using signature = Sig;
};

template <typename T>
using extract_signature = typename extract_signature_impl<T>::signature;
}

// Use std::function deduction guides for EventLoopDispatcher. This works with function pointers, lambdas (without
// auto parameters), and any other function object that has a non-overloaded, non-templated call operator.
template<typename T,
         typename Sig = detail::extract_signature<decltype(std::function(std::declval<T>()))>>
EventLoopDispatcher(const T&) -> EventLoopDispatcher<Sig>;

} // namespace util
} // namespace realm

#endif

