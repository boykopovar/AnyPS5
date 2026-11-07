#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

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

void* APS5_VABI sk_pop_nid_postfix(LibreSslStack* stack) {
    if (!stack || stack->num <= 0) return nullptr;
    char* const element = Elements(stack, __func__)[stack->num - 1];
    --stack->num;
    return element;
}

void APS5_VABI sk_pop_free_nid_postfix(LibreSslStack* stack, StackElementFree release) {
    if (!stack) return;
    if (stack->num > 0) {
        char** const elements = Elements(stack, __func__);
        for (int index = 0; index < stack->num; ++index) {
            if (!elements[index]) continue;
            if (!release) throw std::invalid_argument(std::string(__func__) + ": null free function");
            release(elements[index]);
        }
    }
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

}
