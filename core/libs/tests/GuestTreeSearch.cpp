#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

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

int APS5_VABI CompareInts(const void* left, const void* right) {
    const int a = *static_cast<const int*>(left);
    const int b = *static_cast<const int*>(right);
    return (a > b) - (a < b);
}

std::vector<int> visited;
int leaves = 0;
int preorders = 0;
int postorders = 0;
int endorders = 0;

void APS5_VABI Record(const void* node, int which, int) {
    const int value = *static_cast<const int*>(*static_cast<const void* const*>(node));
    if (which == 3) ++leaves;
    if (which == 0) ++preorders;
    if (which == 1) ++postorders;
    if (which == 2) ++endorders;
    if (which == 3 || which == 1) visited.push_back(value);
}

void ResetWalk() {
    visited.clear();
    leaves = preorders = postorders = endorders = 0;
}

template<class TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}

int main() {
    RegisterAllocator();
    constexpr int count = 1000;
    std::vector<int> values(count);
    for (int index = 0; index < count; ++index) values[index] = index * 2;
    void* root = nullptr;

    for (int index = 0; index < count; ++index) {
        void* slot = tsearch_nid_postfix(&values[index], &root, CompareInts);
        Require(slot != nullptr && *static_cast<const int* const*>(slot) == &values[index]);
    }
    for (int index = 0; index < count; ++index) {
        int probe = values[index];
        void* slot = tfind_nid_postfix(&probe, &root, CompareInts);
        Require(slot != nullptr && *static_cast<const int* const*>(slot) == &values[index]);
    }
    int missing = 1;
    Require(tfind_nid_postfix(&missing, &root, CompareInts) == nullptr);

    int duplicate = values[10];
    void* existing = tsearch_nid_postfix(&duplicate, &root, CompareInts);
    Require(*static_cast<const int* const*>(existing) == &values[10]);

    ResetWalk();
    twalk_nid_postfix(root, Record);
    Require(static_cast<int>(visited.size()) == count);
    for (int index = 0; index < count; ++index) Require(visited[index] == values[index]);
    Require(leaves + postorders == count && preorders == postorders && endorders == postorders);

    for (int index = 0; index < count; index += 2) {
        void* parent = tdelete_nid_postfix(&values[index], &root, CompareInts);
        Require(parent != nullptr);
    }
    Require(tdelete_nid_postfix(&values[0], &root, CompareInts) == nullptr);
    for (int index = 0; index < count; ++index) {
        int probe = values[index];
        const bool present = tfind_nid_postfix(&probe, &root, CompareInts) != nullptr;
        Require(present == (index % 2 == 1));
    }
    ResetWalk();
    twalk_nid_postfix(root, Record);
    Require(static_cast<int>(visited.size()) == count / 2);
    for (std::size_t index = 0; index < visited.size(); ++index) Require(visited[index] == values[2 * index + 1]);

    std::vector<int> sorted(100000);
    for (std::size_t index = 0; index < sorted.size(); ++index) sorted[index] = static_cast<int>(index);
    void* large = nullptr;
    for (auto& value : sorted) Require(tsearch_nid_postfix(&value, &large, CompareInts) != nullptr);
    ResetWalk();
    twalk_nid_postfix(large, Record);
    Require(visited.size() == sorted.size());
    Require(leaves + postorders == static_cast<int>(sorted.size()));

    void* single = nullptr;
    int lone = 7;
    Require(tsearch_nid_postfix(&lone, &single, CompareInts) != nullptr);
    Require(tdelete_nid_postfix(&lone, &single, CompareInts) == &single && single == nullptr);

    Require(Throws([&] { tsearch_nid_postfix(nullptr, &root, CompareInts); }));
    Require(tsearch_nid_postfix(&missing, nullptr, CompareInts) == nullptr);
    Require(tfind_nid_postfix(&missing, nullptr, CompareInts) == nullptr);
    Require(tdelete_nid_postfix(&missing, nullptr, CompareInts) == nullptr);
    Require(Throws([&] { tfind_nid_postfix(&missing, &root, nullptr); }));
    Require(Throws([&] { tdelete_nid_postfix(nullptr, &root, CompareInts); }));
    twalk_nid_postfix(root, nullptr);
    Require(root != nullptr);
    {
        int first = 1, second = 2, third = 3;
        void* tree = nullptr;
        Require(tsearch_nid_postfix(&second, &tree, CompareInts) != nullptr);
        Require(tsearch_nid_postfix(&first, &tree, CompareInts) != nullptr);
        Require(tsearch_nid_postfix(&third, &tree, CompareInts) != nullptr);
        void* parent = tdelete_nid_postfix(&first, &tree, CompareInts);
        Require(parent != nullptr && *static_cast<const int* const*>(parent) == &second);
    }
    return 0;
}
