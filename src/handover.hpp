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

#ifndef realm_handover_hpp
#define realm_handover_hpp

#include "list.hpp"
#include "object_accessor.hpp"
#include "results.hpp"

#include <realm/group_shared.hpp>
#include <realm/link_view.hpp>
#include <realm/query.hpp>
#include <realm/row.hpp>
#include <realm/table_view.hpp>

namespace realm {
class AnyHandover;
class AnyThreadConfined;

// Type-erased wrapper for any type which must be exported to be handed between threads
class AnyThreadConfined {
public:
    enum class Type {
        Object,
        List,
        Results,
    };
    
    // Constructors
    AnyThreadConfined(Object object)   : m_type(Type::Object),  m_object(object)   { }
    AnyThreadConfined(List list)       : m_type(Type::List),    m_list(list)       { }
    AnyThreadConfined(Results results) : m_type(Type::Results), m_results(results) { }

    AnyThreadConfined(const AnyThreadConfined&);
    AnyThreadConfined& operator=(const AnyThreadConfined&);
    AnyThreadConfined(AnyThreadConfined&&);
    AnyThreadConfined& operator=(AnyThreadConfined&&);
    ~AnyThreadConfined();

    Type get_type() const { return m_type; }
    SharedRealm get_realm() const;

    // Getters
    Object  get_object()  const { REALM_ASSERT(m_type == Type::Object);  return m_object;  }
    List    get_list()    const { REALM_ASSERT(m_type == Type::List);    return m_list;    }
    Results get_results() const { REALM_ASSERT(m_type == Type::Results); return m_results; }

    AnyHandover export_for_handover() const;

private:
    Type m_type;
    union {
        Object  m_object;
        List    m_list;
        Results m_results;
    };
};

// Type-erased wrapper for a `Handover` of an `AnyThreadConfined` value
class AnyHandover {
public:
    AnyHandover(const AnyHandover&) = delete;
    AnyHandover& operator=(const AnyHandover&) = delete;
    AnyHandover(AnyHandover&&);
    AnyHandover& operator=(AnyHandover&&);
    ~AnyHandover();

    // Destination `Realm` version must match that of the source Realm at the time of export
    AnyThreadConfined import_from_handover(SharedRealm realm) &&;

private:
    friend AnyHandover AnyThreadConfined::export_for_handover() const;

    using RowHandover      = std::unique_ptr<SharedGroup::Handover<Row>>;
    using QueryHandover    = std::unique_ptr<SharedGroup::Handover<Query>>;
    using LinkViewHandover = std::unique_ptr<SharedGroup::Handover<LinkView>>;

    AnyThreadConfined::Type m_type;
    union {
        struct {
            RowHandover row_handover;
            std::string object_schema_name;
        } m_object;

        struct {
            LinkViewHandover link_view_handover;
        } m_list;

        struct {
            QueryHandover query_handover;
            SortOrder sort_order;
        } m_results;
    };

    AnyHandover(RowHandover row_handover, std::string object_schema_name) : m_type(AnyThreadConfined::Type::Object),
        m_object({std::move(row_handover), std::move(object_schema_name)}) {}
    AnyHandover(LinkViewHandover link_view) : m_type(AnyThreadConfined::Type::List),
        m_list({std::move(link_view)}) {}
    AnyHandover(QueryHandover query_handover, SortOrder sort_order) : m_type(AnyThreadConfined::Type::Results),
        m_results({std::move(query_handover), sort_order}) {}
};
}

#endif /* realm_handover_hpp */
