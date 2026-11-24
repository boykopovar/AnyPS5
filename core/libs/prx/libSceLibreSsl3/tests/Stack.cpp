#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 stack check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;
std::size_t freeCalls = 0;
std::size_t reallocations = 0;
bool failAllocate = false;
bool failReallocate = false;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failAllocate) return nullptr;
    void* result = std::malloc(size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    ++freeCalls;
    std::free(pointer);
}

void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (failReallocate) return nullptr;
    if (pointer) Require(allocations.erase(pointer) == 1);
    void* result = std::realloc(pointer, size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    ++reallocations;
    return result;
}

void* APS5_VABI UnexpectedCalloc_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected calloc"); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

int APS5_VABI Compare_nid_no_patch(const void*, const void*) { return 0; }

std::size_t released = 0;
void APS5_VABI Release_nid_no_patch(void*) { ++released; }

std::array<int, 10> items{};

void* Item(int index) { return &items[index]; }

void RoundTrip() {
    auto* stack = sk_new_nid_postfix(Compare_nid_no_patch);
    Require(stack != nullptr);
    Require(allocations.size() == 2 && allocations.at(stack) == sizeof(LibreSslStack) && allocations.at(stack->data) == 4 * sizeof(char*));
    Require(stack->num == 0 && stack->sorted == 0 && stack->numAlloc == 4 && stack->comp == Compare_nid_no_patch);
    for (int index = 0; index < 4; ++index) Require(stack->data[index] == nullptr);
    Require(sk_push_nid_postfix(stack, Item(0)) == 1);
    stack->sorted = 1;
    Require(sk_push_nid_postfix(stack, Item(1)) == 2 && stack->sorted == 0);
    Require(sk_push_nid_postfix(stack, nullptr) == 3);
    Require(sk_num_nid_postfix(stack) == 3);
    Require(sk_value_nid_postfix(stack, 0) == Item(0) && sk_value_nid_postfix(stack, 1) == Item(1) && sk_value_nid_postfix(stack, 2) == nullptr);
    Require(sk_pop_nid_postfix(stack) == nullptr && sk_pop_nid_postfix(stack) == Item(1) && sk_num_nid_postfix(stack) == 1);
    Require(sk_push_nid_postfix(stack, Item(2)) == 2 && sk_value_nid_postfix(stack, 1) == Item(2));
    const std::size_t freesBefore = freeCalls;
    sk_free_nid_postfix(stack);
    Require(freeCalls == freesBefore + 2 && allocations.empty());
}

void Growth() {
    auto* stack = sk_new_nid_postfix(nullptr);
    Require(stack != nullptr && stack->comp == nullptr);
    reallocations = 0;
    for (int index = 0; index < 3; ++index) Require(sk_push_nid_postfix(stack, Item(index)) == index + 1);
    Require(stack->numAlloc == 4 && reallocations == 0);
    Require(sk_push_nid_postfix(stack, Item(3)) == 4);
    Require(stack->numAlloc == 8 && reallocations == 1 && allocations.at(stack->data) == 8 * sizeof(char*));
    for (int index = 4; index < 7; ++index) Require(sk_push_nid_postfix(stack, Item(index)) == index + 1);
    Require(stack->numAlloc == 8 && reallocations == 1);
    for (int index = 7; index < 10; ++index) Require(sk_push_nid_postfix(stack, Item(index)) == index + 1);
    Require(stack->numAlloc == 16 && reallocations == 2 && allocations.at(stack->data) == 16 * sizeof(char*));
    for (int index = 0; index < 10; ++index) Require(sk_value_nid_postfix(stack, index) == Item(index));
    released = 0;
    sk_pop_free_nid_postfix(stack, Release_nid_no_patch);
    Require(released == 10 && allocations.empty());
}

void FreeNull() {
    const std::size_t freesBefore = freeCalls;
    sk_free_nid_postfix(nullptr);
    Require(freeCalls == freesBefore);
}

void AllocationFailures() {
    failAllocate = true;
    Require(sk_new_nid_postfix(nullptr) == nullptr && allocations.empty());
    failAllocate = false;
    failReallocate = true;
    Require(sk_new_nid_postfix(nullptr) == nullptr && allocations.empty());
    failReallocate = false;

    auto* stack = sk_new_nid_postfix(nullptr);
    Require(stack != nullptr);
    for (int index = 0; index < 3; ++index) Require(sk_push_nid_postfix(stack, Item(index)) == index + 1);
    char** const before = stack->data;
    failReallocate = true;
    Require(sk_push_nid_postfix(stack, Item(3)) == 0);
    failReallocate = false;
    Require(stack->num == 3 && stack->numAlloc == 4 && stack->data == before && sk_value_nid_postfix(stack, 2) == Item(2));
    Require(sk_push_nid_postfix(stack, Item(3)) == 4 && stack->numAlloc == 8);
    sk_free_nid_postfix(stack);
    Require(allocations.empty());
}

