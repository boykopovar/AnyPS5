#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

bool failAllocation = false;

void* APS5_VABI Allocate(std::size_t size) { return failAllocation ? nullptr : std::malloc(size); }
void APS5_VABI Free(void* ptr) { std::free(ptr); }
void* APS5_VABI Reallocate(void* ptr, std::size_t size) { return std::realloc(ptr, size); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* ptr, std::size_t size, std::size_t) { return std::realloc(ptr, size); }
int APS5_VABI PosixAlign(void** ptr, std::size_t, std::size_t size) {
    *ptr = std::malloc(size);
    return *ptr ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

struct Element {
    int key;
    int tag;
};

int APS5_VABI CompareKeys(const void* left, const void* right) {
    const int first = static_cast<const Element*>(left)->key;
    const int second = static_cast<const Element*>(right)->key;
    return (first > second) - (first < second);
}

int APS5_VABI CompareFirstByte(const void* left, const void* right) {
    const int first = *static_cast<const unsigned char*>(left);
    const int second = *static_cast<const unsigned char*>(right);
    return (first > second) - (first < second);
}

int APS5_VABI CompareKeysWithDirection(const void* left, const void* right, void* context) {
    return *static_cast<const int*>(context) * CompareKeys(left, right);
}

int APS5_VABI CompareKeysWithThunk(void* thunk, const void* left, const void* right) {
    return *static_cast<const int*>(thunk) * CompareKeys(left, right);
}

bool ExpectOrderR(const char* name, const std::vector<int>& keys, const std::vector<int>& tags) {
    std::vector<Element> elements;
    for (std::size_t index = 0; index < keys.size(); ++index) elements.push_back({keys[index], static_cast<int>(index)});
    int direction = 1;
    qsort_r_nid_postfix(elements.data(), elements.size(), sizeof(Element), &direction, CompareKeysWithThunk);
    for (std::size_t index = 0; index < tags.size(); ++index) {
        if (elements[index].tag != tags[index]) {
            std::fprintf(stderr, "%s: qsort_r element %zu has tag %d, expected %d\n", name, index, elements[index].tag, tags[index]);
            return false;
        }
    }
    return true;
}

bool ExpectReverseOrderR() {
    std::vector<Element> elements{{1, 0}, {3, 1}, {2, 2}, {4, 3}};
    int direction = -1;
    qsort_r_nid_postfix(elements.data(), elements.size(), sizeof(Element), &direction, CompareKeysWithThunk);
    if (elements.size() != 4) return false;
    if (elements[0].key != 4 || elements[1].key != 3 || elements[2].key != 2 || elements[3].key != 1) {
        std::fprintf(stderr, "reverse qsort_r: elements not sorted in descending order\n");
        return false;
    }
    return true;
}

bool ExpectQsortRValidation() {
    Element pair[2] = {{2, 0}, {1, 1}};
    int direction = 1;
    qsort_r_nid_postfix(nullptr, 0, sizeof(Element), &direction, CompareKeysWithThunk);
    qsort_r_nid_postfix(pair, 1, sizeof(Element), &direction, CompareKeysWithThunk);
    bool threw = false;
    try {
        qsort_r_nid_postfix(pair, 2, sizeof(Element), &direction, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    if (!threw) return false;
    threw = false;
    try {
        qsort_r_nid_postfix(pair, 2, 0, &direction, CompareKeysWithThunk);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    if (!threw) return false;
    threw = false;
    try {
        qsort_r_nid_postfix(nullptr, 2, sizeof(Element), &direction, CompareKeysWithThunk);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    return threw;
}

bool ExpectOrderS(const char* name, const std::vector<int>& keys, const std::vector<int>& tags) {
    std::vector<Element> elements;
    for (std::size_t index = 0; index < keys.size(); ++index) elements.push_back({keys[index], static_cast<int>(index)});
    int direction = 1;
    if (qsort_s_nid_postfix(elements.data(), elements.size(), sizeof(Element), CompareKeysWithDirection, &direction) != 0) {
        std::fprintf(stderr, "%s: qsort_s failed\n", name);
        return false;
    }
    for (std::size_t index = 0; index < tags.size(); ++index) {
        if (elements[index].tag != tags[index]) {
            std::fprintf(stderr, "%s: qsort_s element %zu has tag %d, expected %d\n", name, index, elements[index].tag, tags[index]);
            return false;
        }
    }
    return true;
}

int constraintCalls = 0;
int constraintError = 0;
const char* constraintMessage = nullptr;

void APS5_VABI CountConstraint(const char* message, void*, int error) {
    ++constraintCalls;
    constraintMessage = message;
    constraintError = error;
}

bool ExpectConstraint(const char* name, void* base, std::size_t count, std::size_t size, bool comparator) {
    int direction = 1;
    const int calls = constraintCalls;
    const int result = qsort_s_nid_postfix(base, count, size, comparator ? CompareKeysWithDirection : nullptr, &direction);
    if (result != 22 || constraintCalls != calls + 1 || constraintError != 22 || !constraintMessage || std::strcmp(constraintMessage, name) != 0) {
        std::fprintf(stderr, "qsort_s did not report \"%s\" through the installed handler (result %d, calls %d)\n", name, result, constraintCalls - calls);
        return false;
    }
    return true;
}

bool ExpectQsortSConstraints() {
    void* previous = set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(CountConstraint));
    Element element{};
    constexpr std::size_t huge = (static_cast<std::size_t>(-1) >> 1) + 1;
    bool correct = ExpectConstraint("qsort_s : n > RSIZE_MAX", &element, huge, sizeof(Element), true);
    correct &= ExpectConstraint("qsort_s : es > RSIZE_MAX", &element, 1, huge, true);
    correct &= ExpectConstraint("qsort_s : a == NULL", nullptr, 2, sizeof(Element), true);
    correct &= ExpectConstraint("qsort_s : cmp == NULL", &element, 2, sizeof(Element), false);
    if (set_constraint_handler_s_nid_postfix(previous) != reinterpret_cast<void*>(CountConstraint)) {
        std::fprintf(stderr, "set_constraint_handler_s did not return the installed handler\n");
        correct = false;
    }
    return correct;
}

bool ExpectQsortS() {
    std::vector<Element> elements;
    for (int index = 0; index < 50; ++index) elements.push_back({(index * 13) % 17, index});
    int direction = -1;
    if (qsort_s_nid_postfix(elements.data(), elements.size(), sizeof(Element), CompareKeysWithDirection, &direction) != 0) return false;
    for (std::size_t index = 1; index < elements.size(); ++index) {
        if (elements[index - 1].key < elements[index].key) {
            std::fprintf(stderr, "qsort_s with a descending context is not sorted at %zu\n", index);
            return false;
        }
    }
    if (qsort_s_nid_postfix(nullptr, 0, sizeof(Element), nullptr, nullptr) != 0) {
        std::fprintf(stderr, "qsort_s of no elements failed\n");
        return false;
    }
    unsigned char bytes[] = {3, 1, 2};
    if (qsort_s_nid_postfix(bytes, 3, 0, CompareKeysWithDirection, &direction) != 0 || bytes[0] != 3 || bytes[1] != 1 || bytes[2] != 2) {
        std::fprintf(stderr, "qsort_s with zero-size elements changed them\n");
        return false;
    }
    return true;
}

bool ExpectOrder(const char* name, const std::vector<int>& keys, const std::vector<int>& tags) {
    std::vector<Element> elements;
    for (std::size_t index = 0; index < keys.size(); ++index) elements.push_back({keys[index], static_cast<int>(index)});
    qsort_nid_postfix(elements.data(), elements.size(), sizeof(Element), CompareKeys);

    for (std::size_t index = 0; index < tags.size(); ++index) {
        if (elements[index].tag != tags[index]) {
            std::fprintf(stderr, "%s: element %zu has tag %d, expected %d\n", name, index, elements[index].tag, tags[index]);
            return false;
        }
    }
    return true;
}

bool ExpectSorted(std::size_t size, std::size_t count) {
    std::vector<unsigned char> bytes(size * count);
    unsigned int state = static_cast<unsigned int>(size * 31 + count);
    for (auto& byte : bytes) {
        state = state * 1103515245u + 12345u;
        byte = static_cast<unsigned char>(state >> 16);
    }
    std::vector<unsigned char> counts(256);
    for (std::size_t index = 0; index < count; ++index) ++counts[bytes[index * size]];
    qsort_nid_postfix(bytes.data(), count, size, CompareFirstByte);

    for (std::size_t index = 0; index < count; ++index) {
        if (index > 0 && bytes[(index - 1) * size] > bytes[index * size]) {
            std::fprintf(stderr, "qsort of %zu %zu-byte elements is not sorted at %zu\n", count, size, index);
            return false;
        }
        --counts[bytes[index * size]];
    }
    for (const unsigned char remaining : counts) {
        if (remaining != 0) {
            std::fprintf(stderr, "qsort of %zu %zu-byte elements lost or duplicated elements\n", count, size);
            return false;
        }
    }
    return true;
}

bool CheckMergesort() {
    InitializeHeap();
    bool correct = true;
    for (const std::size_t count : {0, 1, 2, 3, 7, 16, 33, 1000}) {
        std::vector<Element> elements;
        for (std::size_t index = 0; index < count; ++index) elements.push_back({static_cast<int>((index * 7919) % 5), static_cast<int>(index)});
        correct &= mergesort_nid_postfix(elements.data(), count, sizeof(Element), CompareKeys) == 0;
        for (std::size_t index = 1; index < count; ++index) {
            const auto& before = elements[index - 1];
            const auto& after = elements[index];
            correct &= before.key < after.key || (before.key == after.key && before.tag < after.tag);
        }
    }
    unsigned char bytes[3] = {3, 1, 2};
    *__error_nid_postfix() = 0;
    correct &= mergesort_nid_postfix(bytes, 3, 1, CompareFirstByte) == -1 && *__error_nid_postfix() == 22 && bytes[0] == 3;
    correct &= mergesort_nid_postfix(nullptr, 0, 4, CompareKeys) == 0;
    Element pair[2] = {{2, 0}, {1, 1}};
    failAllocation = true;
    correct &= mergesort_nid_postfix(pair, 2, sizeof(Element), CompareKeys) == -1 && pair[0].key == 2;
    failAllocation = false;
    if (!correct) std::fprintf(stderr, "mergesort failed\n");
    return correct;
}

bool CheckHeapsort() {
    bool correct = true;
    for (const std::size_t count : {0, 1, 2, 3, 7, 16, 33, 1000}) {
        std::vector<Element> elements;
        for (std::size_t index = 0; index < count; ++index) elements.push_back({static_cast<int>((index * 7919) % 5), static_cast<int>(index)});
        correct &= heapsort_nid_postfix(elements.data(), count, sizeof(Element), CompareKeys) == 0;
        for (std::size_t index = 1; index < count; ++index) {
            const auto& before = elements[index - 1];
            const auto& after = elements[index];
            correct &= before.key <= after.key;
        }
    }
    unsigned char bytes[3] = {3, 1, 2};
    *__error_nid_postfix() = 0;
    correct &= heapsort_nid_postfix(nullptr, 0, 0, nullptr) == 0;
    unsigned char single = 42;
    correct &= heapsort_nid_postfix(&single, 1, 0, nullptr) == 0;
    correct &= heapsort_nid_postfix(bytes, 3, 0, CompareFirstByte) == -1 && *__error_nid_postfix() == 22 && bytes[0] == 3;
    correct &= heapsort_nid_postfix(nullptr, 0, 4, CompareKeys) == 0;
    if (!correct) std::fprintf(stderr, "heapsort failed\n");
    return correct;
}
}

int main() {
    bool correct = true;
    correct &= ExpectOrder("insertion sort", {1, 0, 1, 0}, {1, 3, 0, 2});
    correct &= ExpectOrder("median of three", {2, 1, 2, 0, 1, 2, 0, 1, 0, 2, 1, 0}, {11, 3, 6, 8, 10, 1, 7, 4, 9, 5, 0, 2});
    correct &= ExpectOrderS("insertion sort", {1, 0, 1, 0}, {1, 3, 0, 2});
    correct &= ExpectOrderS("median of three", {2, 1, 2, 0, 1, 2, 0, 1, 0, 2, 1, 0}, {11, 3, 6, 8, 10, 1, 7, 4, 9, 5, 0, 2});
    correct &= ExpectOrderR("insertion sort", {1, 0, 1, 0}, {1, 3, 0, 2});
    correct &= ExpectOrderR("median of three", {2, 1, 2, 0, 1, 2, 0, 1, 0, 2, 1, 0}, {11, 3, 6, 8, 10, 1, 7, 4, 9, 5, 0, 2});
    correct &= ExpectReverseOrderR();
    correct &= ExpectQsortRValidation();
    correct &= ExpectQsortS();
    correct &= ExpectQsortSConstraints();
    std::vector<int> keys;
    for (int index = 0; index < 48; ++index) keys.push_back((index * 7 + 3) % 5);
    correct &= ExpectOrder("pseudomedian of nine", keys,
        {21, 11, 36, 6, 1, 31, 16, 46, 41, 26, 29, 4, 24, 44, 9, 39, 34, 14, 19, 12, 2, 7, 17, 22,
         27, 32, 37, 42, 47, 5, 40, 30, 15, 35, 25, 20, 0, 45, 10, 28, 33, 13, 38, 8, 3, 23, 43, 18});
    for (const std::size_t size : {1, 3, 8, 24})
        for (const std::size_t count : {0, 1, 2, 6, 7, 8, 40, 41, 1000})
            correct &= ExpectSorted(size, count);
    correct &= CheckMergesort();
    correct &= CheckHeapsort();
    return correct ? 0 : 1;
}
