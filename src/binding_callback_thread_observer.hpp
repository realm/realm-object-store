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

#ifndef REALM_OBJECT_STORE_BINDING_CALLBACK_THREAD_OBSERVER_HPP
#define REALM_OBJECT_STORE_BINDING_CALLBACK_THREAD_OBSERVER_HPP

namespace realm {
    // Interface for bindings interested in registering callbacks before/after the ObjectStore thread runs.
    // This is for example helpful to attach/detach the pthread to the JavaVM in order to be able to perform JNI calls.
    class BindingCallbackThreadObserver {
    public:
        // This method is called just before the thread is started
        virtual void did_create_thread() = 0;

        // This method is called just before the thread is being destroyed
        virtual void will_destroy_thread() = 0;
    };

    extern BindingCallbackThreadObserver* g_sync_client_thread_listener;

    void set_sync_client_thread_listener(BindingCallbackThreadObserver& listener);
}

#endif //REALM_OBJECT_STORE_BINDING_CALLBACK_THREAD_OBSERVER_HPP
