#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {

unsigned releases = 0;
void* lastReleased = nullptr;
std::array<void*, 4> released{};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal exception destructors: %s\n", message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) {
    if (releases < released.size()) released[releases] = pointer;
    ++releases;
    lastReleased = pointer;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

using Destructor = void (APS5_VABI*)(void*);

void CheckMessageDestructor(Destructor destructor, const char* text) {
    LibcException::ExceptionObject object{};
    _ZNSt13runtime_errorC1EPKc_nid_postfix(&object, text);
    Require(object.message != nullptr && std::strcmp(object.message, text) == 0, "runtime_error keeps its message");
    destructor(&object);
    Require(object.message == nullptr, "the base-object destructor releases the message");
    destructor(&object);
    Require(object.message == nullptr, "a second destruction leaves the message released");
}

void CheckFutureError() {
    LibcException::FutureErrorObject inline_{};
    inline_.message.capacity = 15;
    inline_.message.size = 3;
    std::memcpy(inline_.message.buffer, "abc", 4);
    releases = 0;
    sceLibcInternalFutureErrorBaseDestructor_nid_no_patch(&inline_);
    Require(inline_.message.reserved == 0 && inline_.message.size == 0 && inline_.message.capacity == 15, "an inline message is reset");
    Require(inline_.message.buffer[0] == 0 && releases == 0, "an inline message is not released");

    LibcException::FutureErrorObject heap{};
    heap.message.capacity = 24;
    heap.message.size = 24;
    heap.message.pointer = static_cast<char*>(_Znwm_nid_postfix(25));
    std::memcpy(heap.message.pointer, "future already retrieved", 25);
    char* storage = heap.message.pointer;
    releases = 0;
    sceLibcInternalFutureErrorBaseDestructor_nid_no_patch(&heap);
    Require(releases == 1 && lastReleased == storage, "a heap message is released");
    Require(heap.message.size == 0 && heap.message.capacity == 15 && heap.message.buffer[0] == 0, "a heap message is reset to the inline buffer");

    LibcException::FutureErrorObject expected{};
    expected.message.capacity = 15;
    LibcException::FutureErrorObject actual = expected;
    _ZNSt12future_errorD1Ev_nid_postfix(&expected);
    sceLibcInternalFutureErrorBaseDestructor_nid_no_patch(&actual);
    Require(std::memcmp(&expected, &actual, sizeof(expected)) == 0, "D2 leaves the object as D1 does");
}

void CheckMessageDeletingDestructor(Destructor destructor, const char* text) {
    auto* object = static_cast<LibcException::ExceptionObject*>(_Znwm_nid_postfix(sizeof(LibcException::ExceptionObject)));
    _ZNSt13runtime_errorC1EPKc_nid_postfix(object, text);
    releases = 0;
    destructor(object);
    Require(releases == 1 && released[0] == object, "the deleting destructor frees the object through operator delete");
}

void CheckFutureErrorDeletingDestructor() {
    auto* object = new (_Znwm_nid_postfix(sizeof(LibcException::FutureErrorObject))) LibcException::FutureErrorObject{};
    object->message.capacity = 24;
    object->message.size = 24;
    object->message.pointer = static_cast<char*>(_Znwm_nid_postfix(25));
    std::memcpy(object->message.pointer, "future already retrieved", 25);
    char* storage = object->message.pointer;
    releases = 0;
    sceLibcInternalFutureErrorDeletingDestructor_nid_no_patch(object);
    Require(releases == 2 && released[0] == storage && released[1] == object, "the message and then the object are freed");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckMessageDestructor(&sceLibcInternalOverflowErrorBaseDestructor_nid_no_patch, "value too large");
    CheckMessageDestructor(&sceLibcInternalUnderflowErrorBaseDestructor_nid_no_patch, "value too small");
    CheckFutureError();
    CheckMessageDeletingDestructor(&sceLibcInternalOverflowErrorDeletingDestructor_nid_no_patch, "value too large");
    CheckMessageDeletingDestructor(&sceLibcInternalUnderflowErrorDeletingDestructor_nid_no_patch, "value too small");
    CheckFutureErrorDeletingDestructor();

    std::puts("libc internal exception destructor checks passed");
    return 0;
}
