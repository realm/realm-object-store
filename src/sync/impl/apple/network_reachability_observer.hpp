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

#ifndef REALM_OS_NETWORK_REACHABILITY_OBSERVER_HPP
#define REALM_OS_NETWORK_REACHABILITY_OBSERVER_HPP

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include <realm/util/cf_ptr.hpp>
#include <realm/util/optional.hpp>

#include "sync/impl/network_reachability.hpp"

#if NETWORK_REACHABILITY_AVAILABLE

#include "sync/impl/apple/system_configuration.hpp"

namespace realm {

namespace util {
    class Logger;
}

enum class NetworkReachabilityStatus {
    NotReachable,
    ReachableViaWiFi,
    ReachableViaWWAN
};

using ReachabilityCallback = std::function<void(const NetworkReachabilityStatus)>;

class NetworkReachabilityObserver {
public:
    static NetworkReachabilityObserver& shared(util::Optional<util::Logger&> logger_ref=none);

    /// Get the current reachability status.
    NetworkReachabilityStatus reachability_status() const;

    /// Register a callback to be called whenever the reachability status changes.
    /// This method returns a token that can later be used to unregister the callback.
    uint64_t register_observer(ReachabilityCallback&& callback);

    /// Unregister a previously-registered callback based on the token value. If the
    /// token value is invalid, nothing happens.
    void unregister_observer(uint64_t token);

private:
    NetworkReachabilityObserver(util::Optional<std::string> hostname=none);
    ~NetworkReachabilityObserver();

    bool start_observing();
    void stop_observing();
    void reachability_changed();

    std::mutex m_mutex;

    bool m_currently_observing;
    uint64_t m_latest_token = 0;
    std::unordered_map<int64_t, ReachabilityCallback> m_change_handlers;

    util::CFPtr<SCNetworkReachabilityRef> m_reachability_ref;
    NetworkReachabilityStatus m_previous_status;
    dispatch_queue_t m_callback_queue;
};

} // namespace realm

#endif // NETWORK_REACHABILITY_AVAILABLE

#endif // REALM_OS_NETWORK_REACHABILITY_OBSERVER_HPP
