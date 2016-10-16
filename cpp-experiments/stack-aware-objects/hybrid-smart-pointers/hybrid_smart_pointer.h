#ifndef HYBRID_SMART_POINTER_H
#define HYBRID_SMART_POINTER_H

#include "../is_address_in_stack.h"
#include <atomic>
#include <functional>

inline bool is_address_in_cpp_heap(const void * addr) {
    // _CrtIsValidHeapPointer triggers a break point if the given address is in the stack
    // we use is_address_in_stack for now, but it won't work in multithreading tests
    // because it can belong to another stack
    return !is_address_in_stack(addr) && _CrtIsValidHeapPointer(addr);
}

//----------------------------------------------------------------------------
// WARNING: Proof of concept! The following code is far from perfect!
//
// This is a smart pointer class that can use both reference counting
// and reference linking to manage the given object lifetime.
//
// By default it uses reference counting (like shared_ptr), unless:
// - it is the first (and thus unique) instance: the counter creation is
//   delayed to the first copy of that specific instance, unless...
// - if both the source smart pointer and the copied one are in the same stack,
//   the copied instances links itself to the top most smart pointer on
//   its stack instead of increasing the shared counter.
//
// The goal is to test the feasability of that design, and to see if avoiding
// to atomically increment/decrement a shared counter can have a positive
// impact in a heavily multithreaded environment.

template<typename T>
struct make_deleter {
    static void deleter(T * t) {
        delete t;
    }
};

// for testing purpose: total count of shared counter allocation
extern unsigned int total_nb_allocs;

template<typename T>
class hybrid_shared_ptr {
public:
    typedef const decltype(&make_deleter<T>::deleter) deleter_ptr;

    hybrid_shared_ptr(T * t = nullptr) : ptr_to_obj_or_root_ptr_on_stack(reinterpret_cast<size_t>(t)) {
        if (is_obj()) {
            // get the address of the deleter for T type
            ptr.set_deleter_ptr(&make_deleter<T>::deleter);
        }
    }

    // the given ptr can't be const
    hybrid_shared_ptr(hybrid_shared_ptr & other) : ptr_to_obj_or_root_ptr_on_stack(other.ptr_to_obj_or_root_ptr_on_stack) {
        if (other.is_obj()) { // not crosslinked
            if (is_address_in_stack(this)) {
                if (is_address_in_stack(&other)) {
                    link_to_ptr_on_stack(&other);
                    assert(get_stack_root_ptr() == other.get_stack_root_ptr());
                }
                else if (other.ptr.is_deleter_ptr()) {
                    assert(!other.ptr.is_shared_data() && !other.is_crosslinked());

                    // if we are in the stack and the source pointer is not
                    // and if this source pointer is the unique instance so far
                    // (the shared counter hasn't been created yet), we cross
                    // link each pointer so we can avoid creating the shared data
                    cross_link_to_unique_instance_not_in_stack(&other);
                    assert(get_global_root_ptr() == &other);
                }
            }

            if (ptr.is_null()) { // nothing was done so far
                if (other.ptr.is_shared_data()) {
                    // classic reference counting already in place
                    ptr.inc_ref(other.ptr);
                }
                else {
                    // create the shared data if not already done
                    hybrid_shared_ptr * root = other.get_global_root_ptr();
                    if (!root->ptr.is_shared_data()) {
                        root->create_shared_data();
                    }

                    // link to the shared data + increase the counter
                    ptr.inc_ref(root->ptr);
                }
            }
        }
        else if (other.is_crosslinked()) {
            assert(is_address_in_cpp_heap(&other));
            // we are copying from a unique ptr on heap that is already crosslinked
            auto crosslink = other.get_crosslinked_ptr();
            if (is_address_in_stack(this) && is_address_in_stack(crosslink)) {
                // the stack is supposed to grow downwards
                assert(this > crosslink);
                link_to_ptr_on_stack(crosslink);
                assert(get_stack_root_ptr() == crosslink);
            }
            else {
                // we need to create the shared data and fix the pointers
                other.ptr_to_obj_or_root_ptr_on_stack = reinterpret_cast<size_t>(crosslink->obj());
                other.create_shared_data();

                // reference the object + increase the counter
                ptr.inc_ref(other.ptr);
            }
            assert(is_obj());
        }
    }

