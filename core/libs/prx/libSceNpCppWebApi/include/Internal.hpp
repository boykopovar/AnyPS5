#pragma once

#include <cstddef>

extern "C" {

// Test-only access to the LibContext arena. Guest code never sees these symbols.
void* NpCppWebApiContextAllocate(void* libContext, std::size_t size);
void NpCppWebApiContextDeallocate(void* libContext, void* object);
void* NpCppWebApiContextMspace(void* libContext);

}
