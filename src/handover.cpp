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

#include "handover.hpp"

using namespace realm;

AnyThreadConfined::AnyThreadConfined(const AnyThreadConfined& thread_confined)
{
    switch (thread_confined.m_type) {
        case Type::Object:
            new (&m_object) Object(thread_confined.m_object);
            break;

        case Type::List:
            new (&m_list) List(thread_confined.m_list);
            break;

        case Type::Results:
            new (&m_results) Results(thread_confined.m_results);
            break;
    }
    new (&m_type) Type(thread_confined.m_type);
}

AnyThreadConfined::AnyThreadConfined(AnyThreadConfined&& thread_confined)
{
    switch (thread_confined.m_type) {
        case Type::Object:
            new (&m_object) Object(std::move(thread_confined.m_object));
            break;

        case Type::List:
            new (&m_list) List(std::move(thread_confined.m_list));
            break;

        case Type::Results:
            new (&m_results) Results(std::move(thread_confined.m_results));
            break;
    }
    new (&m_type) Type(std::move(thread_confined.m_type));
}

AnyThreadConfined::~AnyThreadConfined()
{
    switch (m_type) {
        case Type::Object:
            m_object.~Object();
            break;

        case Type::List:
            m_list.~List();
            break;

        case Type::Results:
            m_results.~Results();
            break;
    }
}

SharedRealm AnyThreadConfined::get_realm() const
{
    switch (m_type) {
        case Type::Object:
            return m_object.realm();

        case Type::List:
            return m_list.get_realm();

        case Type::Results:
            return m_results.get_realm();
    }
}

AnyHandover AnyThreadConfined::export_for_handover() const
{
    SharedGroup& shared_group = Realm::Internal::get_shared_group(*get_realm());
    switch (m_type) {
        case AnyThreadConfined::Type::Object:
            return AnyHandover(shared_group.export_for_handover(m_object.row()), &m_object.get_object_schema());

        case AnyThreadConfined::Type::List:
            return AnyHandover(shared_group.export_linkview_for_handover(m_list.get_linkview()));

        case AnyThreadConfined::Type::Results:
            return AnyHandover(shared_group.export_for_handover(m_results.get_query(), ConstSourcePayload::Copy),
                               m_results.get_sort());
    }
}

AnyHandover::AnyHandover(AnyHandover&& handover)
{
    switch (handover.m_type) {
        case AnyThreadConfined::Type::Object:
            new (&m_object.row_handover) RowHandover(std::move(handover.m_object.row_handover));
            m_object.object_schema = handover.m_object.object_schema;
            break;

        case AnyThreadConfined::Type::List:
            new (&m_list.link_view_handover) LinkViewHandover(std::move(handover.m_list.link_view_handover));
            break;

        case AnyThreadConfined::Type::Results:
            new (&m_results.query_handover) QueryHandover(std::move(handover.m_results.query_handover));
            new (&m_results.sort_order) SortOrder(std::move(handover.m_results.sort_order));
            break;
    }
    new (&m_type) AnyThreadConfined::Type(handover.m_type);
}

AnyHandover::~AnyHandover()
{
    switch (m_type) {
        case AnyThreadConfined::Type::Object:
            m_object.row_handover.~unique_ptr();
            break;

        case AnyThreadConfined::Type::List:
            m_list.link_view_handover.~unique_ptr();
            break;

        case AnyThreadConfined::Type::Results:
            m_results.query_handover.~unique_ptr();
            m_results.sort_order.~SortOrder();
            break;
    }
}

AnyThreadConfined AnyHandover::import_from_handover(SharedRealm realm) &&
{
    SharedGroup& shared_group = Realm::Internal::get_shared_group(*realm);
    switch (m_type) {
        case AnyThreadConfined::Type::Object: {
            auto row = shared_group.import_from_handover(std::move(m_object.row_handover));
            return AnyThreadConfined(Object(std::move(realm), *m_object.object_schema, std::move(*row)));
        }
        case AnyThreadConfined::Type::List: {
            auto link_view_ref = shared_group.import_linkview_from_handover(std::move(m_list.link_view_handover));
            return AnyThreadConfined(List(std::move(realm), std::move(link_view_ref)));
        }
        case AnyThreadConfined::Type::Results: {
            auto query = shared_group.import_from_handover(std::move(m_results.query_handover));
            return AnyThreadConfined(Results(std::move(realm), std::move(*query), m_results.sort_order));
        }
    }
}
