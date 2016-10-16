#include "../is_address_in_stack.h"

#include <string>
#include <sstream>
#include <stack>
#include <memory>
#include <iostream>

class A {
public:
    A(const char * functionName) {
        if (is_address_in_stack(this)) {
            previous_instance = last_instance;
            last_instance = this;
            attached_function_name = functionName;
        }
    }

    std::string getCallingFlow() {
        std::ostringstream oss;

        if (attached_function_name) {
            std::stack<const char *> callers;

            callers.push(attached_function_name);
            for (auto it = previous_instance; it != nullptr; it = it->previous_instance) {
                callers.push(it->attached_function_name);
            }

            std::string indent; // manage indentation
            while (!callers.empty()) {
                oss << indent << callers.top() << "()\n";
                indent += " ";
                callers.pop();
            }
        }
        return oss.str();
    }

    ~A() {
        if (is_address_in_stack(this)) {
            last_instance = previous_instance; // can be null
        }
    }
private:
    const char * attached_function_name = nullptr;
    A * previous_instance = nullptr;
    static THREAD_LOCAL A * last_instance;
};

A * A::last_instance = nullptr;

std::string f(int n) {
    A a(__FUNCTION__);
    std::unique_ptr<A> pA;
    if (n % 3 == 0) {
        pA.reset(new A("_not_in_the_stack_"));
    }
    if (n > 0) {
        return f(n - 1);
    }
    else {
        return a.getCallingFlow();
    }
}

int main() {
    A a(__FUNCTION__);
    std::cout << f(10);
}