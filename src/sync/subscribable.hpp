////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020 Realm Inc.
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

#ifndef REALM_SUBSCRIBABLE_HPP
#define REALM_SUBSCRIBABLE_HPP

#include <cstdint>
#include <unordered_map>

namespace realm {

/// Generic subscribable that allows for coarse, manual notifications
/// from class type T.
template <class T>
struct Subscribable {
    /// Token that identifies an observer.
    /// Unsubscribes when deconstructed to
    /// avoid dangling observers.
    struct Token {
        Token(Subscribable& subscribable, uint64_t token)
        : m_subscribable(subscribable)
        , m_token(std::make_unique<uint64_t>(token))
        {
        }
        Token(Token&& other)
        : m_subscribable(std::move(other.m_subscribable))
        , m_token(std::move(other.m_token))
        {
        }
        Token& operator=(Token&& other) {
            m_subscribable = std::move(other.m_subscribable);
            m_token = std::move(other.m_token);
        }
        Token(const Token&) = delete;
        Token& operator=(const Token&) = delete;

        ~Token()
        {
            if (m_token) {
                m_subscribable.get().unsubscribe(*this);
            }
        }

        uint64_t value() const
        {
            return *m_token;
        }
    private:
        std::reference_wrapper<Subscribable> m_subscribable;
        std::unique_ptr<uint64_t> m_token;

        template <class U> friend struct Subscribable;
    };

    using observer_t = std::function<void(const T&)>;

    /// Subscribe to notifications for class type T. Any mutation to the T class
    /// will trigger the observer. Notifying subscribers must be done manually
    /// by the Subscribable.
    /// @param observer callback to be called on mutation
    /// @returns a token identifying the observer
    [[maybe_unused]] Token subscribe(observer_t&& observer)
    {
        static uint64_t m_token;
        m_subscribers.insert({m_token, std::move(observer)});
        return Token {*this, m_token++};
    }

    /// Unsubscribe to notifications for this Subscribable using the
    /// token returned when calling `subscribe`.
    /// @param token the token identifying the observer.
    void unsubscribe(const Token& token)
    {
        m_subscribers.erase(*token.m_token);
    }

    /// A count of subscribers subscribed to class T.
    /// @return the amount of subscribers subscribed to class T.
    size_t subscribers_count() const
    {
        return m_subscribers.size();
    }
protected:
    std::unordered_map<uint64_t, observer_t> m_subscribers;
};

} // namespace realm

#endif /* REALM_SUBSCRIBABLE_HPP */
