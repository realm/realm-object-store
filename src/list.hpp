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

#ifndef REALM_LIST_HPP
#define REALM_LIST_HPP

#include "collection_notifications.hpp"
#include "impl/collection_notifier.hpp"

#include "util/compiler.hpp"

#include <realm/data_type.hpp>
#include <realm/link_view_fwd.hpp>
#include <realm/row.hpp>
#include <realm/table_ref.hpp>

#include <functional>
#include <memory>

namespace realm {
using RowExpr = BasicRowExpr<Table>;

class ObjectSchema;
class Query;
class Realm;
class Results;
class SortDescriptor;
template <typename T> class ThreadSafeReference;

namespace _impl {
class ListNotifier;
}

class List {
public:
    List() noexcept;
    List(std::shared_ptr<Realm> r, Table& parent_table, size_t col, size_t row);
    List(std::shared_ptr<Realm> r, LinkViewRef l) noexcept;
    List(std::shared_ptr<Realm> r, TableRef l) noexcept;
    ~List();

    List(const List&);
    List& operator=(const List&);
    List(List&&);
    List& operator=(List&&);

    const std::shared_ptr<Realm>& get_realm() const { return m_realm; }
    Query get_query() const;
    const ObjectSchema& get_object_schema() const;
    size_t get_origin_row_index() const;

    bool is_valid() const;
    void verify_attached() const;
    void verify_in_transaction() const;

    size_t size() const;

    void move(size_t source_ndx, size_t dest_ndx);
    void remove(size_t list_ndx);
    void remove_all();
    void swap(size_t ndx1, size_t ndx2);
    void delete_all();

    template<typename T = RowExpr>
    T get(size_t row_ndx) const;
    size_t get_unchecked(size_t row_ndx) const noexcept;
    template<typename T>
    size_t find(T const& value) const;

    template<typename T>
    void add(T value);
    template<typename T>
    void insert(size_t list_ndx, T value);
    template<typename T>
    void set(size_t row_ndx, T value);

    // Get the min/max/average/sum of the values in the list
    // All but sum() returns none when there are zero matching rows
    // sum() returns 0, except for when it returns none
    // Throws UnsupportedColumnTypeException for sum/average on timestamp or non-numeric column
    // Throws OutOfBoundsIndexException for an out-of-bounds column
    template<typename T> util::Optional<T> max(size_t);
    template<typename T> util::Optional<T> min(size_t);
    template<typename T> util::Optional<T> average(size_t);
    template<typename T> util::Optional<T> sum(size_t);

    Results sort(SortDescriptor order) const;
    Results sort(std::vector<std::pair<std::string, bool>> const& keypaths) const;
    Results filter(Query q) const;

    // Return a Results representing a live view of this List.
    Results as_results() const;

    // Return a Results representing a snapshot of this List.
    Results snapshot() const;

    bool operator==(List const& rgt) const noexcept;

    NotificationToken add_notification_callback(CollectionChangeCallback cb) &;

    template<typename Context>
    auto get(Context&, size_t row_ndx) const;
    template<typename Context>
    auto get_unchecked(Context&, size_t row_ndx) const noexcept;
    template<typename T, typename Context>
    size_t find(Context&, T&& value) const;

    template<typename T, typename Context>
    void add(Context&, T&& value, bool update=false);
    template<typename T, typename Context>
    void insert(Context&, size_t list_ndx, T&& value, bool update=false);
    template<typename T, typename Context>
    void set(Context&, size_t row_ndx, T&& value, bool update=false);

    // Get the min/max/average/sum of the values in the list
    // All but sum() returns none when there are zero matching rows
    // sum() returns 0, except for when it returns none
    // Throws UnsupportedColumnTypeException for sum/average on timestamp or non-numeric column
    template<typename Context>
    auto max(Context&);
    template<typename Context>
    auto min(Context&);
    template<typename Context>
    auto average(Context&);
    template<typename Context>
    auto sum(Context&);

    // The List object has been invalidated (due to the Realm being invalidated,
    // or the containing object being deleted)
    // All non-noexcept functions can throw this
    struct InvalidatedException : public std::logic_error {
        InvalidatedException() : std::logic_error("Access to invalidated List object") {}
    };

