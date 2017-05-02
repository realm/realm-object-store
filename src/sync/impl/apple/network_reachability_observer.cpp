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

#include "sync/impl/apple/network_reachability_observer.hpp"

#include <realm/util/logger.hpp>

#if NETWORK_REACHABILITY_AVAILABLE

using namespace realm;
using namespace realm::_impl;

namespace {

NetworkReachabilityStatus reachability_status_for_flags(SCNetworkReachabilityFlags flags)
{
    if (!(flags & kSCNetworkReachabilityFlagsReachable))
        return NetworkReachabilityStatus::NotReachable;

    if (flags & kSCNetworkReachabilityFlagsConnectionRequired) {
        if (!(flags & kSCNetworkReachabilityFlagsConnectionOnTraffic) ||
            (flags & kSCNetworkReachabilityFlagsInterventionRequired))
            return NetworkReachabilityStatus::NotReachable;
    }

    NetworkReachabilityStatus status = NetworkReachabilityStatus::ReachableViaWiFi;

#if TARGET_OS_IPHONE
    if (flags & kSCNetworkReachabilityFlagsIsWWAN)
        status = ReachableViaWWAN;
#endif

    return status;
}

} // (anonymous namespace)

NetworkReachabilityObserver& NetworkReachabilityObserver::shared(util::Optional<util::Logger&> logger_ref)
{
    // FIXME: is this static property going to screw up our tests?
    static NetworkReachabilityObserver shared;
    if (!shared.start_observing() && logger_ref)
        logger_ref->error("Failed to set up network reachability observer.");

    return shared;
}

NetworkReachabilityObserver::NetworkReachabilityObserver(util::Optional<std::string> hostname)
: m_callback_queue(dispatch_queue_create("io.realm.sync.reachability", DISPATCH_QUEUE_SERIAL))
{
    if (hostname) {
        m_reachability_ref = util::adoptCF(SystemConfiguration::shared().network_reachability_create_with_name(nullptr,
                                                                                                               hostname->c_str()));
    } else {
        struct sockaddr zeroAddress = {};
        zeroAddress.sa_len = sizeof(zeroAddress);
        zeroAddress.sa_family = AF_INET;

        m_reachability_ref = util::adoptCF(SystemConfiguration::shared().network_reachability_create_with_address(nullptr,
                                                                                                                  &zeroAddress));
    }
}

NetworkReachabilityObserver::~NetworkReachabilityObserver()
{
    stop_observing();
    dispatch_release(m_callback_queue);
}

NetworkReachabilityStatus NetworkReachabilityObserver::reachability_status() const
{
    SCNetworkReachabilityFlags flags;

    if (SystemConfiguration::shared().network_reachability_get_flags(m_reachability_ref.get(), &flags))
        return reachability_status_for_flags(flags);

    return NetworkReachabilityStatus::NotReachable;
}

uint64_t NetworkReachabilityObserver::register_observer(ReachabilityCallback&& callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t this_token = ++m_latest_token;
    m_change_handlers[this_token] = callback;
    return this_token;
}

void NetworkReachabilityObserver::unregister_observer(uint64_t token)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_change_handlers.erase(token);
}

bool NetworkReachabilityObserver::start_observing()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currently_observing)
        return true;

    m_previous_status = reachability_status();

    auto callback = [](SCNetworkReachabilityRef, SCNetworkReachabilityFlags, void* self) {
        static_cast<NetworkReachabilityObserver*>(self)->reachability_changed();
    };

    SCNetworkReachabilityContext context = {0, this, nullptr, nullptr, nullptr};

    if (!SystemConfiguration::shared().network_reachability_set_callback(m_reachability_ref.get(), callback, &context))
        return false;

    if (!SystemConfiguration::shared().network_reachability_set_dispatch_queue(m_reachability_ref.get(), m_callback_queue))
        return false;

    m_currently_observing = true;
    return true;
}

void NetworkReachabilityObserver::stop_observing()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_currently_observing)
        return;

    SystemConfiguration::shared().network_reachability_set_dispatch_queue(m_reachability_ref.get(), nullptr);
    SystemConfiguration::shared().network_reachability_set_callback(m_reachability_ref.get(), nullptr, nullptr);

    // Wait for all previously-enqueued blocks to execute to guarantee that
    // no callback will be called after returning from this method
    dispatch_sync(m_callback_queue, ^{});

    m_currently_observing = false;
}

void NetworkReachabilityObserver::reachability_changed()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto current_status = reachability_status();

    // When observing reachability of the specific host the callback might be called
    // several times (because of DNS queries) with the same reachability flags while
    // the caller should be notified only when the reachability status is really changed.
    if (current_status != m_previous_status) {
        for (auto& pair : m_change_handlers) {
            pair.second(current_status);
        }
        m_previous_status = current_status;
    }
}

#endif