    ~hybrid_shared_ptr() {
        if (ptr_to_obj_or_root_ptr_on_stack != 0ULL) {
            if (is_obj()) {
                if (ptr.is_linked_ptr()) {
                    assert(is_address_in_stack(this));
                    if (ptr.is_deleter_ptr()) {
                        // we are a root stack pointer linked to a heap pointer holding the deleter
                        // we need to uncross the previous link
                        undo_cross_link(obj());
                    }
                    else {
                        // we are a simple stack pointer linked to another (root) stack pointer
                        // the root smart pointer should still be valid
                        // if it's not, it has been invalidated via a reference which is something forbidden
                        assert(obj() == get_stack_root_ptr()->obj());
                    }
                }
                else {
                    if (ptr.is_deleter_ptr()) {
                        // we are the single (root) instance: delete the object
                        auto deleter = ptr.get_deleter_ptr();
                        deleter(obj());
                    }
                    else {
                        // use the shared data
                        auto data = ptr.get_shared_data();

                        --(data->ref_count);
                        if (data->ref_count == 0) {
                            data->deleter(obj());
                            delete data;
                        }
                    }
                }
            }
            else {
                assert(is_crosslinked() && ptr.is_deleter_ptr());
                assert(get_crosslinked_ptr()->get_global_root_ptr() == this);
                // we are the main heap instance being destroyed: we pass the deleter to
                // (root) stack pointer we are linked to, and clear the crosslinking
                auto crosslinked = get_crosslinked_ptr();
                assert(crosslinked->is_obj());
                crosslinked->ptr.set_deleter_ptr(ptr.get_deleter_ptr());
            }
            // clear to help catch bugs
            ptr_to_obj_or_root_ptr_on_stack = 0ULL;
            ptr.clear();
        }
        else {
            assert(ptr.is_null());
        }
    }

    T * operator->() {
        assert(ptr_to_obj_or_root_ptr_on_stack);
        if (is_obj()) {
            return obj();
        }
        assert(ptr_to_obj_or_root_ptr_on_stack & flags::IsLinkedPtr);
        return get_crosslinked_ptr()->obj();
    }

    const T * operator->() const {
        return const_cast<hybrid_shared_ptr &>(*this)::operator->();
    }

    T & operator*() {
        return *operator->();
    }

    const T & operator*() const {
        return *operator->();
    }

private:
    void create_shared_data() {
        assert(obj() && ptr.is_deleter_ptr());

        ptr.set_shared_data(new shared_data(
            ptr.get_deleter_ptr()));
    }

private:
    size_t ptr_to_obj_or_root_ptr_on_stack;
    bool is_obj() const {
        return (ptr_to_obj_or_root_ptr_on_stack != 0ULL) &&
            ((ptr_to_obj_or_root_ptr_on_stack & flags::IsLinkedPtr) == 0ULL);
    }
    T * obj() const {
        assert(is_obj());
        T * obj = reinterpret_cast<T*>(ptr_to_obj_or_root_ptr_on_stack);
        assert(is_address_in_cpp_heap(obj));
        return obj;
    }

    bool is_crosslinked() const {
        assert(!is_address_in_stack(this));
        return (ptr_to_obj_or_root_ptr_on_stack & flags::IsLinkedPtr);
    }

    // returns 
    hybrid_shared_ptr * get_crosslinked_ptr() const {
        assert(is_crosslinked());
        hybrid_shared_ptr * crosslinked_ptr = reinterpret_cast<hybrid_shared_ptr*>(
            ptr_to_obj_or_root_ptr_on_stack & (size_t(-1) ^ flags::IsLinkedPtr));
        // the pointer can be in a different stack that the current one
        assert(!is_address_in_cpp_heap(crosslinked_ptr));
        assert(crosslinked_ptr == crosslinked_ptr->get_stack_root_ptr());
        return crosslinked_ptr;
    }

    const hybrid_shared_ptr * get_stack_root_ptr() const {
        assert(!ptr.is_null() && is_address_in_stack(this));
        // caution: the stack root pointer can be linked to the unique heap pointer
        if (ptr.is_linked_ptr() && !ptr.is_deleter_ptr()) {
            return ptr.get_linked_ptr();
        }
        return this;
    }

    // if called on a unique heap instance, returns itself
    // must not be called if there are several global instances
    hybrid_shared_ptr * get_global_root_ptr() {
        assert(!ptr.is_shared_data() || ptr.get_shared_data()->ref_count == 1);

        auto root = ptr.is_null() ? this : const_cast<hybrid_shared_ptr *>(get_stack_root_ptr());
        if (root->ptr.is_linked_ptr()) {
            return root->ptr.get_linked_ptr();
        }
        assert(is_address_in_stack(root) || root == this);
        return root;
    }

    void cross_link_to_unique_instance_not_in_stack(hybrid_shared_ptr * other) {
        assert(!other->is_crosslinked());
        assert(ptr.is_null() && other->is_obj());
        assert(is_address_in_stack(this) && !is_address_in_stack(other));
        ptr.link_to_global_ptr_on_heap(other);
        // we link back this pointer to us
        other->ptr_to_obj_or_root_ptr_on_stack = reinterpret_cast<size_t>(this) | flags::IsLinkedPtr;
    }