    // The input index parameter was out of bounds
    struct OutOfBoundsIndexException : public std::out_of_range {
        OutOfBoundsIndexException(size_t r, size_t c);
        size_t requested;
        size_t valid_count;
    };

private:
    friend ThreadSafeReference<List>;

    std::shared_ptr<Realm> m_realm;
    mutable const ObjectSchema* m_object_schema = nullptr;
    LinkViewRef m_link_view;
    TableRef m_table;
    _impl::CollectionNotifier::Handle<_impl::ListNotifier> m_notifier;

    void verify_valid_row(size_t row_ndx, bool insertion = false) const;
    void validate(RowExpr) const;

    template<typename Fn>
    auto dispatch(Fn&&) const;

    int get_type() const;
    bool is_optional() const noexcept;

    size_t to_table_ndx(size_t row) const noexcept;

    friend struct std::hash<List>;
};

template<typename Fn>
auto List::dispatch(Fn&& fn) const
{
    switch (get_type()) {
        case type_Int:    return is_optional() ? fn((util::Optional<int64_t>*)0) : fn((int64_t*)0);
        case type_Bool:   return is_optional() ? fn((util::Optional<bool>*)0)    : fn((bool*)0);
        case type_Float:  return is_optional() ? fn((util::Optional<float>*)0)   : fn((float*)0);
        case type_Double: return is_optional() ? fn((util::Optional<double>*)0)  : fn((double*)0);
        case type_String: return fn((StringData*)0);
        case type_Binary: return fn((BinaryData*)0);
        case type_Timestamp: return fn((Timestamp*)0);
        case type_Link: return fn((RowExpr*)0);
//        default: REALM_COMPILER_HINT_UNREACHABLE();
        default: REALM_UNREACHABLE();
    }
}

template<typename Context>
auto List::get(Context& ctx, size_t row_ndx) const
{
    return dispatch([&](auto t) { return ctx.box(get<std::decay_t<decltype(*t)>>(row_ndx)); });
}
template<typename Context>
auto List::get_unchecked(Context& ctx, size_t row_ndx) const noexcept
{
    return dispatch([&](auto t) { return ctx.box(get_unchecked<std::decay_t<decltype(*t)>>(row_ndx)); });
}

template<typename T, typename Context>
size_t List::find(Context& ctx, T&& value) const
{
    return dispatch([&](auto t) { return find(ctx.template unbox<std::decay_t<decltype(*t)>>(value)); });
}

template<typename T, typename Context>
void List::add(Context& ctx, T&& value, bool update)
{
    dispatch([&](auto t) { add(ctx.template unbox<std::decay_t<decltype(*t)>>(value, true, update)); });
}

template<typename T, typename Context>
void List::insert(Context& ctx, size_t list_ndx, T&& value, bool update)
{
    dispatch([&](auto t) { insert(list_ndx, ctx.template unbox<std::decay_t<decltype(*t)>>(value, true, update)); });
}

template<typename T, typename Context>
void List::set(Context& ctx, size_t row_ndx, T&& value, bool update)
{
    dispatch([&](auto t) { set(row_ndx, ctx.template unbox<std::decay_t<decltype(*t)>>(value, true, update)); });
}

template<typename Context>
auto List::max(Context& ctx)
{
    return dispatch([&](auto t) { return ctx.box(max<std::decay_t<decltype(*t)>>()); });
}

template<typename Context>
auto List::min(Context& ctx)
{
    return dispatch([&](auto t) { return ctx.box(min<std::decay_t<decltype(*t)>>()); });
}

template<typename Context>
auto List::average(Context& ctx)
{
    return dispatch([&](auto t) { return ctx.box(average<std::decay_t<decltype(*t)>>()); });
}

template<typename Context>
auto List::sum(Context& ctx)
{
    return dispatch([&](auto t) { return ctx.box(sum<std::decay_t<decltype(*t)>>()); });
}
} // namespace realm

namespace std {
template<> struct hash<realm::List> {
    size_t operator()(realm::List const&) const;
};
}

#endif // REALM_LIST_HPP
