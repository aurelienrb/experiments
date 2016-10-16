#ifndef IS_ADDRESS_IN_STACK_H
#define IS_ADDRESS_IN_STACK_H

// VC++ doesn't support yet thread_local from C++11 so we define the macro THREAD_LOCAL
// and not thread_local because:
// "error C1189: #error :  The C++ Standard Library forbids macroizing keywords."
#define THREAD_LOCAL __declspec(thread)

bool is_address_in_stack(const void * address);

void unit_test__is_address_in_stack();

#endif