    void undo_cross_link(T * obj) {
        assert(ptr.is_linked_ptr() && is_address_in_cpp_heap(obj));
        hybrid_shared_ptr * other = ptr.get_linked_ptr();
        assert(other->is_crosslinked());
        other->ptr_to_obj_or_root_ptr_on_stack = reinterpret_cast<size_t>(obj);
        ptr.clear();
    }

    void link_to_ptr_on_stack(const hybrid_shared_ptr * other) {
        assert(is_address_in_stack(this) && is_address_in_stack(other));
        ptr.set_linked_ptr(other);
        ptr_to_obj_or_root_ptr_on_stack = reinterpret_cast<size_t>(other->obj());
    }

    struct shared_data {
        std::atomic<unsigned int> ref_count;
        std::function<void(T*)> deleter;

        shared_data(deleter_ptr del) : ref_count(1), deleter(del) {
            total_nb_allocs += 1;
        }
    };

    enum flags : size_t {
        // combinaison of IsLinkedPtr + IsDeleterPtr means we are linked
        // to the unique instance not in stack (=> in heap)
        IsLinkedPtr = (1LL << 63),
        IsDeleterPtr = (1LL << 62),
    };

    // versatile_ptr is used to store a pointer where 2 bits are reserved
    // for storing info about the pointed object
    class versatile_ptr {
    public:
        versatile_ptr() : address(0ULL) {
            // the goal of this struct is to be of the size of a ptr
            static_assert(sizeof(void*) == 8, "only 64 bits is supported");
            static_assert(sizeof(*this) == sizeof(void*), "versatile_ptr size != pointer size!");
        }

        versatile_ptr & operator=(const versatile_ptr &) = delete;

        void inc_ref(const versatile_ptr & other) {
            assert(is_null() && !other.is_null()); // not supposed to be called is null
            set_shared_data(other.get_shared_data());
            ++get_shared_data()->ref_count;
        }

        void clear() {
            address = 0ULL;
        }

        bool is_null() const {
            return address == 0ULL;
        }

        bool is_shared_data() const {
            size_t mask = flags::IsLinkedPtr | flags::IsDeleterPtr;
            return (address != 0ULL) && ((address & mask) == 0ULL);
        }

        // links to another smart pointer. must be on the stack pointer
        bool is_linked_ptr() const {
            bool b = (address & flags::IsLinkedPtr) != 0ULL;
            assert(!b || is_address_in_stack(this));
            return b;
        }

        // if we are the main pointer that hasn't created yet the shared data
        // we are holding a pointer to the deleter to be used and eventually
        // to be stored in the shared data when created
        bool is_deleter_ptr() const {
            return (address & flags::IsDeleterPtr) != 0ULL;
        }

        shared_data * get_shared_data() const {
            assert(!is_null() && is_shared_data());
            auto data = reinterpret_cast<shared_data *>(address);
            assert(is_address_in_cpp_heap(data));
            return data;
        }

        void set_shared_data(shared_data * data) {
            assert(is_null() || is_deleter_ptr());
            address = reinterpret_cast<size_t>(data);
        }

        hybrid_shared_ptr * get_linked_ptr() const {
            assert(!is_null() && is_linked_ptr());
            return reinterpret_cast<hybrid_shared_ptr *>(address & (size_t(-1) ^ (flags::IsLinkedPtr | flags::IsDeleterPtr)));
        }

        void set_linked_ptr(const hybrid_shared_ptr * other) {
            assert(is_address_in_stack(this) && is_address_in_stack(other));
            address = (reinterpret_cast<size_t>(other->get_stack_root_ptr()) | flags::IsLinkedPtr);
        }

        void link_to_global_ptr_on_heap(const hybrid_shared_ptr * other) {
            assert(is_null() && is_address_in_cpp_heap(other));
            // linked to a pointer holding the deleter
            address = (reinterpret_cast<size_t>(other) | flags::IsLinkedPtr | flags::IsDeleterPtr);
        }

        deleter_ptr get_deleter_ptr() const {
            assert(!is_null() && is_deleter_ptr());
            return reinterpret_cast<deleter_ptr>(address & (size_t(-1LL) ^ flags::IsDeleterPtr));
        }

        void set_deleter_ptr(deleter_ptr del)  {
            // if we are not null, the deleter is supposed to be set by the non stack pointer we are linked to
            assert(is_null() || !is_address_in_stack(get_linked_ptr()));
            address = (reinterpret_cast<size_t>(del) | flags::IsDeleterPtr);
            assert(get_deleter_ptr() == del);
        }

    private:
        size_t address;
    } ptr;
};

#endif
