#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL stack check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static std::vector<void*> freed;

static void* APS5_VABI Allocate(std::size_t size) {
    return std::malloc(size);
}

static void APS5_VABI Free(void* pointer) {
    freed.push_back(pointer);
    std::free(pointer);
}

static void* APS5_VABI Calloc(std::size_t, std::size_t) { throw std::runtime_error("unexpected calloc"); }
static void* APS5_VABI Reallocate(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
static void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
static void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
static int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

static void InitializeHeap() {
    void* const api[10] {reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc), reinterpret_cast<void*>(Reallocate),
        reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}

static void TestNum() {
    char first = 'a';
    Require(sk_num_nid_postfix(nullptr) == -1);
    LibreSslStack stack {3, nullptr, 0, 0, nullptr};
    char* elements[3] {&first, nullptr, &first};
    stack.data = elements;
    Require(sk_num_nid_postfix(&stack) == 3);
    stack.num = 0;
    Require(sk_num_nid_postfix(&stack) == 0);
    stack.num = -2;
    Require(sk_num_nid_postfix(&stack) == -2);
    LibreSslStack detached {2, nullptr, 0, 0, nullptr};
    Require(sk_num_nid_postfix(&detached) == 2);
}

static void TestValue() {
    char first = 'a';
    char second = 'b';
    char third = 'c';
    Require(sk_value_nid_postfix(nullptr, 0) == nullptr);
    char* elements[4] {&first, nullptr, &third, &second};
    LibreSslStack stack {3, elements, 0, 4, nullptr};
    Require(sk_value_nid_postfix(&stack, 0) == &first);
    Require(sk_value_nid_postfix(&stack, 1) == nullptr);
    Require(sk_value_nid_postfix(&stack, 2) == &third);
    Require(sk_value_nid_postfix(&stack, 3) == nullptr);
    Require(sk_value_nid_postfix(&stack, -1) == nullptr);
    stack.num = -2;
    Require(sk_value_nid_postfix(&stack, 0) == nullptr);
    LibreSslStack detached {2, nullptr, 0, 0, nullptr};
    Require(sk_value_nid_postfix(&detached, 2) == nullptr);
    RequireThrows([&] { sk_value_nid_postfix(&detached, 1); });
}

static void TestPop() {
    char first = 'a';
    char third = 'c';
    Require(sk_pop_nid_postfix(nullptr) == nullptr);
    char* elements[8] {&first, nullptr, &third};
    LibreSslStack stack {3, elements, 1, 8, nullptr};
    Require(sk_pop_nid_postfix(&stack) == &third);
    Require(sk_num_nid_postfix(&stack) == 2 && stack.numAlloc == 8 && stack.data == elements && elements[2] == &third && stack.sorted == 1);
    Require(sk_value_nid_postfix(&stack, 2) == nullptr);
    Require(sk_pop_nid_postfix(&stack) == nullptr && sk_num_nid_postfix(&stack) == 1);
    Require(sk_pop_nid_postfix(&stack) == &first && sk_num_nid_postfix(&stack) == 0 && stack.sorted == 1);
    Require(sk_pop_nid_postfix(&stack) == nullptr && sk_num_nid_postfix(&stack) == 0);
    stack.num = -2;
    Require(sk_pop_nid_postfix(&stack) == nullptr && sk_num_nid_postfix(&stack) == -2);
    LibreSslStack detached {2, nullptr, 0, 0, nullptr};
    RequireThrows([&] { sk_pop_nid_postfix(&detached); });
    Require(detached.num == 2);
}

static std::vector<void*> released;

static void APS5_VABI Release(void* element) {
    released.push_back(element);
}

static LibreSslStack* NewStack(std::vector<char*> elements, int capacity) {
    auto* stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
    Require(stack != nullptr);
    std::memset(stack, 0, sizeof(LibreSslStack));
    stack->data = static_cast<char**>(malloc_nid_postfix(sizeof(char*) * static_cast<std::size_t>(capacity > 0 ? capacity : 1)));
    Require(stack->data != nullptr);
    for (int index = 0; index < capacity; ++index) stack->data[index] = nullptr;
    for (std::size_t index = 0; index < elements.size(); ++index) stack->data[index] = elements[index];
    stack->num = static_cast<int>(elements.size());
    stack->numAlloc = capacity;
    return stack;
}

static void TestPopFree() {
    char first = 'a';
    char second = 'b';
    char third = 'c';
    sk_pop_free_nid_postfix(nullptr, Release);
    sk_pop_free_nid_postfix(nullptr, nullptr);
    Require(released.empty());

    LibreSslStack* stack = NewStack({&first, nullptr, &second, &third}, 4);
    char** const data = stack->data;
    freed.clear();
    sk_pop_free_nid_postfix(stack, Release);
    Require(released == std::vector<void*>({&first, &second, &third}));
    Require(freed == std::vector<void*>({data, stack}));
    released.clear();

    stack = NewStack({nullptr, nullptr}, 2);
    sk_pop_free_nid_postfix(stack, nullptr);
    stack = NewStack({}, 0);
    sk_pop_free_nid_postfix(stack, nullptr);
    stack = NewStack({&first}, 1);
    stack->num = -2;
    sk_pop_free_nid_postfix(stack, nullptr);
    Require(released.empty());

    stack = NewStack({&first, &second}, 2);
    RequireThrows([&] { sk_pop_free_nid_postfix(stack, nullptr); });
    stack->data[0] = nullptr;
    stack->data[1] = nullptr;
    sk_pop_free_nid_postfix(stack, nullptr);
}

template<typename TCall> static void RejectsPreservingErrno(TCall call, std::source_location location = std::source_location::current()) {
    errno = 71;
    bool rejected = false;
    try { call(); } catch (const std::runtime_error&) { rejected = errno == 71; }
    Require(rejected, location);
}

static void TestGuestPointers() {
    auto* unmapped = reinterpret_cast<LibreSslStack*>(std::uintptr_t {0x10});
    RejectsPreservingErrno([&] { sk_num_nid_postfix(unmapped); });
    RejectsPreservingErrno([&] { sk_value_nid_postfix(unmapped, 0); });
    Require(sk_value_nid_postfix(unmapped, -1) == nullptr);
    RejectsPreservingErrno([&] { sk_pop_nid_postfix(unmapped); });
    RejectsPreservingErrno([&] { sk_pop_free_nid_postfix(unmapped, Release); });

    char first = 'a';
    LibreSslStack detached {2, reinterpret_cast<char**>(std::uintptr_t {0x10}), 0, 2, nullptr};
    RejectsPreservingErrno([&] { sk_value_nid_postfix(&detached, 1); });
    Require(sk_value_nid_postfix(&detached, 2) == nullptr);
    RejectsPreservingErrno([&] { sk_pop_nid_postfix(&detached); });
    Require(detached.num == 2);
    freed.clear();
    RejectsPreservingErrno([&] { sk_pop_free_nid_postfix(&detached, Release); });
    Require(released.empty() && freed.empty());

#ifdef _WIN32
    SYSTEM_INFO info {};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(page != nullptr);
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* page = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    char* elements[2] {&first, nullptr};
    auto* readOnly = static_cast<LibreSslStack*>(page);
    *readOnly = LibreSslStack {2, elements, 0, 2, nullptr};
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, pageSize, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, pageSize, PROT_READ) == 0);
#endif
    Require(sk_num_nid_postfix(readOnly) == 2);
    Require(sk_value_nid_postfix(readOnly, 0) == &first);
    RejectsPreservingErrno([&] { sk_pop_nid_postfix(readOnly); });
    Require(readOnly->num == 2);
#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, pageSize);
#endif
}

int main() {
    InitializeHeap();
    TestNum();
    TestValue();
    TestPop();
    TestPopFree();
    TestGuestPointers();
    std::puts("libSceLibreSsl stack tests passed");
}
