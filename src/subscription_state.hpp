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

#ifndef REALM_OS_SUBSCRIPTION_STATE_HPP
#define REALM_OS_SUBSCRIPTION_STATE_HPP

#include <cstdint>

namespace realm {
namespace partial_sync {

// Enum describing the various states a partial sync subscription can have.
// These states are propagated using the standard collection notification system.
enum class SubscriptionState : int8_t {
    Undefined = -3,         // Unknown which state Partial Sync is in.
    NotSupported = - 2,     // Partial Sync not supported.
    Error = -1,             // An error was detect in Partial Sync.
    Uninitialized = 0,      // The subscription was just created, but not handled by sync yet.
    Initialized = 1         // The subscription has been initialized successfully and is syncing data to the device.
};
}
}

#endif // REALM_OS_SUBSCRIPTION_STATE_HPP
