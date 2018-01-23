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

#ifndef REALM_OS_PARTIAL_SYNC_HPP
#define REALM_OS_PARTIAL_SYNC_HPP

#include "impl/results_notifier.hpp"

#include <functional>
#include <memory>
#include <string>

namespace realm {
class Realm;
class Results;
class Group;

namespace partial_sync {
enum class SubscriptionState : int8_t;

// Returns the default name for subscriptions. Used if a specific name isn't provided.
std::string get_default_name(Query &query);

// Deprecated
void register_query(std::shared_ptr<Realm>, const std::string &object_class, const std::string &query,
					std::function<void (Results, std::exception_ptr)>);

void register_query(Group& group, std::string const& name, std::string const& object_class, std::string const& query,
					_impl::ResultsNotifier& notifier);

void get_query_status(Group& group, std::string const& name, SubscriptionState& new_state, std::string& error);

} // namespace partial_sync
} // namespace realm

#endif // REALM_OS_PARTIAL_SYNC_HPP