void GuestPointerValidation() {
    struct ProtectedPages {
        std::size_t pageSize = 0;
        std::uint8_t* base = nullptr;
        ProtectedPages() {
#ifdef _WIN32
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            pageSize = info.dwPageSize;
            base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Require(base != nullptr);
#else
            const long size = sysconf(_SC_PAGESIZE);
            pageSize = size > 0 ? static_cast<std::size_t>(size) : 4096u;
            void* mapped = mmap(nullptr, 2 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(mapped != MAP_FAILED);
            base = static_cast<std::uint8_t*>(mapped);
#endif
        }
        ~ProtectedPages() {
            if (!base) return;
#ifdef _WIN32
            VirtualFree(base, 0, MEM_RELEASE);
#else
            munmap(base, 2 * pageSize);
#endif
        }
        void Protect() {
#ifdef _WIN32
            DWORD previous = 0;
            Require(VirtualProtect(base, pageSize, PAGE_READONLY, &previous) != 0);
            Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
            Require(mprotect(base, pageSize, PROT_READ) == 0);
            Require(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
#endif
        }
    } pages;

    auto* const validStack = sk_new_nid_postfix(nullptr);
    Require(validStack != nullptr);
    const std::size_t allocsBefore = allocations.size();

    LibreSslStack roTemplate = *validStack;
    std::memcpy(pages.base, &roTemplate, sizeof(roTemplate));
    LibreSslStack roGrowTemplate = *validStack;
    roGrowTemplate.num = roGrowTemplate.numAlloc - 1;
    std::memcpy(pages.base + 256, &roGrowTemplate, sizeof(roGrowTemplate));
    std::memset(pages.base + 512, 0xa5, 64);
    pages.Protect();

    auto* const roStack = reinterpret_cast<LibreSslStack*>(pages.base);
    auto* const roGrowStack = reinterpret_cast<LibreSslStack*>(pages.base + 256);
    auto** const roData = reinterpret_cast<char**>(pages.base + 512);
    auto* const noAccess = pages.base + pages.pageSize;
    auto* const unmapped = reinterpret_cast<void*>(std::uintptr_t{1});

    auto expectInvalid = [&](LibreSslStack* target, void* element) {
        errno = E2BIG;
        bool threw = false;
        try {
            sk_push_nid_postfix(target, element);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        Require(errno == E2BIG);
        Require(allocations.size() == allocsBefore);
    };

    expectInvalid(nullptr, Item(0));
    expectInvalid(reinterpret_cast<LibreSslStack*>(unmapped), Item(0));
    expectInvalid(reinterpret_cast<LibreSslStack*>(noAccess), Item(0));
    expectInvalid(roStack, Item(0));
    expectInvalid(roGrowStack, Item(0));
    Require(roStack->num == 0 && roStack->numAlloc == 4 && roStack->sorted == 0);
    Require(roGrowStack->num == 3 && roGrowStack->numAlloc == 4 && roGrowStack->sorted == 0);

    LibreSslStack badDataStack{};
    badDataStack.num = 0;
    badDataStack.numAlloc = 4;
    badDataStack.sorted = 1;

    badDataStack.data = reinterpret_cast<char**>(unmapped);
    expectInvalid(&badDataStack, Item(0));
    Require(badDataStack.num == 0 && badDataStack.numAlloc == 4 && badDataStack.sorted == 1);

    badDataStack.data = reinterpret_cast<char**>(noAccess);
    expectInvalid(&badDataStack, Item(0));
    Require(badDataStack.num == 0 && badDataStack.numAlloc == 4 && badDataStack.sorted == 1);

    badDataStack.data = roData;
    expectInvalid(&badDataStack, Item(0));
    Require(badDataStack.num == 0 && badDataStack.numAlloc == 4 && badDataStack.sorted == 1);

    badDataStack.num = 3;
    badDataStack.data = reinterpret_cast<char**>(unmapped);
    expectInvalid(&badDataStack, Item(0));
    Require(badDataStack.num == 3 && badDataStack.numAlloc == 4 && badDataStack.sorted == 1);

    badDataStack.data = roData;
    expectInvalid(&badDataStack, Item(0));
    Require(badDataStack.num == 3 && badDataStack.numAlloc == 4 && badDataStack.sorted == 1);

    sk_free_nid_postfix(validStack);
    Require(allocations.empty());
}

}

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(UnexpectedCalloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    RoundTrip();
    Growth();
    FreeNull();
    AllocationFailures();
    GuestPointerValidation();
    std::puts("libSceLibreSsl3 stack tests passed");
}
