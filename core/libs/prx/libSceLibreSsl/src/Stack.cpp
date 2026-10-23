#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

using StackCompare = int (APS5_VABI*)(const void*, const void*);

struct LibreSslStack {
    int num;
    char** data;
    int sorted;
    int numAlloc;
    StackCompare comp;
};

static_assert(sizeof(LibreSslStack) == 32);
static_assert(offsetof(LibreSslStack, num) == 0);
static_assert(offsetof(LibreSslStack, data) == 8);
static_assert(offsetof(LibreSslStack, sorted) == 16);
static_assert(offsetof(LibreSslStack, numAlloc) == 20);
static_assert(offsetof(LibreSslStack, comp) == 24);

namespace {

char** Elements(const LibreSslStack* stack, const char* function) {
    if (!stack->data) throw std::invalid_argument(std::string(function) + ": null element array");
    return stack->data;
}

}

extern "C" {

int APS5_VABI sk_num_nid_postfix(const LibreSslStack* stack) {
    if (!stack) return -1;
    return stack->num;
}

void* APS5_VABI sk_value_nid_postfix(const LibreSslStack* stack, int index) {
    if (!stack || index < 0 || index >= stack->num) return nullptr;
    return Elements(stack, __func__)[index];
}

}
