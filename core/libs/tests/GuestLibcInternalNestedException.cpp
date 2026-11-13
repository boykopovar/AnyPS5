#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

struct NestedExceptionLayout {
    const void* vtable;
    GuestExceptionPointer ptr;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal nested exception: %s\n", message);
    std::abort();
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    _ZNSt16nested_exceptionD2Ev_nid_postfix(nullptr);
    _ZNSt16nested_exceptionD1Ev_nid_postfix(nullptr);
    _ZNSt16nested_exceptionD0Ev_nid_postfix(nullptr);

    NestedExceptionLayout baseObj{nullptr, GuestExceptionPointer(nullptr)};
    _ZNSt16nested_exceptionD2Ev_nid_postfix(&baseObj);
    Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&baseObj.ptr) == nullptr, "base destructor leaves pointer null");

    NestedExceptionLayout completeObj{nullptr, GuestExceptionPointer(nullptr)};
    _ZNSt16nested_exceptionD1Ev_nid_postfix(&completeObj);
    Require(_ZNKSt15__exception_ptr13exception_ptr6_M_getEv_nid_postfix(&completeObj.ptr) == nullptr, "complete destructor leaves pointer null");

    void* memory = _Znwm_nid_postfix(sizeof(NestedExceptionLayout));
    Require(memory != nullptr, "allocation should succeed");
    std::memset(memory, 0, sizeof(NestedExceptionLayout));
    _ZNSt16nested_exceptionD0Ev_nid_postfix(memory);

    return 0;
}
