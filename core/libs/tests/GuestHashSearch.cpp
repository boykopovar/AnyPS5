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
    Require(hsearch_nid_postfix(HsearchEntry{const_cast<char*>("alpha"), nullptr}, Find) == nullptr && errno == 3);

    Require(hcreate_nid_postfix(0) == 1);
    {
        int marker = 0;
        std::array<char[8], 17> minimum{};
        for (int index = 0; index < 17; ++index) std::snprintf(minimum[index], sizeof(minimum[index]), "m%02d", index);
        for (int index = 0; index < 16; ++index) Require(hsearch_nid_postfix(HsearchEntry{minimum[index], &marker}, Enter) != nullptr);
        errno = 0;
        Require(hsearch_nid_postfix(HsearchEntry{minimum[16], &marker}, Enter) == nullptr && errno == 12);
        hdestroy_nid_postfix();
        Require(hcreate_nid_postfix(0) == 1);
    }
    Require(hcreate_nid_postfix(10) == 0 && errno == 17);
    hdestroy_nid_postfix();
    Require(hcreate_nid_postfix(10) == 1);

    std::array<char[8], 8> keys{"k0", "k1", "k2", "k3", "k4", "k5", "k6", "k7"};
    int payloads[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (int index = 0; index < 8; ++index) {
        HsearchEntry* entry = hsearch_nid_postfix(HsearchEntry{keys[index], &payloads[index]}, Enter);
        Require(entry != nullptr && entry->data == &payloads[index]);
    }
    for (int index = 0; index < 8; ++index) {
        char probe[8];
        std::memcpy(probe, keys[index], sizeof(probe));
        HsearchEntry* entry = hsearch_nid_postfix(HsearchEntry{probe, nullptr}, Find);
        Require(entry != nullptr && entry->data == &payloads[index] && std::strcmp(entry->key, keys[index]) == 0);
    }

    int replacement = 99;
    HsearchEntry* again = hsearch_nid_postfix(HsearchEntry{const_cast<char*>("k3"), &replacement}, Enter);
    Require(again != nullptr && again->data == &payloads[3]);
    errno = 0;
    Require(hsearch_nid_postfix(HsearchEntry{const_cast<char*>("absent"), nullptr}, Find) == nullptr && errno == 0);

    std::array<char[8], 24> extra{};
    for (int index = 0; index < 24; ++index) std::snprintf(extra[index], sizeof(extra[index]), "x%02d", index);
    errno = 0;
    for (int index = 0; index < 24; ++index) Require(hsearch_nid_postfix(HsearchEntry{extra[index], &replacement}, Enter) != nullptr);
    Require(hsearch_nid_postfix(HsearchEntry{const_cast<char*>("overflow"), &replacement}, Enter) == nullptr && errno == 12);

    hdestroy_nid_postfix();
    hdestroy_nid_postfix();
    Require(hsearch_nid_postfix(HsearchEntry{keys[0], nullptr}, Find) == nullptr && errno == 3);
    Require(hcreate_nid_postfix(4) == 1);
    Require(hsearch_nid_postfix(HsearchEntry{keys[0], nullptr}, Find) == nullptr);
    Require(hsearch_nid_postfix(HsearchEntry{keys[0], &payloads[0]}, 7) == nullptr && errno == 22);
    hdestroy_nid_postfix();
    return 0;
}
