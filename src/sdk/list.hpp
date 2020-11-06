#ifndef realm_sdk_list_hpp
#define realm_sdk_list_hpp

#include "accessor.hpp"
#include "property.hpp"
#include "type_info.hpp"
#include <type_traits>

namespace realm::sdk {

template <typename T>
struct Object;

// MARK: List

template <class T>
struct Property<std::vector<T>, typename std::enable_if_t<!std::is_pointer_v<T>>>
: private PropertyBase
{
    static_assert(!std::is_base_of_v<Object<T>, T>, "Cannot create List for Object Type");

    using base = PropertyBase;
    using base::base;
    using base::is_managed;

    void push_back(T&& value)
    {
        if (!base::is_managed()) {
            return m_value.push_back(std::move(value));
        }

        base::m_accessor.as<std::vector<T>>().add(value);
    }

    void push_back(T& value)
    {
        if (!base::is_managed()) {
            return m_value.push_back(value);
        }

        base::m_accessor.as<std::vector<T>>().add(value);
    }

    T& at(size_t idx)
    {
        if (!base::is_managed()) {
            return m_value.at(idx);
        }

        m_value[idx] = base::m_accessor.as<std::vector<T>>().get(idx);
        return m_value[idx];
    }

    T& operator[](size_t idx)
    {
        if (!base::is_managed()) {
            return m_value[idx];
        }

        auto value = m_accessor.as<std::vector<T>>().get(idx);
        m_value.insert(m_value.begin() + idx, std::move(value));
        return m_value[idx];
    }

    size_t size() const
    {
        if (!is_managed()) {
            return m_value.size();
        }

        return base::m_accessor.as<std::vector<T>>().size();
    }

    class Iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef const T value_type;
        typedef ptrdiff_t difference_type;
        typedef const T* pointer;
        typedef const T& reference;

        Iterator(Property<std::vector<T>>* l, size_t ndx)
            : m_list(l)
            , m_ndx(ndx)
        {
        }
        pointer operator->()
        {
            m_val = m_list->at(m_ndx);
            return &m_val;
        }
        reference operator*()
        {
            return *operator->();
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
        Property<std::vector<T>>* m_list;
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

    const std::vector<T> value() const
    {
        if (!is_managed()) {
            return m_value;
        }

        auto& accessor = m_accessor.as<std::vector<T>>();
        return accessor.to_value_type(accessor.get());
    }

    void attach_and_write(SharedRealm r, TableRef table, ObjKey key) override
    {
        PropertyBase::attach(r, table, key);
        m_accessor.as<std::vector<T>>().set(m_value);
    }

private:
    std::vector<T> m_value;

    template <typename Impl>
    friend struct ObjectBase;
    friend struct PropertyFriend;
};

// MARK: ListAccessor

template <typename T>
struct Accessor<std::vector<T>, typename std::enable_if_t<!std::is_pointer_v<T>>> : public AccessorBase {
    using base = AccessorBase;
    using base::base;
    using type_info = TypeInfo<T>;
    using value_accessor = Accessor<T>;

    Accessor(const AccessorBase& o) : base(o)
    {
    }

    Lst<typename type_info::realm_type> get()
    {
        return base::get_obj().template get_list<typename type_info::realm_type>(base::get_column_key());
    }
    ConstLst<typename type_info::const_realm_type> get() const
    {
        return base::get_obj().template get_list<typename type_info::const_realm_type>(base::get_column_key());
    }

    void set(std::vector<T>& values)
    {
        for (auto& v : values) {
            if constexpr (util::is_embedded_v<T>) {
                auto lst = base::get_obj().get_linklist(base::get_column_key());
                auto obj = lst.create_and_insert_linked_object(lst.size());
                v.attach_and_write(realm(), obj.get_table(), obj.get_key());
            } else {
                auto lst = base::get_obj().template get_list<typename type_info::realm_type>(base::get_column_key());
                lst.add(value_accessor(static_cast<const base&>(*this)).to_realm_type(v));
            }
        }
    }

    std::vector<T> to_value_type(const ConstLst<typename type_info::const_realm_type>& realm_value) const
    {
        std::vector<T> values;
        for (const auto v : realm_value) {
            values.push_back(value_accessor(static_cast<const base&>(*this)).to_value_type(v));
        }
        return values;
    }

    T get(size_t idx) const
    {
        auto lst = base::get_obj().template get_list<typename type_info::const_realm_type>(base::get_column_key());
        return value_accessor(static_cast<const base&>(*this)).to_value_type(lst.get(idx));
    }

    void add(T& value)
    {
        if constexpr (util::is_embedded_v<T>) {
            auto lst = base::get_obj().get_linklist(base::get_column_key());
            auto obj = lst.create_and_insert_linked_object(lst.size());
            value.attach_and_write(realm(), obj.get_table(), obj.get_key());
        } else {
            auto lst = base::get_obj().template get_list<typename type_info::const_realm_type>(base::get_column_key());
            lst.add(value_accessor(static_cast<const base&>(*this)).to_realm_type(value));
        }
    }

    size_t size() const
    {
        auto lst = base::get_obj().template get_list<typename type_info::const_realm_type>(base::get_column_key());
        return lst.size();
    }

    auto begin() const
    {
        auto lst = base::get_obj().template get_list<typename type_info::const_realm_type>(base::get_column_key());
        return lst.begin();
    }

    auto end() const
    {
        auto lst = base::get_obj().template get_list<typename type_info::const_realm_type>(base::get_column_key());
        return lst.end();
    }
};

// MARK: LinkList

template <class T>
struct Property<std::vector<T>, typename std::enable_if_t<std::is_pointer_v<T>>>
: private PropertyBase
{
    using removed_pointer_t = std::remove_pointer_t<T>;
    Property() = default;
    Property(const Property& o) {
        *this = o;
    }
    Property& operator=(const Property& o) {
        m_unmanaged_value = o.m_unmanaged_value;
        return *this;
    }
    Property(Property&& o) {
        *this = std::move(o);
    }
    Property& operator=(Property&& o) {
        m_unmanaged_value = std::move(o.m_unmanaged_value);
        return *this;
    }
    void attach_and_write(SharedRealm r, TableRef table, ObjKey key) override
    {
        PropertyBase::attach(r, table, key);
        m_accessor.template as<std::vector<T>>().set(m_unmanaged_value);
    }

    bool is_managed() const
    {
        return m_accessor.is_valid();
    }

    void push_back(T value)
    {
        if (!is_managed()) {
            return m_unmanaged_value.push_back(value);
        }

        m_accessor.template as<std::vector<T>>().add(value);
    }

    T& at(size_t idx)
    {
        if (!is_managed()) {
            return m_unmanaged_value.at(idx);
        }

        m_value[idx] = m_accessor.template as<std::vector<T>>().get(idx);
        return m_value[idx];
    }

    T operator[](size_t idx)
    {
        if (!is_managed()) {
            return m_unmanaged_value[idx];
        }

        auto value = m_accessor.template as<std::vector<T>>().get(idx);
        if (m_value.size() > idx) {
            m_value[idx] = std::move(value);
        } else {
            m_value.emplace(m_value.begin() + idx, std::move(value));
        }
        return &m_value[idx];
    }

    size_t size() const
    {
        if (!is_managed()) {
            return m_unmanaged_value.size();
        }

        return m_accessor.template as<std::vector<T>>().size();
    }
private:
    std::vector<T> m_unmanaged_value;
    std::vector<removed_pointer_t> m_value;

    template <typename Impl>
    friend struct ObjectBase;
    friend struct PropertyFriend;
};

// MARK: LinkList Accessor
template <typename T>
struct Accessor<std::vector<T>, typename std::enable_if_t<std::is_pointer_v<T>>> : public AccessorBase {
    using base = AccessorBase;
    using base::base;
    using type_info = TypeInfo<T>;
    using value_accessor = Accessor<T>;
    using removed_pointer_t = std::remove_pointer_t<T>;

    Accessor(const AccessorBase& o) : base(o)
    {
    }

    Lst<typename type_info::realm_type> get()
    {
        return base::get_obj().template get_list<typename type_info::realm_type>(base::get_column_key());
    }
    ConstLst<typename type_info::const_realm_type> get() const
    {
        return base::get_obj().template get_list<typename type_info::const_realm_type>(base::get_column_key());
    }

    void set(std::vector<T>& values)
    {
        std::vector<typename type_info::realm_type> realm_values;
        for (auto& v : values) {
            base::get_obj().template get_list<typename type_info::realm_type>(base::get_column_key()).add(value_accessor(static_cast<const base&>(*this)).to_realm_type(*v));
        }
    }

    std::vector<T> to_value_type(const ConstLst<typename type_info::const_realm_type>& realm_value) const
    {
        std::vector<T> values;
        for (const auto v : realm_value) {
            values.push_back(value_accessor(static_cast<const base&>(*this)).to_value_type(v));
        }
        return values;
    }

    removed_pointer_t get(size_t idx) const
    {
        auto lst = base::get_obj().template get_list<typename type_info::const_realm_type>(base::get_column_key());
        return value_accessor(static_cast<const base&>(*this)).to_value_type(lst.get(idx));
    }

    void add(T& value)
    {
        auto lst = base::get_obj().template get_list<typename type_info::const_realm_type>(base::get_column_key());
        lst.add(value_accessor(static_cast<const base&>(*this)).to_realm_type(*value));
    }

    size_t size() const
    {
        auto lst = base::get_obj().template get_list<typename type_info::const_realm_type>(base::get_column_key());
        return lst.size();
    }
};

template <typename T>
using List = Property<std::vector<T>>;

template <typename T>
static bool operator==(const List<T>& lhs, const List<T>& rhs)
{
    for (size_t i = 0; i < lhs.size(); i++) {
        if (lhs.get(i) != rhs.get(i)) {
            return false;
        }
    }
    return true;
}

template <typename T>
static bool operator!=(const List<T>& lhs, const List<T>& rhs)
{
    return !(lhs == rhs);
}


}
#endif /* list_hpp */
