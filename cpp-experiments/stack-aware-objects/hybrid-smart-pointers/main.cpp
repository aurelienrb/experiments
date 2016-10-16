#include "hybrid_smart_pointer.h"

#include <cassert>
#include <iostream>

unsigned int total_nb_allocs = 0;

struct A {
    static int nb_instances;

    A() {
        std::cout << "A()\n";
        ++nb_instances;
    }
    ~A() {
        std::cout << "~A()\n";
        --nb_instances;
    }

    void hello() const {
        std::cout << "hello!\n";
    }
};
int A::nb_instances = 0;

void f(hybrid_shared_ptr<A> p) {
    (*p).hello();
}

struct B {
    B() : pA(new A) {
    }

    B(hybrid_shared_ptr<A> & a) : pA(a) {
    }

    hybrid_shared_ptr<A> pA;
};

int main() {
    {
        hybrid_shared_ptr<A> n1(new A);
        {
            hybrid_shared_ptr<A> n2 = n1;
            f(n2);
        }
        n1->hello();
        f(n1);
        B b(n1);

        delete new B;
    }
    assert(A::nb_instances == 0);
    assert(total_nb_allocs == 0);
    {
        B * b1;
        B * b2;
        {
            hybrid_shared_ptr<A> n1(new A);
            hybrid_shared_ptr<A> n2 = n1;
            b1 = new B(n1);
            b2 = new B(n2);
        }
        assert(total_nb_allocs == 1);

        hybrid_shared_ptr<A> n3 = b2->pA; // won't link on the stack
        delete b1;
        delete b2;
        n3->hello();
    }
    assert(A::nb_instances == 0);
    assert(total_nb_allocs == 1);

    // test we can have a unique shared pointer in heap and several ones in stack
    // without having to alloc a shared counter
    total_nb_allocs = 0;
    {
        B * b = new B;
        f(b->pA);

        {
            // destroy before the delete of the global instance
            auto n1 = b->pA;
            f(n1);
        }

        // destroy after the delete of the global instance
        auto n2 = b->pA;
        f(n2);
        b->pA->hello();
        delete b;
    }
    assert(total_nb_allocs == 0);
    assert(A::nb_instances == 0);

    // creation of several pointers on the same stack, but not from the exisiting stack root
    {
        B * b = new B;
        auto n1 = b->pA;
        auto n2 = b->pA;
        delete b;
    }
    assert(total_nb_allocs == 0);
    assert(A::nb_instances == 0);

#if 0
    // root stack instance switching test
    {
        hybrid_shared_ptr<A> n1;
        B * b = new B;
        {
            hybrid_shared_ptr<A> n2;
            {
                // create temporary root instance on the stack
                auto n3 = b->pA;
                n2 = n3;
            }
            // n2 becomes the new root instance on the stack
            n1 = n2;
            delete b;
        }
        // n1 becomes the new root instance on the stack
        assert(A::nb_instances == 1);
    }
    assert(total_nb_allocs == 0);
    assert(A::nb_instances == 0);
#endif
}
