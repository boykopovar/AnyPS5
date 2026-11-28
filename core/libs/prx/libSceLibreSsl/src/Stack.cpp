#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cerrno>
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

template <typename T>
T CopyGuestInPreservingErrno(const T* source, const char* function) {
    T destination{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return destination;
}

void StoreCountPreservingErrno(LibreSslStack* stack, int count, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(&stack->num, &count, sizeof(count));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

char** Elements(const LibreSslStack& stack, const char* function) {
    if (!stack.data) throw std::invalid_argument(std::string(function) + ": null element array");
    return stack.data;
}

}

extern "C" {
int APS5_VABI sk_num_nid_postfix(const LibreSslStack* stack) {
    if (!stack) return -1;
    return CopyGuestInPreservingErrno(stack, __func__).num;
}

void* APS5_VABI sk_value_nid_postfix(const LibreSslStack* stack, int index) {
    if (!stack || index < 0) return nullptr;
    const LibreSslStack copy = CopyGuestInPreservingErrno(stack, __func__);
    if (index >= copy.num) return nullptr;
    return CopyGuestInPreservingErrno(Elements(copy, __func__) + index, __func__);
}

void* APS5_VABI sk_pop_nid_postfix(LibreSslStack* stack) {
    if (!stack) return nullptr;
    const LibreSslStack copy = CopyGuestInPreservingErrno(stack, __func__);
    if (copy.num <= 0) return nullptr;
    char* const element = CopyGuestInPreservingErrno(Elements(copy, __func__) + copy.num - 1, __func__);
    StoreCountPreservingErrno(stack, copy.num - 1, __func__);
    return element;
}

void APS5_VABI sk_pop_free_nid_postfix(LibreSslStack* stack, StackElementFree release) {
    if (!stack) return;
    const LibreSslStack copy = CopyGuestInPreservingErrno(stack, __func__);
    if (copy.num > 0) {
        char** const elements = Elements(copy, __func__);
        for (int index = 0; index < CopyGuestInPreservingErrno(stack, __func__).num; ++index) {
            char* const element = CopyGuestInPreservingErrno(elements + index, __func__);
            if (!element) continue;
            if (!release) throw std::invalid_argument(std::string(__func__) + ": null free function");
            release(element);
        }
    }
    free_nid_postfix(CopyGuestInPreservingErrno(stack, __func__).data);
    free_nid_postfix(stack);
}

}
