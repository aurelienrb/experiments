#include "poly_list.h"

#include <iostream>
#include <string>
#include <algorithm>
using namespace std;

class A {
public:
    virtual std::string id() = 0;

protected:
    virtual ~A() = default;
};

class B : public A {
public:
    static int nb_instances;

    B(const B & other) {
        m_name = other.m_name;
        //std::cout << "copy      B:" << m_name << "    @" << std::hex << (long)this << "\n";
        nb_instances += 1;
    }

    B(const std::string & name) : m_name(name) {
        //std::cout << "construct B:" << m_name << "    @" << std::hex << (long)this << "\n";
        nb_instances += 1;
    }

    ~B() {
        //std::cout << "destroy   B:" << m_name << "    @" << std::hex << (long)this << "\n";
        nb_instances -= 1;
    }

    virtual std::string id() override {
        return "B:" + m_name;
    }

private:
    std::string m_name;
};

int B::nb_instances = 0;

// C is much bigger than B
class C : public A {
    virtual std::string id() override {
        return "C";
    }
    std::string data[10];
};

// D can throw in ctor / copy ctor
class D : public A {
public:
    static bool fail_in_copy;
    D(bool fail) {
        if (fail) {
            throw std::exception("failure");
        }
    }
    D(const D & other) {
        if (fail_in_copy) {
            throw std::exception("failure");
        }
        *this = other;
    }
    virtual std::string id() override {
        return "D";
    }
};

bool D::fail_in_copy = false;

void test_empty() {
    poly_list<A> list;
    assert(list.empty());
    assert(list.begin() == list.end());
}

void test_emplace_back() {
    B::nb_instances = 0;
    {
        poly_list<A> list;
        list.emplace_back<B>("b1");
        assert(B::nb_instances == 1);
        assert(!list.empty());
        assert(list.begin() != list.end());
        assert(list.begin()->id() == "B:b1");
    }
    assert(B::nb_instances == 0);
}

void test_clear() {
    B::nb_instances = 0;
    {
        // clear an empty list
        poly_list<A> list;
        list.clear();
    }
    {
        // clear a non empty list
        poly_list<A> list;
        list.emplace_back<B>("b1");
        list.clear();
        assert(B::nb_instances == 0);
        assert(list.empty());
        assert(list.begin() == list.end());
    }
    {
        B::nb_instances = 0;
        // insert, clear, insert again
        poly_list<A> list;
        list.emplace_back<B>("b1");
        list.clear();
        list.emplace_back<B>("b2");
        assert(B::nb_instances == 1);
        assert(!list.empty());
        assert(list.begin() != list.end());
        assert(list.begin()->id() == "B:b2");
    }
    assert(B::nb_instances == 0);
}

void test_grow_from_empty() {
    poly_list<A> list;
    list.emplace_back<B>("b1");
    list.clear();
    list.emplace_back<C>();
}

void test_grow() {
    const int nb_b_to_create = 100;
    B::nb_instances = 0;
    {
        poly_list<A> list;
        for (int i = 0; i < nb_b_to_create; ++i) {
            list.emplace_back<B>(std::to_string(i));
        }
        assert(B::nb_instances == nb_b_to_create);
    }
    assert(B::nb_instances == 0);
}

void test_heterogeneous_grow() {
    poly_list<A> list;
    for (int i = 0; i < 100; ++i) {
        if (i % 2) {
            list.emplace_back<B>(std::to_string(i));
        }
        else {
            list.emplace_back<C>();
        }
    }
}

void test_ctor_exception() {
    B::nb_instances = 0;
    D::fail_in_copy = false;

    poly_list<A> list;
    try {
        list.emplace_back<D>(true);
    }
    catch (...) {
    }
    assert(list.empty());

    try {
        list.emplace_back<B>("1");
        list.emplace_back<D>(true);
    }
    catch (...) {
    }
    assert(!list.empty());
    assert(B::nb_instances == 1);

    // exception in copy ctor
    int expected_nb_b = B::nb_instances;
    try {
        D::fail_in_copy = false;
        list.emplace_back<D>(false);

        for (int i = 0; i < 100; ++i) {
            list.emplace_back<B>(std::to_string(i));
            expected_nb_b += 1;
            if (i == 30) {
                D::fail_in_copy = true;
            }
        }

        assert(false); // should have thrown
    }
    catch (...) {
    }
    assert(B::nb_instances == expected_nb_b);
    D::fail_in_copy = false;
}

void test_iterator() {
    B::nb_instances = 0;
    D::fail_in_copy = false;

    poly_list<A> list;

    const int nbEach = 20;
    for (int i = 0; i < nbEach; ++i) {
        list.emplace_back<B>(std::to_string(i));
        list.emplace_back<C>();
        list.emplace_back<D>(false);
    }
    assert(B::nb_instances == nbEach);

    int nbB = 0;
    int nbC = 0;
    int nbD = 0;

    // C++98 style - pre-increment, operator*
    for (poly_list<A>::iterator it = list.begin(), end = list.end(); it != end; ++it) {
        switch ((*it).id()[0]) {
        case 'B':
            nbB += 1;
            break;
        case 'C':
            nbC += 1;
            break;
        case 'D':
            nbD += 1;
            break;
        }
    }
    assert(nbB == nbEach);
    assert(nbC == nbEach);
    assert(nbD == nbEach);

    nbB = nbC = nbD = 0; // reset

    // C++98 style - post-increment, operator->
    for (poly_list<A>::iterator it = list.begin(), end = list.end(); it != end; it++) {
        switch (it->id()[0]) {
        case 'B':
            nbB += 1;
            break;
        case 'C':
            nbC += 1;
            break;
        case 'D':
            nbD += 1;
            break;
        }
    }
    assert(nbB == nbEach);
    assert(nbC == nbEach);
    assert(nbD == nbEach);
}

int main() {
    test_empty();
    test_emplace_back();
    test_clear();
    test_grow();
    test_grow_from_empty();
    test_heterogeneous_grow();
    test_ctor_exception();
    test_iterator();
}
