#include "is_address_in_stack.h"

#include <cassert>
#include <future>

#include <windows.h>

bool is_address_in_stack(const void * address) {
    THREAD_LOCAL static ULONG_PTR stack_low_limit;
    THREAD_LOCAL static ULONG_PTR stack_high_limit;

    if (stack_low_limit == 0) {
        // Windows 8 required (_WIN32_WINNT >= 0x0602)
        ::GetCurrentThreadStackLimits(&stack_low_limit, &stack_high_limit);
        assert(stack_low_limit > 0);
        assert(stack_high_limit > stack_low_limit);
    }

    ULONG_PTR addr = reinterpret_cast<ULONG_PTR>(address);
    return (addr >= stack_low_limit) && (addr <= stack_high_limit);
}


static void do_test() {
    static int staticInt = 0;
    int stackInt = 0;
    int * heapInt = new int(0);

    assert(is_address_in_stack(&stackInt));
    assert(!is_address_in_stack(&staticInt));
    assert(!is_address_in_stack(heapInt));

    delete heapInt;
}

void unit_test__is_address_in_stack() {
    do_test();
    std::async(std::launch::async, &do_test).get();
}
