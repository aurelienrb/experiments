#ifndef POLY_LIST_H
#define POLY_LIST_H

/*
    Polymorphic list: a (simply linked) list that can contain various subtypes of a given base class BaseT:
    - unlike std::list, instances are stored contiguously in the same memory chunk (similar to std::vector)
    - unlike std::vector, random access to each element is not possible

    Example:
        // A is the common base class for B and C
        poly_list<A> list;
        list.emplace_back<B>();
        list.emplace_back<C>(10);

        for (auto e : list) {
            cout << e->info() << "\n"; // decltype(e) == A*
        }
*/

#include <functional>
#include <cassert>

template<typename BaseT>
class poly_list {
public:
    class iterator;

    ~poly_list() {
        clear();
        if (buffer) {
            assert(buffer_size);
            delete[] buffer;
            buffer = nullptr;
            buffer_size = 0;
        }
    }

    bool empty() const {
        assert(last_used_entry == nullptr || buffer);
        return (last_used_entry == nullptr);
    }

    // clear does not modify the buffer size
    void clear() {
        if (last_used_entry) {
            assert(buffer);
            destroy_all(first_entry(), last_used_entry);
            last_used_entry = nullptr;
        }
    }

    template<typename ChildT, typename... Args>
    void emplace_back(Args&&... args) {
        static_assert(std::is_base_of<BaseT, ChildT>::value, "emplace_back<T>(): the given type T is not a derived class of BaseT");
        /*static_assert(std::is_abstract<ChildT>::value, "emplace_back<T>(): the given type T is abstract");*/

        // allocate
        auto result = grow(sizeof ChildT, get_poly_functions<ChildT>());
        // (try to) initialise
        new (result.new_object_placeholder) ChildT(std::forward<Args>(args)...);
        // commit
        commit(result);
    }

    iterator begin() {
        return iterator(first_entry(), last_used_entry);
    }

    iterator end() {
        return iterator(nullptr, nullptr);
    }

private:
    friend class iterator;

    struct poly_functions {
        size_t object_size;
        //std::function<void(T*, const T&&)> poly_move;
        std::function<void(BaseT*, const BaseT*)> copy_construct;
        std::function<void(BaseT*)> poly_destruct;
    };

    struct list_entry {
#ifdef _DEBUG
        static const long long magic_value { 0xFFFF11111111AAAA };
        long long magic;
#endif
        const poly_functions * functions;

        list_entry * next_entry() {
            char * address = reinterpret_cast<char *>(this) + sizeof(*this) + functions->object_size;
            list_entry * entry = reinterpret_cast<list_entry*>(address);
            return entry;
        }
        const list_entry * next_entry() const {
            return const_cast<list_entry *>(this)->next_entry();
        }

        size_t total_size() const {
            return sizeof(*this) + object_size();
        }
        size_t object_size() const {
            return functions->object_size;
        }

        char * get_placeholder() {
            return reinterpret_cast<char*>(this) + sizeof(*this);
        }
        const char * get_placeholder() const {
            return const_cast<list_entry *>(this)->get_placeholder();
        }

        template<typename T = BaseT>
        T * get_object() {
            return reinterpret_cast<T*>(get_placeholder());
        }
        template<typename T = BaseT>
        const T * get_object() const {
            return reinterpret_cast<const T*>(get_placeholder());
        }

        void destruct() {
            functions->poly_destruct(get_object());
        }
        void copy_from(const list_entry * other) {
            functions = other->functions;
            functions->copy_construct(get_object(), other->get_object());
#ifdef _DEBUG
            magic = magic_value;
#endif
        }
    };

    // create an empty new entry (only its size member is initialized). The given current_entry can be null
    static list_entry * alloc_entry(const list_entry * current_entry, size_t object_size, char * buffer, size_t buffer_size) {
        std::ptrdiff_t total_used_bytes = current_entry ?
            reinterpret_cast<const char*>(current_entry)+current_entry->total_size() - buffer :
            0;
        assert(total_used_bytes >= 0 && (size_t)total_used_bytes <= buffer_size);

        const size_t required_total_size = sizeof(list_entry) + object_size; // TODO: align size?
        if (total_used_bytes >= 0 && total_used_bytes + required_total_size <= buffer_size) {
            auto new_entry = reinterpret_cast<list_entry *>(buffer + total_used_bytes);
#ifdef _DEBUG
            new_entry->magic = list_entry::magic_value;
#endif
            return new_entry;
        }
        return nullptr;
    }

    // accepts null parameters
    static void destroy_all(list_entry * first_entry, list_entry * last_entry) {
        if (first_entry && last_entry) {
            assert(first_entry <= last_entry);
            for (list_entry * old_entry = first_entry; old_entry <= last_entry; old_entry = old_entry->next_entry()) {
                assert(old_entry->magic == list_entry::magic_value);
                auto p = old_entry->get_object();
                std::string s = p->id();
                old_entry->destruct();
            }
        }
    }

    list_entry * first_entry() {
        return last_used_entry ? reinterpret_cast<list_entry*>(buffer) : nullptr;
    }

    struct uncommitted_growth {
        uncommitted_growth() = default;

        // move ctor: disarm the source object
        uncommitted_growth(uncommitted_growth && other) {
            *this = other;
            other.new_buffer = nullptr;
        }

