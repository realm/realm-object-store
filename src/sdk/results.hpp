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

#ifndef realm_sdk_results_hpp
#define realm_sdk_results_hpp

#include "../shared_realm.hpp"
#include "../results.hpp"
#include "util.hpp"

#include "../object_store.hpp"
#include <realm/obj.hpp>

namespace realm::sdk {

using CoreResults = realm::Results;

template <typename T>
struct Results {
    realm::util::Optional<T> first()
    {
        if (auto first = m_results.first()) {
            auto def = T();
            def.attach(m_results.get_realm(), first->get_table(), first->get_key());
            return def;
        }

        return realm::util::none;
    }

    class Iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef const T value_type;

        Iterator(Results<T>* l, size_t ndx)
            : m_list(l)
            , m_ndx(ndx)
        {
        }
        T operator->()
        {
            return m_list->get(m_ndx);
        }
        T operator*()
        {
            return m_list->get(m_ndx);
        }
        Iterator& operator++()
        {
            m_ndx++;
            return *this;
        }
        Iterator operator++(int)
        {
            Iterator tmp(*this);
            operator++();
            return tmp;
        }

        bool operator!=(const Iterator& rhs)
        {
            return m_ndx != rhs.m_ndx;
        }

        bool operator==(const Iterator& rhs)
        {
            return m_ndx == rhs.m_ndx;
        }

    private:
        T m_val;
        Results<T>* m_list;
        size_t m_ndx;
    };

    auto begin()
    {
        return Iterator(this, 0);
    }

    auto end()
    {
        return Iterator(this, size());
    }

    T at(size_t idx)
    {
        return get(idx);
    }
    T get(size_t idx);

    size_t size()
    {
        return m_results.size();
    }
private:
    Results(const SharedRealm& r);

    CoreResults m_results;
    friend struct Realm;
};

template <typename T>
Results<T>::Results(const SharedRealm& r)
: m_results(r, ObjectStore::table_for_object_type(r->read_group(), util::demangle(typeid(T).name())))
{
}

template <typename T>
T Results<T>::get(size_t idx) {
    auto managed = T();
    auto obj = m_results.get<Obj>(idx);
    managed.attach(m_results.get_realm(), obj.get_table(), obj.get_key());
    return managed;
}

}

#endif /* results_hpp */
