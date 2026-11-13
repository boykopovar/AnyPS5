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
std::size_t allocations = 0;
std::size_t releases = 0;

void* APS5_VABI Allocate(std::size_t size) {
    void* pointer = failAllocation ? nullptr : std::malloc(size);
    if (pointer) ++allocations;
    return pointer;
}
void APS5_VABI Free(void* ptr) {
    if (ptr) ++releases;
    std::free(ptr);
}
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
std::uint32_t comparisonHash = 2166136261u;
std::size_t comparisonCount = 0;

int APS5_VABI CompareSelfSwap(const void* left, const void* right) {
    comparisonHash = (comparisonHash ^ static_cast<std::uint32_t>(static_cast<const Element*>(left)->tag)) * 16777619u;
    comparisonHash = (comparisonHash ^ static_cast<std::uint32_t>(static_cast<const Element*>(right)->tag)) * 16777619u;
    ++comparisonCount;
    return CompareKeys(left, right);
}

int APS5_VABI CompareSelfSwapS(const void* left, const void* right, void* context) {
    return *static_cast<const int*>(context) * CompareSelfSwap(left, right);
}

bool ExpectSelfSwap() {
    constexpr std::array<int, 33> keys{3, 1, 0, 1, 0, 1, 1, 0, 1, 3, 3, 2, 0, 2, 2, 3, 2, 1, 2, 3, 0, 0, 0, 3, 0, 2, 0, 3, 0, 2, 3, 1, 3};
    constexpr std::array<int, 33> tags{2, 4, 7, 12, 20, 21, 22, 24, 26, 28, 1, 3, 5, 6, 8, 17, 31, 16, 13, 14, 11, 18, 25, 29, 10, 19, 15, 27, 0, 9, 30, 23, 32};
    for (const bool withContext : {false, true}) {
        std::array<Element, keys.size()> elements{};
        for (std::size_t i = 0; i < keys.size(); ++i) elements[i] = {keys[i], static_cast<int>(i)};
        comparisonHash = 2166136261u;
        comparisonCount = 0;
        int direction = 1;
        if (withContext) {
            if (qsort_s_nid_postfix(elements.data(), elements.size(), sizeof(Element), CompareSelfSwapS, &direction) != 0) return false;
        } else qsort_nid_postfix(elements.data(), elements.size(), sizeof(Element), CompareSelfSwap);
        if (comparisonCount != 181 || comparisonHash != 0x78b091e1u) {
            std::fprintf(stderr, "partition self-swap: comparator order differs (%zu calls, hash %08x)\n", comparisonCount, comparisonHash);
            return false;
        }
        for (std::size_t i = 0; i < tags.size(); ++i) {
            if (elements[i].tag != tags[i]) {
                std::fprintf(stderr, "partition self-swap: element %zu has tag %d, expected %d\n", i, elements[i].tag, tags[i]);
                return false;
            }
        }
    }
    return true;
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
void* constraintPointer = nullptr;

void APS5_VABI CountConstraint(const char* message, void* pointer, int error) {
    ++constraintCalls;
    constraintMessage = message;
    constraintPointer = pointer;
    constraintError = error;
}

bool ExpectConstraint(const char* name, void* base, std::size_t count, std::size_t size, bool comparator) {
    int direction = 1;
    const int calls = constraintCalls;
    const int result = qsort_s_nid_postfix(base, count, size, comparator ? CompareKeysWithDirection : nullptr, &direction);
    if (result != 22 || constraintCalls != calls + 1 || constraintError != 22 || constraintPointer || !constraintMessage || std::strcmp(constraintMessage, name) != 0) {
        std::fprintf(stderr, "qsort_s did not report \"%s\" through the installed handler (result %d, calls %d)\n", name, result, constraintCalls - calls);
        return false;
    }
    return true;
}

bool ExpectQsortSConstraints() {
    void* previous = set_constraint_handler_s_nid_postfix(reinterpret_cast<void*>(CountConstraint));
    Element element{};
    constexpr std::size_t huge = (static_cast<std::size_t>(-1) >> 1) + 1;
    constexpr std::size_t aboveLimit = std::size_t{1} << 61;
    bool correct = ExpectConstraint("qsort_s: bad argument", &element, huge, sizeof(Element), true);
    correct &= ExpectConstraint("qsort_s: bad argument", &element, 1, huge, true);
    correct &= ExpectConstraint("qsort_s: bad argument", &element, aboveLimit, sizeof(Element), true);
    correct &= ExpectConstraint("qsort_s: bad argument", &element, 1, aboveLimit, true);
    correct &= ExpectConstraint("qsort_s: bad argument", &element, 0, aboveLimit, true);
    correct &= ExpectConstraint("qsort_s: bad argument", nullptr, 2, sizeof(Element), true);
    correct &= ExpectConstraint("qsort_s: bad argument", &element, 2, sizeof(Element), false);
    int direction = 1;
    const int calls = constraintCalls;
    bool zeroSizeThrew = false;
    try {
        qsort_s_nid_postfix(&element, aboveLimit - 1, 0, CompareKeysWithDirection, &direction);
    } catch (const std::invalid_argument&) {
        zeroSizeThrew = true;
    }
    if (!zeroSizeThrew) {
        std::fprintf(stderr, "qsort_s accepted a zero element size for SIZE_MAX >> 3 elements\n");
        correct = false;
    }
    correct &= qsort_s_nid_postfix(&element, 1, aboveLimit - 1, CompareKeysWithDirection, &direction) == 0;
    if (constraintCalls != calls) {
        std::fprintf(stderr, "qsort_s reported a count or size of SIZE_MAX >> 3\n");
        correct = false;
    }
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
    alignas(Element) unsigned char bytes[sizeof(Element)] = {3, 1, 2};
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

bool ExpectOrderHash(const char* name, const std::vector<int>& keys, std::uint32_t expected) {
    std::vector<Element> elements;
    for (std::size_t index = 0; index < keys.size(); ++index) elements.push_back({keys[index], static_cast<int>(index)});
    qsort_nid_postfix(elements.data(), elements.size(), sizeof(Element), CompareKeys);
    std::uint32_t hash = 2166136261u;
    for (const auto& element : elements) hash = (hash ^ static_cast<std::uint32_t>(element.tag)) * 16777619u;
    if (hash != expected) {
        std::fprintf(stderr, "%s: tag order hashes to %08x, expected %08x\n", name, hash, expected);
        return false;
    }
    return true;
}

int comparisons = 0;

int APS5_VABI CountFirstByte(const void* left, const void* right) {
    ++comparisons;
    return CompareFirstByte(left, right);
}

bool ExpectConsoleEdges() {
    unsigned char bytes[] = {3, 1, 2};
    qsort_nid_postfix(nullptr, 5, 1, CountFirstByte);
    qsort_nid_postfix(bytes, 3, 1, nullptr);
    bool correct = comparisons == 0 && bytes[0] == 3 && bytes[1] == 1 && bytes[2] == 2;
    qsort_nid_postfix(bytes, 3, 0, CountFirstByte);
    correct &= comparisons == 4 && bytes[0] == 3 && bytes[1] == 1 && bytes[2] == 2;
    if (!correct) std::fprintf(stderr, "qsort with a null base or comparator or zero-size elements: %d comparisons, bytes %d %d %d\n", comparisons, bytes[0], bytes[1], bytes[2]);
    return correct;
}

bool ExpectShortArraysUntouched() {
    int values[2] = {2, 1};
    qsort_nid_postfix(values, 0, sizeof(int), nullptr);
    qsort_nid_postfix(values, 1, sizeof(int), nullptr);
    qsort_nid_postfix(values, 0, 0, CompareKeys);
    qsort_nid_postfix(values, 1, 0, CompareKeys);
    qsort_nid_postfix(nullptr, 0, 0, nullptr);
    if (values[0] != 2 || values[1] != 1) {
        std::fprintf(stderr, "qsort of zero or one element changed the array\n");
        return false;
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

std::vector<int> mergesortCalls;

int APS5_VABI RecordKeys(const void* left, const void* right) {
    const auto* first = static_cast<const Element*>(left);
    const auto* second = static_cast<const Element*>(right);
    mergesortCalls.push_back(first->tag * 100 + second->tag);
    return (first->key > second->key) - (first->key < second->key);
}

bool CheckMergesortComparisons() {
    InitializeHeap();
    struct Case {
        std::vector<int> keys;
        std::vector<int> calls;
    };
    const std::vector<Case> cases = {
        {{3, 1, 2, 5, 4}, {1, 2, 102, 3, 304, 4}},
        {{9, 4, 7, 1, 8, 2, 6, 3, 5, 0, 9, 4, 7, 1, 8, 2, 6, 3, 5, 0, 11},
         {1617, 1618, 1718, 1619, 1819, 1719, 1620, 1, 203, 405, 607, 809, 1011, 1213, 1415, 102, 304, 506, 708, 910, 1112, 1314, 1419, 103, 102, 2, 507, 704,
          406, 911, 1108, 810, 1315, 1512, 1214, 305, 501, 107, 106, 206, 204, 4, 913, 1311, 1115, 1112, 812, 1210, 1014, 309, 313, 513, 515, 715, 711, 111,
          1106, 608, 612, 212, 1204, 414, 1400, 10, 919, 1903, 317, 1713, 1705, 1715, 1707, 1701, 118, 1811, 1808, 1806, 616, 1602, 220, 2012, 2004, 2014, 2000,
          2010}},
        {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 0, 0, 30, 29, 28, 27},
         {2223, 2224, 2324, 2225, 2325, 2425, 1, 203, 405, 607, 809, 1011, 1213, 1415, 1617, 1819, 2021, 102, 304, 506, 708, 910, 1112, 1314, 1516, 1718,
          1920, 2125, 20, 21, 25, 125, 2502, 2503, 2504, 2505, 2506, 2507, 2508, 2510, 2514, 2519}},
        {{40, 41, 42, 43, 44, 45, 46, 47, 0, 1, 2, 3, 4, 5, 6, 7, 48, 49, 50, 51, 52, 53, 54, 55, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17},
         {3031, 3132, 3233, 1, 203, 405, 607, 809, 1011, 1213, 1415, 1617, 1819, 2021, 2223, 2425, 2627, 2829, 102, 304, 506, 708, 910, 1112, 1314, 1516, 1718,
          1920, 2122, 2324, 2526, 2728, 2930, 8, 9, 10, 11, 12, 13, 14, 15, 17, 16, 116, 1602, 1604, 1607, 824, 2409, 2411, 2415, 2407, 2403, 2401, 2400, 25, 26,
          28, 32, 33}},
    };
    bool correct = true;
    for (const auto& sample : cases) {
        std::vector<Element> elements;
        for (std::size_t index = 0; index < sample.keys.size(); ++index) elements.push_back({sample.keys[index], static_cast<int>(index)});
        mergesortCalls.clear();
        correct &= mergesort_nid_postfix(elements.data(), elements.size(), sizeof(Element), RecordKeys) == 0;
        if (mergesortCalls != sample.calls) {
            std::fprintf(stderr, "mergesort of %zu elements made %zu comparisons in a different order than FreeBSD's merge.c (%zu expected)\n",
                sample.keys.size(), mergesortCalls.size(), sample.calls.size());
            correct = false;
        }
        for (std::size_t index = 1; index < elements.size(); ++index) {
            correct &= elements[index - 1].key < elements[index].key ||
                (elements[index - 1].key == elements[index].key && elements[index - 1].tag < elements[index].tag);
        }
    }
    return correct;
}

bool CheckMergesortUnalignedElements() {
    InitializeHeap();
    constexpr std::size_t Size = 12;
    constexpr std::size_t Count = 1000;
    std::vector<unsigned char> storage(Count * Size + 8);
    unsigned char* const base = storage.data() + 4;
    for (std::size_t index = 0; index < Count; ++index) {
        const Element element{static_cast<int>((index / 50) % 2 ? Count - index : (index * 7919) % 13), static_cast<int>(index)};
        std::memcpy(base + index * Size, &element, sizeof(element));
        std::memset(base + index * Size + sizeof(element), static_cast<int>(index & 0xff), Size - sizeof(element));
    }
    bool correct = mergesort_nid_postfix(base, Count, Size, CompareKeys) == 0;
    Element previous{};
    for (std::size_t index = 0; index < Count; ++index) {
        Element element;
        std::memcpy(&element, base + index * Size, sizeof(element));
        correct &= base[index * Size + sizeof(element)] == static_cast<unsigned char>(element.tag & 0xff);
        if (index != 0) correct &= previous.key < element.key || (previous.key == element.key && previous.tag < element.tag);
        previous = element;
    }
    if (!correct) std::fprintf(stderr, "mergesort of unaligned 12-byte elements failed\n");
    return correct;
}

std::vector<int> adversaryValues;
int adversaryGas = 0;
int adversarySolid = 0;
int adversaryCandidate = 0;
std::size_t adversaryComparisons = 0;

int APS5_VABI CompareAdversary(const void* left, const void* right) {
    ++adversaryComparisons;
    const int first = *static_cast<const int*>(left);
    const int second = *static_cast<const int*>(right);
    if (adversaryValues[first] == adversaryGas && adversaryValues[second] == adversaryGas)
        adversaryValues[first == adversaryCandidate ? first : second] = adversarySolid++;
    if (adversaryValues[first] == adversaryGas) adversaryCandidate = first;
    else if (adversaryValues[second] == adversaryGas) adversaryCandidate = second;
    return (adversaryValues[first] > adversaryValues[second]) - (adversaryValues[first] < adversaryValues[second]);
}

int APS5_VABI CompareAdversaryS(const void* left, const void* right, void*) {
    return CompareAdversary(left, right);
}

bool ExpectAdversaryFallsBackToHeapSort(bool withContext) {
    constexpr int Count = 4096;
    constexpr std::size_t Budget = 6 * Count * 12;
    std::vector<int> elements(Count);
    for (int index = 0; index < Count; ++index) elements[index] = index;
    adversaryValues.assign(Count, Count);
    adversaryGas = Count;
    adversarySolid = 0;
    adversaryCandidate = 0;
    adversaryComparisons = 0;
    if (withContext) {
        if (qsort_s_nid_postfix(elements.data(), elements.size(), sizeof(int), CompareAdversaryS, nullptr) != 0) return false;
    } else qsort_nid_postfix(elements.data(), elements.size(), sizeof(int), CompareAdversary);
    const char* const name = withContext ? "qsort_s" : "qsort";
    std::vector<bool> seen(Count);
    for (int index = 0; index < Count; ++index) {
        const int element = elements[index];
        if (element < 0 || element >= Count || seen[element]) {
            std::fprintf(stderr, "%s of an adversarial input lost or duplicated element %d\n", name, element);
            return false;
        }
        seen[element] = true;
        if (index > 0 && adversaryValues[elements[index - 1]] > adversaryValues[element]) {
            std::fprintf(stderr, "%s of an adversarial input is not sorted at %d\n", name, index);
            return false;
        }
    }
    if (adversaryComparisons > Budget) {
        std::fprintf(stderr, "%s of %d adversarial elements made %zu comparisons, more than %zu: no heap sort fallback\n", name, Count, adversaryComparisons, Budget);
        return false;
    }
    return true;
}

std::uintptr_t heapsortBase = 0;
std::uintptr_t heapsortLimit = 0;
std::size_t heapsortStride = 0;
bool heapsortArgumentsValid = true;
int heapsortCompareCalls = 0;

int APS5_VABI RecordingCompareKeys(const void* left, const void* right) {
    ++heapsortCompareCalls;
    const auto inArray = [](const void* candidate) {
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(candidate);
        return address >= heapsortBase && address < heapsortLimit
            && (address - heapsortBase) % heapsortStride == 0;
    };
    const bool leftInArray = inArray(left);
    const bool rightInArray = inArray(right);
    if (!leftInArray && !rightInArray) heapsortArgumentsValid = false;
    return CompareKeys(left, right);
}

bool CheckHeapsort() {
    InitializeHeap();
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
    for (const std::size_t count : {0, 1, 2, 3, 7, 16, 33, 1000}) {
        std::vector<Element> elements;
        for (std::size_t index = 0; index < count; ++index) elements.push_back({static_cast<int>((index * 7919) % 11), static_cast<int>(index)});
        std::vector<int> counts(11);
        for (const auto& element : elements) ++counts[element.key];
        correct &= heapsort_nid_postfix(elements.data(), count, sizeof(Element), CompareKeys) == 0;
        for (std::size_t index = 1; index < count; ++index) correct &= elements[index - 1].key <= elements[index].key;
        std::vector<bool> seen(count);
        for (const auto& element : elements) {
            if (element.key < 0 || element.key >= 11 || element.tag < 0 || static_cast<std::size_t>(element.tag) >= count) {
                correct = false;
                continue;
            }
            --counts[element.key];
            correct &= !seen[element.tag] && element.key == (element.tag * 7919) % 11;
            seen[element.tag] = true;
        }
        for (const int remaining : counts) correct &= remaining == 0;
    }
    {
        unsigned char payload[8] = {5, 2, 4, 1, 3, 2, 5, 1};
        correct &= heapsort_nid_postfix(payload, 8, 1, CompareFirstByte) == 0;
        const unsigned char expected[8] = {1, 1, 2, 2, 3, 4, 5, 5};
        correct &= std::memcmp(payload, expected, sizeof(payload)) == 0;
    }
    {
        struct ThreeByte { unsigned char key; unsigned char tag[2]; };
        ThreeByte records[6] = {{4, {1,17}}, {1, {2,29}}, {4, {3,43}}, {2, {4,61}}, {0, {5,83}}, {3, {6,107}}};
        static_assert(sizeof(ThreeByte) == 3);
        const auto original = std::to_array(records);
        correct &= heapsort_nid_postfix(records, 6, sizeof(ThreeByte), CompareFirstByte) == 0;
        for (std::size_t index = 1; index < 6; ++index) correct &= records[index - 1].key <= records[index].key;
        std::array<bool, 6> seen{};
        for (const auto& record : records) {
            if (record.tag[0] < 1 || record.tag[0] > 6) {
                correct = false;
                continue;
            }
            const auto index = record.tag[0] - 1;
            correct &= !seen[index] && record.key == original[index].key && record.tag[1] == original[index].tag[1];
            seen[index] = true;
        }
    }
    {
        std::vector<Element> elements;
        for (int index = 0; index < 64; ++index) elements.push_back({(index * 11) % 7, index});
        heapsortBase = reinterpret_cast<std::uintptr_t>(elements.data());
        heapsortStride = sizeof(Element);
        heapsortLimit = heapsortBase + elements.size() * heapsortStride;
        heapsortArgumentsValid = true;
        heapsortCompareCalls = 0;
        correct &= heapsort_nid_postfix(elements.data(), elements.size(), sizeof(Element), RecordingCompareKeys) == 0 && heapsortArgumentsValid && heapsortCompareCalls > 0;
        for (std::size_t index = 1; index < elements.size(); ++index) correct &= elements[index - 1].key <= elements[index].key;
    }
    unsigned char bytes[3] = {3, 1, 2};
    *__error_nid_postfix() = 0;
    correct &= heapsort_nid_postfix(nullptr, 0, 0, nullptr) == 0;
    unsigned char single = 42;
    correct &= heapsort_nid_postfix(&single, 1, 0, nullptr) == 0;
    correct &= heapsort_nid_postfix(bytes, 3, 0, CompareFirstByte) == -1 && *__error_nid_postfix() == 22 && bytes[0] == 3;
    correct &= heapsort_nid_postfix(nullptr, 0, 4, CompareKeys) == 0;
    correct &= heapsort_nid_postfix(nullptr, 1, 4, CompareKeys) == 0;
    correct &= heapsort_nid_postfix(nullptr, 1, 0, CompareKeys) == 0;
    Element pair[2] = {{2, 0}, {1, 1}};
    failAllocation = true;
    correct &= heapsort_nid_postfix(pair, 2, sizeof(Element), CompareKeys) == -1 && pair[0].key == 2;
    failAllocation = false;
    if (!correct) std::fprintf(stderr, "heapsort failed\n");
    return correct;
}

std::vector<int> heapsortCalls;

int APS5_VABI RecordHeapsortCalls(const void* left, const void* right) {
    const auto identify = [](const void* candidate) {
        const auto address = reinterpret_cast<std::uintptr_t>(candidate);
        const bool inArray = address >= heapsortBase && address < heapsortLimit;
        return static_cast<const Element*>(candidate)->tag + (inArray ? 0 : 100);
    };
    heapsortCalls.push_back(identify(left) * 1000 + identify(right));
    return CompareKeys(left, right);
}

bool CheckHeapsortComparisons() {
    InitializeHeap();
    struct Case {
        std::vector<int> keys;
        std::vector<int> calls;
        std::vector<int> order;
    };
    const std::vector<Case> cases = {
        {{3, 1, 2, 5, 4}, {3004, 3001, 3002, 3000, 1004, 4000, 4002, 100001, 100004, 2, 101000, 102001}, {1, 2, 0, 4, 3}},
        {{1, 1, 1, 1, 1}, {3004, 3001, 1002, 1000, 1002, 104003, 104001, 1002, 103001, 102001}, {1, 2, 3, 4, 0}},
        {{9, 4, 7, 1, 8, 2, 6, 3, 5, 0, 9, 4, 7, 1, 8, 2},
         {15007, 13014, 14006, 11012, 12005, 9010, 10004, 7008, 8003, 12014, 14002, 13006, 6002, 8010, 10001, 9004, 4001, 10014, 10000, 10014, 8004, 9001,
          115001, 4014, 8001, 7003, 106007, 106008, 106004, 6014, 12002, 11005, 113011, 6012, 11002, 105002, 6002, 11005, 113011, 6011, 8001, 7003, 115007,
          8011, 7001, 109001, 1011, 7009, 103015, 7011, 13005, 103005, 7005, 15009, 103015, 15005, 3009, 113003, 113015, 13005, 109005, 13009, 103013,
          109013},
         {9, 13, 3, 5, 15, 7, 11, 1, 8, 6, 2, 12, 14, 4, 10, 0}},
        {{1, 3, 2}, {1002, 1000, 102000}, {0, 2, 1}},
    };
    bool correct = true;
    for (const auto& sample : cases) {
        std::vector<Element> elements;
        for (std::size_t index = 0; index < sample.keys.size(); ++index) elements.push_back({sample.keys[index], static_cast<int>(index)});
        heapsortBase = reinterpret_cast<std::uintptr_t>(elements.data());
        heapsortLimit = heapsortBase + elements.size() * sizeof(Element);
        heapsortCalls.clear();
        correct &= heapsort_nid_postfix(elements.data(), elements.size(), sizeof(Element), RecordHeapsortCalls) == 0;
        if (heapsortCalls != sample.calls) {
            std::fprintf(stderr, "heapsort of %zu elements made %zu comparisons in a different order than FreeBSD's heapsort.c (%zu expected)\n",
                sample.keys.size(), heapsortCalls.size(), sample.calls.size());
            correct = false;
        }
        for (std::size_t index = 0; index < elements.size(); ++index) correct &= elements[index].tag == sample.order[index];
    }
    return correct;
}

struct ComparisonFailure {};
std::size_t mergeComparisons = 0;
std::size_t mergeFailureAt = 0;

int APS5_VABI ThrowingCompare(const void* left, const void* right) {
    if (++mergeComparisons == mergeFailureAt) throw ComparisonFailure{};
    return CompareKeys(left, right);
}

bool CheckMergesortCleanup() {
    std::array<Element, 17> original{};
    for (std::size_t index = 0; index < original.size(); ++index) original[index] = {static_cast<int>((index * 7) % 5), static_cast<int>(index)};
    auto elements = original;
    mergeComparisons = 0;
    if (mergesort_nid_postfix(elements.data(), elements.size(), sizeof(Element), ThrowingCompare) != 0) return false;
    const auto total = mergeComparisons;
    for (std::size_t failure = 1; failure <= total; ++failure) {
        elements = original;
        mergeComparisons = 0;
        mergeFailureAt = failure;
        const auto allocated = allocations;
        const auto released = releases;
        bool caught = false;
        try {
            mergesort_nid_postfix(elements.data(), elements.size(), sizeof(Element), ThrowingCompare);
        } catch (const ComparisonFailure&) {
            caught = true;
        }
        if (!caught || mergeComparisons != failure || allocations != allocated + 1 || releases != released + 1) {
            std::fprintf(stderr, "mergesort cleanup failed at comparison %zu: caught=%d allocations=%zu releases=%zu\n",
                failure, caught, allocations - allocated, releases - released);
            return false;
        }
    }
    mergeFailureAt = 0;
    elements = original;
    if (mergesort_nid_postfix(elements.data(), elements.size(), sizeof(Element), CompareKeys) != 0 || allocations != releases) return false;
    for (std::size_t index = 1; index < elements.size(); ++index) {
        const auto& previous = elements[index - 1];
        const auto& current = elements[index];
        if (previous.key > current.key || (previous.key == current.key && previous.tag > current.tag)) return false;
    }
    return true;
}
}

int main() {
    bool correct = true;
    correct &= ExpectOrder("insertion sort", {1, 0, 1, 0}, {1, 3, 0, 2});
    correct &= ExpectOrder("insertion sort below 33 elements", {2, 1, 2, 0, 1, 2, 0, 1, 0, 2, 1, 0}, {3, 6, 8, 11, 1, 4, 7, 10, 0, 2, 5, 9});
    correct &= ExpectOrderS("insertion sort", {1, 0, 1, 0}, {1, 3, 0, 2});
    correct &= ExpectOrderS("insertion sort below 33 elements", {2, 1, 2, 0, 1, 2, 0, 1, 0, 2, 1, 0}, {3, 6, 8, 11, 1, 4, 7, 10, 0, 2, 5, 9});
    correct &= ExpectSelfSwap();
    correct &= ExpectOrderR("insertion sort", {1, 0, 1, 0}, {1, 3, 0, 2});
    correct &= ExpectOrderR("median of three", {2, 1, 2, 0, 1, 2, 0, 1, 0, 2, 1, 0}, {11, 3, 6, 8, 10, 1, 7, 4, 9, 5, 0, 2});
    correct &= ExpectReverseOrderR();
    correct &= ExpectQsortRValidation();
    correct &= ExpectQsortS();
    correct &= ExpectQsortSConstraints();
    std::vector<int> keys;
    for (int index = 0; index < 48; ++index) keys.push_back((index * 7 + 3) % 5);
    const std::vector<int> partitioned{6, 1, 46, 21, 36, 11, 41, 31, 16, 26, 19, 4, 44, 29, 39, 9, 34, 14, 24, 2, 12, 7, 17, 22,
        47, 27, 32, 37, 42, 20, 15, 0, 10, 25, 40, 30, 5, 45, 35, 18, 23, 33, 13, 38, 8, 28, 43, 3};
    correct &= ExpectOrder("median of nine partition", keys, partitioned);
    correct &= ExpectOrderS("median of nine partition", keys, partitioned);
    std::vector<int> manyKeys;
    for (int index = 0; index < 1000; ++index) manyKeys.push_back((index * 37 + 11) % 7);
    correct &= ExpectOrderHash("nested partitions", manyKeys, 0xefee58d9u);
    correct &= ExpectAdversaryFallsBackToHeapSort(false);
    correct &= ExpectAdversaryFallsBackToHeapSort(true);
    correct &= ExpectConsoleEdges();
    correct &= ExpectShortArraysUntouched();
    for (const std::size_t size : {1, 3, 8, 24})
        for (const std::size_t count : {0, 1, 2, 6, 7, 8, 40, 41, 1000})
            correct &= ExpectSorted(size, count);
    correct &= CheckMergesort();
    correct &= CheckMergesortComparisons();
    correct &= CheckMergesortUnalignedElements();
    correct &= CheckMergesortCleanup();
    correct &= CheckHeapsort();
    correct &= CheckHeapsortComparisons();
    return correct ? 0 : 1;
}
