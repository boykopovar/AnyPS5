#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI AlignedAllocate(std::size_t, std::size_t size) { return Allocate(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return Reallocate(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    if (pointer == nullptr) return 22;
    *pointer = Allocate(size);
    return *pointer == nullptr ? 12 : 0;
}

void RegisterAllocator() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&Allocate);
    api[1] = reinterpret_cast<void*>(&Free);
    api[2] = reinterpret_cast<void*>(&Calloc);
    api[3] = reinterpret_cast<void*>(&Reallocate);
    api[4] = reinterpret_cast<void*>(&AlignedAllocate);
    api[5] = reinterpret_cast<void*>(&Realign);
    api[6] = reinterpret_cast<void*>(&PosixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());
}

constexpr int Find = 0;
constexpr int Enter = 1;

}

int main() {
    RegisterAllocator();
    errno = 0;
    Require(hsearch_nid_postfix(HsearchEntry{const_cast<char*>("alpha"), nullptr}, Find) == nullptr && errno == 3);
    int marker = 0;
    HsearchEntry* lazy = hsearch_nid_postfix(HsearchEntry{const_cast<char*>("alpha"), &marker}, Enter);
    Require(lazy != nullptr && lazy->data == &marker);

    Require(hcreate_nid_postfix(0) == 1);
    Require(hcreate_nid_postfix(10) == 1);
    Require(hsearch_nid_postfix(HsearchEntry{const_cast<char*>("alpha"), nullptr}, Find) == lazy);
    hdestroy_nid_postfix();
    hdestroy_nid_postfix();
    errno = 0;
    Require(hsearch_nid_postfix(HsearchEntry{const_cast<char*>("alpha"), nullptr}, Find) == nullptr && errno == 3);

    Require(hcreate_nid_postfix(4) == 1);
    constexpr int Count = 1000;
    static char keys[Count][8];
    static int payloads[Count];
    for (int index = 0; index < Count; ++index) {
        std::snprintf(keys[index], sizeof(keys[index]), "k%03d", index);
        payloads[index] = index;
        HsearchEntry* entry = hsearch_nid_postfix(HsearchEntry{keys[index], &payloads[index]}, Enter);
        Require(entry != nullptr && entry->key == keys[index] && entry->data == &payloads[index]);
    }
    for (int index = 0; index < Count; ++index) {
        char probe[8];
        std::memcpy(probe, keys[index], sizeof(probe));
        HsearchEntry* entry = hsearch_nid_postfix(HsearchEntry{probe, nullptr}, Find);
        Require(entry != nullptr && entry->key == keys[index] && entry->data == &payloads[index]);
    }

    int replacement = 99;
    HsearchEntry* again = hsearch_nid_postfix(HsearchEntry{keys[3], &replacement}, Enter);
    Require(again != nullptr && again->data == &payloads[3]);
    errno = 0;
    Require(hsearch_nid_postfix(HsearchEntry{const_cast<char*>("absent"), nullptr}, Find) == nullptr && errno == 3);
    HsearchEntry* other = hsearch_nid_postfix(HsearchEntry{const_cast<char*>("other"), &replacement}, 7);
    Require(other != nullptr && other->data == &replacement);
    Require(hsearch_nid_postfix(HsearchEntry{const_cast<char*>("other"), nullptr}, Find) == other);
    hdestroy_nid_postfix();
    return 0;
}