        char * new_object_placeholder = nullptr; // uninitialized memory
        char * new_buffer = nullptr;
        size_t new_buffer_size = 0;
        list_entry *new_last_used_entry = nullptr;

        ~uncommitted_growth() {
            // rollback if not committed
            if (new_buffer) {
                destroy_all(reinterpret_cast<list_entry*>(new_buffer), new_last_used_entry);
                delete[] new_buffer;
            }
        }
    };

void commit(uncommitted_growth & result) {
    assert(result.new_last_used_entry);

    // is there a new buffer to use?
    if (result.new_buffer) {
        // destroy objects from the current buffer
        destroy_all(first_entry(), last_used_entry);

        // free the current buffer and switch to the new one
        delete[] buffer;
        buffer = result.new_buffer;
        buffer_size = result.new_buffer_size;

        // tag the result as committed
        result.new_buffer = nullptr;
    }

    last_used_entry = result.new_last_used_entry;
};

// Accepts null parameters.
// result is updated after each copy in order to ensure proper rollback if the next copy fails
static void copy_all(list_entry * first_entry, list_entry * last_entry, uncommitted_growth & result) {
    assert(result.new_last_used_entry == nullptr);

    if (first_entry && last_entry) {
        assert(first_entry <= last_entry);

        list_entry * last_copied_entry = nullptr;
        for (const list_entry * old_entry = first_entry; old_entry <= last_entry; old_entry = old_entry->next_entry()) {
            assert(old_entry->magic == list_entry::magic_value);

            last_copied_entry = alloc_entry(last_copied_entry, old_entry->object_size(), result.new_buffer, result.new_buffer_size);
            assert(last_copied_entry);
            // TODO: use placement new?
            last_copied_entry->copy_from(old_entry);

            // sync result (for exception safety)
            result.new_last_used_entry = last_copied_entry;
        }
    }
}

uncommitted_growth grow(size_t object_size, const poly_functions * object_functions) {
    assert(object_size > 0 && object_functions);

    // will rollback the second buffer allocation if an exception is thrown somewhere
    uncommitted_growth result;

    // compute here the size of our new buffer (only if it needs to grow)
    // we double its size from the last time
    const size_t new_buffer_size = (buffer_size + object_size + sizeof(list_entry)) * 2;

    if (buffer_size == 0) {
        assert(last_used_entry == nullptr && buffer == nullptr);
        buffer = new char[new_buffer_size];
        buffer_size = new_buffer_size;
    }

    list_entry * new_entry = alloc_entry(last_used_entry, object_size, buffer, buffer_size);
    if (new_entry == nullptr) {
        // allocate new buffer
        result.new_buffer = new char[new_buffer_size];
        result.new_buffer_size = new_buffer_size;

        // copy elements to the new buffer (if any)
        copy_all(first_entry(), last_used_entry, result);
        if (result.new_last_used_entry == nullptr) { // nothing was copied
            // current buffer doesn't contain any element but is too small for our new single element
            assert(buffer_size < object_size + sizeof(list_entry));
        }

        // append the new object to the new buffer
        new_entry = alloc_entry(result.new_last_used_entry, object_size, result.new_buffer, result.new_buffer_size);
        assert(new_entry);
    }
    assert(new_entry);
    new_entry->functions = object_functions;

    result.new_last_used_entry = new_entry;
    result.new_object_placeholder = new_entry->get_placeholder();
    return result;
}

template<typename ChildT>
static const poly_functions * get_poly_functions() {
    static const poly_functions instance {
        sizeof ChildT,
        [](BaseT * self, const BaseT * other) {
            new (reinterpret_cast<char*>(self)) ChildT(*static_cast<const ChildT*>(other));
        },
            [](BaseT * self) {
            static_cast<ChildT*>(self)->~ChildT();
        }
    };
    return &instance;
}
public:
    class iterator {
    public:
        explicit iterator(list_entry * entry, const list_entry * last_valid) : m_entry(entry), m_last_valid(last_valid) {
            assert(last_valid || (entry == nullptr));
        }
        iterator(const iterator & other) = default;
        BaseT * operator->() {
            assert(m_entry && m_entry <= m_last_valid);
            return m_entry->get_object();
        }
        BaseT & operator*() {
            assert(m_entry && m_entry <= m_last_valid);
            return *m_entry->get_object();
        }
        iterator & operator++() {
            assert(m_entry && m_entry <= m_last_valid);
            if (m_entry != m_last_valid) {
                m_entry = m_entry->next_entry();
                assert(m_entry <= m_last_valid);
            }
            else {
                assert(m_entry && m_last_valid);
                m_entry = nullptr;
                m_last_valid = nullptr;
            }
            return *this;
        }
        iterator operator++(int) {
            assert(m_entry && m_entry <= m_last_valid);
            iterator copy(*this);
            operator++();
            return copy;
        }
        bool operator==(const iterator & other) const {
            assert(m_entry <= m_last_valid);
            assert(other.m_entry <= other.m_last_valid);
            return m_entry == other.m_entry;
        }
        bool operator!=(const iterator & other) const {
            return !operator==(other);
        }
    private:
        list_entry * m_entry;
        const list_entry * m_last_valid;
    };

private:

    size_t buffer_size = 0;
    char  * buffer = nullptr;
    list_entry * last_used_entry = nullptr;
};

#endif
