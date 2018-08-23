////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
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

#include "list.hpp"

#include "impl/list_notifier.hpp"
#include "impl/realm_coordinator.hpp"
#include "object_schema.hpp"
#include "object_store.hpp"
#include "results.hpp"
#include "schema.hpp"
#include "shared_realm.hpp"

namespace {
using namespace realm;

template<typename T>
struct ListType {
    using type = Lst<T>;
};

template<>
struct ListType<Obj> {
    using type = LnkLst;
};
} // anonymous namespace

namespace realm {
using namespace _impl;

ListView::ListView(std::shared_ptr<LstBase> list) noexcept
: m_list_base(list) { }

ListView::ListView(ListView const& l) noexcept
: m_list_base(l.m_list_base), m_sort(l.m_sort), m_distinct(l.m_distinct)
{ }

ListView& ListView::operator=(ListView const& l) noexcept
{
    m_list_base = l.m_list_base;
    m_distinct = l.m_distinct;
    m_sort = l.m_sort;
    m_rows.reset();
    return *this;
}

void ListView::update_if_needed() const
{
    if (m_sort == Direction::none && !m_distinct)
        return;
    if (m_rows && !m_list_base->update_if_needed())
        return;
    auto size = m_list_base->size();
    if (!m_rows || size != m_rows[0]) {
        m_rows.reset(new size_t[size + 1]);
        m_rows[0] = size;
    }
    if (size == 0)
        return;

    for (size_t i = 0; i < size; ++i)
        m_rows[i + 1] = i;
    m_list_base->get_sorted(m_sort != Direction::none, m_sort != Direction::descending, m_distinct, m_rows.get());
}

size_t ListView::size() const
{
    update_if_needed();
    return m_rows ? m_rows[0] : m_list_base->size();
}

size_t ListView::to_list_index(size_t row) const
{
    return m_rows ? m_rows[row + 1] : row;
}

size_t ListView::from_list_index(size_t row) const
{
    if (row != not_found && m_rows) {
        auto end = &m_rows[m_rows[0] + 1];
        auto it = std::find(&m_rows[1], end, row);
        return it == end ? not_found : it - &m_rows[1];
    }
    return row;
}

template<typename T>
T ListView::get(size_t row_ndx) const
{
    update_if_needed();
    return get_as<T>().get(to_list_index(row_ndx));
}

template<>
Obj ListView::get(size_t row_ndx) const
{
    update_if_needed();
    auto& list = get_as<Obj>();
    return list.get_target_table().get_object(list.get(to_list_index(row_ndx)));
}

template<typename T>
size_t ListView::find(T const& value) const
{
    update_if_needed();
    return from_list_index(get_as<T>().find_first(value));
}

template<>
size_t ListView::find(Obj const& o) const
{
    update_if_needed();
    return from_list_index(get_as<Obj>().ConstLstIf<ObjKey>::find_first(o.get_key()));
}

void ListView::clear()
{
    update_if_needed();
    if (m_rows) {
        std::sort(&m_rows[1], &m_rows[m_rows[0] + 1], std::greater<size_t>());
        for (size_t i = 0; i < m_rows[0]; ++i)
            m_list_base->remove(m_rows[i + 1], m_rows[i + 1] + 1);
    }
    else {
        m_list_base->clear();
    }
}

List::List() noexcept = default;
List::~List() = default;

List::List(const List&) = default;
List& List::operator=(const List&) = default;
List::List(List&&) = default;
List& List::operator=(List&&) = default;

List::List(std::shared_ptr<Realm> r, const Obj& parent_obj, ColKey col)
: m_realm(std::move(r))
, m_type(ObjectSchema::from_core_type(*parent_obj.get_table(), col) & ~PropertyType::Array)
, m_list(get_list(m_type, parent_obj, col))
{
}

List::List(std::shared_ptr<Realm> r, const LstBase& list)
: m_realm(std::move(r))
, m_type(ObjectSchema::from_core_type(*list.get_table(), list.get_col_key()) & ~PropertyType::Array)
, m_list(get_list(m_type, list))
{
}

std::unique_ptr<LstBase> List::get_list(PropertyType type, const Obj& parent_obj, ColKey col)
{
    return switch_on_type(type, [&](auto t) {
        using T = std::decay_t<decltype(*t)>;
        return std::unique_ptr<LstBase>(new typename ListType<T>::type(parent_obj, col));
    });
}

std::unique_ptr<LstBase> List::get_list(PropertyType type, const LstBase& list)
{
    return switch_on_type(type, [&](auto t) {
        using T = std::decay_t<decltype(*t)>;
        auto& l = static_cast<const typename ListType<T>::type&>(list);
        return std::unique_ptr<LstBase>(new typename ListType<T>::type(l));
    });
}

static StringData object_name(Table const& table)
{
    return ObjectStore::object_type_for_table_name(table.get_name());
}

ObjectSchema const& List::get_object_schema() const
{
    verify_attached();

    REALM_ASSERT(get_type() == PropertyType::Object);
    if (!m_object_schema) {
        auto object_type = object_name(m_list.get_as<Obj>().get_target_table());
        auto it = m_realm->schema().find(object_type);
        REALM_ASSERT(it != m_realm->schema().end());
        m_object_schema = &*it;
    }
    return *m_object_schema;
}

Query List::get_query() const
{
    verify_attached();
    if (m_type == PropertyType::Object)
        return m_list.get_as<Obj>().get_target_table().where(m_list.get_as<Obj>());
    throw std::runtime_error("not implemented");
}

ObjKey List::get_parent_object_key() const
{
    verify_attached();
    return m_list.get_list_base().get_key();
}

ColKey List::get_parent_column_key() const
{
    verify_attached();
    return m_list.get_list_base().get_col_key();
}

TableKey List::get_parent_table_key() const
{
    verify_attached();
    return m_list.get_list_base().get_table()->get_key();
}

void List::verify_valid_row(size_t row_ndx, bool insertion) const
{
    size_t s = size();
    if (row_ndx > s || (!insertion && row_ndx == s)) {
        throw OutOfBoundsIndexException{row_ndx, s + insertion};
    }
}

template<typename T>
void List::validate(T const&) const { }

template<>
void List::validate(Obj const& obj) const
{
    if (!obj.is_valid())
        throw std::invalid_argument("Object has been deleted or invalidated");
    auto& target = m_list.get_as<Obj>().get_target_table();
    if (obj.get_table() != &target)
        throw std::invalid_argument(util::format("Object of type (%1) does not match List type (%2)",
                                                 object_name(*obj.get_table()),
                                                 object_name(target)));
}

template<typename T>
auto List::to_core_type(T const& value) const
{
    return value;
}

template<>
auto List::to_core_type(Obj const& value) const
{
    return value.get_key();
}

bool List::is_valid() const
{
    if (!m_realm)
        return false;
    m_realm->verify_thread();
    if (!m_realm->is_in_read_transaction())
        return false;
    return m_list.get_list_base().is_attached();
}

void List::verify_attached() const
{
    if (!is_valid()) {
        throw InvalidatedException();
    }
}

void List::verify_in_transaction() const
{
    verify_attached();
    m_realm->verify_in_write();
}

size_t List::size() const
{
    verify_attached();
    return m_list.size();
}

template<typename T>
T List::get(size_t row_ndx) const
{
    verify_valid_row(row_ndx);
    return m_list.get<T>(row_ndx);
}

template<typename T>
size_t List::find(T const& value) const
{
    verify_attached();
    return m_list.find(value);
}

template<>
size_t List::find(Obj const& o) const
{
    verify_attached();
    if (!o.is_valid())
        return not_found;
    validate(o);
    return m_list.find(o);
}

size_t List::find(Query&& q) const
{
    verify_attached();
    if (m_type == PropertyType::Object) {
        ObjKey key = get_query().and_query(std::move(q)).find();
        return key ? m_list.find(key) : not_found;
    }
    throw std::runtime_error("not implemented");
}

template<typename T>
void List::add(T value)
{
    verify_in_transaction();
    validate(value);
    m_list.get_as<T>().add(to_core_type(value));
}

template<typename T>
void List::insert(size_t row_ndx, T value)
{
    verify_in_transaction();
    verify_valid_row(row_ndx, true);
    validate(value);
    m_list.get_as<T>().insert(row_ndx, to_core_type(value));
}

void List::move(size_t source_ndx, size_t dest_ndx)
{
    verify_in_transaction();
    verify_valid_row(source_ndx);
    verify_valid_row(dest_ndx); // Can't be one past end due to removing one earlier
    if (source_ndx == dest_ndx)
        return;

    m_list.get_list_base().move(source_ndx, dest_ndx);
}

void List::remove(size_t row_ndx)
{
    verify_in_transaction();
    verify_valid_row(row_ndx);
    m_list.get_list_base().remove(row_ndx, row_ndx + 1);
}

void List::remove_all()
{
    verify_in_transaction();
    m_list.clear();
}

template<typename T>
void List::set(size_t row_ndx, T value)
{
    verify_in_transaction();
    verify_valid_row(row_ndx);
    validate(value);
    m_list.get_as<T>().set(row_ndx, to_core_type(value));
}

void List::swap(size_t ndx1, size_t ndx2)
{
    verify_in_transaction();
    verify_valid_row(ndx1);
    verify_valid_row(ndx2);
    m_list.get_list_base().swap(ndx1, ndx2);
}

void List::delete_at(size_t row_ndx)
{
    verify_in_transaction();
    verify_valid_row(row_ndx);
    if (m_type == PropertyType::Object)
        m_list.get_as<Obj>().remove_target_row(row_ndx);
    else
        m_list.get_list_base().remove(row_ndx, row_ndx + 1);
}

void List::delete_all()
{
    verify_in_transaction();
    if (m_type == PropertyType::Object)
        m_list.get_as<Obj>().remove_all_target_rows();
    else
        m_list.clear();
}

Results List::sort(SortDescriptor order) const
{
    verify_attached();
    DescriptorOrdering o;
    o.append_sort(std::move(order));
    return Results(m_realm, m_list, m_type, util::none, std::move(o));
}

Results List::sort(std::vector<std::pair<std::string, bool>> const& keypaths) const
{
    return as_results().sort(keypaths);
}

Results List::filter(Query q) const
{
    verify_attached();
    return Results(m_realm, m_list, m_type, get_query().and_query(std::move(q)));
}

Results List::as_results() const
{
    verify_attached();
    return Results(m_realm, m_list, m_type);
}

Results List::snapshot() const
{
    return as_results().snapshot();
}

util::Optional<Mixed> List::max(ColKey col)
{
    return as_results().max(col);
}

util::Optional<Mixed> List::min(ColKey col)
{
    return as_results().min(col);
}

Mixed List::sum(ColKey col)
{
    return *as_results().sum(col);
}

util::Optional<double> List::average(ColKey col)
{
    return as_results().average(col);
}

bool List::operator==(List const& rgt) const noexcept
{
    return m_list.get_list_base().get_table() == rgt.m_list.get_list_base().get_table()
        && m_list.get_list_base().get_key() == rgt.m_list.get_list_base().get_key()
        && m_list.get_list_base().get_col_key() == rgt.m_list.get_list_base().get_col_key();
}

NotificationToken List::add_notification_callback(CollectionChangeCallback cb) &
{
    verify_attached();
    // Adding a new callback to a notifier which had all of its callbacks
    // removed does not properly reinitialize the notifier. Work around this by
    // recreating it instead.
    // FIXME: The notifier lifecycle here is dumb (when all callbacks are removed
    // from a notifier a zombie is left sitting around uselessly) and should be
    // cleaned up.
    if (m_notifier && !m_notifier->have_callbacks())
        m_notifier.reset();
    if (!m_notifier) {
        m_notifier = std::make_shared<ListNotifier>(m_realm, m_list.get_list_base(), m_type);
        RealmCoordinator::register_notifier(m_notifier);
    }
    return {m_notifier, m_notifier->add_callback(std::move(cb))};
}

List::OutOfBoundsIndexException::OutOfBoundsIndexException(size_t r, size_t c)
: std::out_of_range(util::format("Requested index %1 greater than max %2", r, c - 1))
, requested(r), valid_count(c) {}

#define REALM_PRIMITIVE_LIST_TYPE(T) \
    template T List::get<T>(size_t) const; \
    template size_t List::find<T>(T const&) const; \
    template void List::add<T>(T); \
    template void List::insert<T>(size_t, T); \
    template void List::set<T>(size_t, T);

REALM_PRIMITIVE_LIST_TYPE(bool)
REALM_PRIMITIVE_LIST_TYPE(int64_t)
REALM_PRIMITIVE_LIST_TYPE(float)
REALM_PRIMITIVE_LIST_TYPE(double)
REALM_PRIMITIVE_LIST_TYPE(StringData)
REALM_PRIMITIVE_LIST_TYPE(BinaryData)
REALM_PRIMITIVE_LIST_TYPE(Timestamp)
REALM_PRIMITIVE_LIST_TYPE(ObjKey)
REALM_PRIMITIVE_LIST_TYPE(Obj)
REALM_PRIMITIVE_LIST_TYPE(util::Optional<bool>)
REALM_PRIMITIVE_LIST_TYPE(util::Optional<int64_t>)
REALM_PRIMITIVE_LIST_TYPE(util::Optional<float>)
REALM_PRIMITIVE_LIST_TYPE(util::Optional<double>)

#undef REALM_PRIMITIVE_LIST_TYPE
} // namespace realm

namespace {
size_t hash_combine() { return 0; }
template<typename T, typename... Rest>
size_t hash_combine(const T& v, Rest... rest)
{
    size_t h = hash_combine(rest...);
    h ^= std::hash<T>()(v) + 0x9e3779b9 + (h<<6) + (h>>2);
    return h;
}
}

namespace std {
size_t hash<List>::operator()(List const& list) const
{
    auto& impl = list.m_list.get_list_base();
    return hash_combine(impl.get_key().value, impl.get_table()->get_key().value,
                        impl.get_col_key().value);
}
}
