#include "prx/libSceIcu/libSceIcu.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <random>
#include <string>
#include <vector>

struct ReferenceTable;
ReferenceTable* ReferenceOpen(std::int32_t keys, std::int32_t size);
void ReferenceClose(ReferenceTable* table);
void ReferenceSetResizePolicy(ReferenceTable* table, std::int32_t policy);
std::int32_t ReferencePut(ReferenceTable* table, std::intptr_t key, std::int32_t value, std::int32_t* status);
std::int32_t ReferenceGet(ReferenceTable* table, std::intptr_t key);
std::int32_t ReferenceRemove(ReferenceTable* table, std::intptr_t key);
std::int32_t ReferenceCount(ReferenceTable* table);
std::int32_t ReferenceLength(ReferenceTable* table);
std::int32_t ReferenceNext(ReferenceTable* table, std::int32_t position, std::int32_t* hashcode, std::int32_t* value);
std::int32_t ReferenceHashChars(const char* string);
std::int32_t ReferenceHashIChars(const char* string);
std::int32_t ReferenceHashUChars(const char16_t* string);
std::int32_t ReferenceHashUCharsN(const char16_t* string, std::int32_t length);
std::int32_t ReferenceHashCharsN(const char* string, std::int32_t length);
std::int32_t ReferenceHashICharsN(const char* string, std::int32_t length);
bool ReferenceCompareIChars(const char* left, const char* right);
bool ReferenceCompareChars(const char* left, const char* right);
bool ReferenceCompareUChars(const char16_t* left, const char16_t* right);

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t MemoryAllocationError = 7;
constexpr std::int32_t Warning = -124;
constexpr std::int32_t Grow = 0;
constexpr std::int32_t GrowAndShrink = 1;
constexpr std::int32_t Fixed = 2;

struct Heap {
    std::vector<void*> live;
    std::int32_t allocations = 0;
    std::int32_t releases = 0;
    std::int32_t unknownReleases = 0;
    bool exhausted = false;
    std::int32_t allowed = -1;
};

Heap heap;
std::vector<void*> deletedKeys;
std::vector<void*> deletedValues;

void* APS5_VABI Allocate_nid_no_patch(const void*, std::size_t size) {
    if (heap.exhausted || heap.allowed == 0) return nullptr;
    if (heap.allowed > 0) --heap.allowed;
    void* block = std::malloc(size);
    heap.live.push_back(block);
    ++heap.allocations;
    return block;
}

void* APS5_VABI Reallocate_nid_no_patch(const void*, void*, std::size_t) {
    std::abort();
}

void APS5_VABI Release_nid_no_patch(const void*, void* block) {
    ++heap.releases;
    const auto found = std::find(heap.live.begin(), heap.live.end(), block);
    if (found == heap.live.end()) {
        ++heap.unknownReleases;
        return;
    }
    heap.live.erase(found);
    std::free(block);
}

void APS5_VABI DeleteKey_nid_no_patch(void* key) {
    deletedKeys.push_back(key);
}

void APS5_VABI DeleteValue_nid_no_patch(void* value) {
    deletedValues.push_back(value);
}

std::int32_t APS5_VABI ConstantHash_nid_no_patch(UHashTok) {
    return 5;
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU hash: %s\n", message);
    std::abort();
}

template<class F>
bool Throws(F call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

bool Live(const void* block) {
    return std::find(heap.live.begin(), heap.live.end(), block) != heap.live.end();
}

UHashTok Pointer(const void* pointer) {
    UHashTok token{};
    token.pointer = const_cast<void*>(pointer);
    return token;
}

UHashtable* Open(UHashFunction hasher, UKeyComparator comparator, UValueComparator values = nullptr) {
    std::int32_t status = Success;
    UHashtable* hash = uhash_open_nid_postfix(hasher, comparator, values, &status);
    Require(hash != nullptr && status == Success, "a table opens");
    return hash;
}

UHashtable* OpenStrings() {
    return Open(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix, uhash_compareLong_nid_postfix);
}

void Strings() {
    std::mt19937 random(68);
    std::vector<std::string> strings = {"", "a", "A", "abc", "ABC", "aBc", "abd", "zZ", "[", "@", "`", "{"};
    for (std::int32_t index = 0; index < 400; ++index) {
        std::string text;
        const std::int32_t length = static_cast<std::int32_t>(random() % 200);
        for (std::int32_t position = 0; position < length; ++position) text.push_back(static_cast<char>(1 + random() % 255));
        strings.push_back(text);
    }
    for (const std::string& text : strings) {
        const char* chars = text.c_str();
        const auto length = static_cast<std::int32_t>(text.size());
        Require(uhash_hashChars_nid_postfix(Pointer(chars)) == ReferenceHashChars(chars), "uhash_hashChars matches ICU");
        Require(uhash_hashIChars_nid_postfix(Pointer(chars)) == ReferenceHashIChars(chars), "uhash_hashIChars matches ICU");
        for (std::int32_t cut : {length, length / 2, 0, -1, -40}) {
            Require(ustr_hashCharsN_nid_postfix(chars, cut) == ReferenceHashCharsN(chars, cut), "ustr_hashCharsN matches ICU");
            Require(ustr_hashICharsN_nid_postfix(chars, cut) == ReferenceHashICharsN(chars, cut), "ustr_hashICharsN matches ICU");
        }
        std::u16string wide;
        for (char c : text) wide.push_back(static_cast<char16_t>(static_cast<unsigned char>(c) * 251u));
        Require(uhash_hashUChars_nid_postfix(Pointer(wide.c_str())) == ReferenceHashUChars(wide.c_str()), "uhash_hashUChars matches ICU");
        Require(ustr_hashUCharsN_nid_postfix(reinterpret_cast<const std::uint16_t*>(wide.c_str()), length) == ReferenceHashUCharsN(wide.c_str(), length), "ustr_hashUCharsN matches ICU");
    }
    Require(uhash_hashChars_nid_postfix(Pointer("abc")) == static_cast<std::int32_t>(('a' * 37 + 'b') * 37 + 'c'), "short strings hash every character");
    Require(uhash_hashIChars_nid_postfix(Pointer("AbC")) == uhash_hashChars_nid_postfix(Pointer("abc")), "the caseless hash lowercases ASCII");
    Require(uhash_hashChars_nid_postfix(Pointer(nullptr)) == 0 && uhash_hashIChars_nid_postfix(Pointer(nullptr)) == 0 && uhash_hashUChars_nid_postfix(Pointer(nullptr)) == 0, "a null string hashes to zero");
    Require(ustr_hashCharsN_nid_postfix(nullptr, 5) == 0 && ustr_hashICharsN_nid_postfix(nullptr, 5) == 0 && ustr_hashUCharsN_nid_postfix(nullptr, 5) == 0, "a null buffer hashes to zero");

    for (std::size_t left = 0; left < 60; ++left) {
        for (std::size_t right = 0; right < 60; ++right) {
            const char* a = strings[left].c_str();
            const char* b = strings[right].c_str();
            Require(uhash_compareChars_nid_postfix(Pointer(a), Pointer(b)) == ReferenceCompareChars(a, b), "uhash_compareChars matches ICU");
            Require(uhash_compareIChars_nid_postfix(Pointer(a), Pointer(b)) == ReferenceCompareIChars(a, b), "uhash_compareIChars matches ICU");
            std::u16string wa(strings[left].begin(), strings[left].end());
            std::u16string wb(strings[right].begin(), strings[right].end());
            Require(uhash_compareUChars_nid_postfix(Pointer(wa.c_str()), Pointer(wb.c_str())) == ReferenceCompareUChars(wa.c_str(), wb.c_str()), "uhash_compareUChars matches ICU");
        }
    }
    Require(uhash_compareIChars_nid_postfix(Pointer("aBc"), Pointer("AbC")) == 1 && uhash_compareChars_nid_postfix(Pointer("aBc"), Pointer("AbC")) == 0, "only the caseless comparison folds ASCII");
    Require(uhash_compareIChars_nid_postfix(Pointer("\xC0"), Pointer("\xE0")) == 0, "the caseless comparison folds only ASCII");
    Require(uhash_compareChars_nid_postfix(Pointer(nullptr), Pointer(nullptr)) == 1 && uhash_compareChars_nid_postfix(Pointer("a"), Pointer(nullptr)) == 0, "null strings are equal only to null");
    UHashTok a{};
    UHashTok b{};
    a.integer = -7;
    b.integer = -7;
    Require(uhash_hashLong_nid_postfix(a) == -7 && uhash_compareLong_nid_postfix(a, b) == 1, "integer keys hash to themselves");
    b.integer = 7;
    Require(uhash_compareLong_nid_postfix(a, b) == 0, "different integers differ");
}

void Iteration(UHashtable* hash, ReferenceTable* reference) {
    Require(uhash_count_nid_postfix(hash) == ReferenceCount(reference) && hash->length == ReferenceLength(reference), "count and length match ICU");
    std::int32_t position = -1;
    while (true) {
        std::int32_t referenceHash = 0;
        std::int32_t referenceValue = 0;
        const std::int32_t referencePosition = ReferenceNext(reference, position, &referenceHash, &referenceValue);
        const UHashElement* element = uhash_nextElement_nid_postfix(hash, &position);
        if (referencePosition == -2) {
            Require(element == nullptr, "iteration ends where ICU's does");
            return;
        }
        Require(element != nullptr && position == referencePosition, "elements sit in ICU's slots");
        Require(element->hashcode == referenceHash && element->value.integer == referenceValue, "slots hold ICU's hash and value");
    }
}

void Differential(std::int32_t keys, std::int32_t policy, std::int32_t size) {
    std::mt19937 random(static_cast<std::uint32_t>(keys * 100 + policy * 10 + size));
    std::vector<std::string> pool;
    for (std::int32_t index = 0; index < 300; ++index) pool.push_back((index % 3 == 0 ? "Key" : "key") + std::to_string(index * 7919 % 1000));
    UHashFunction hasher = keys == 0 ? uhash_hashChars_nid_postfix : keys == 1 ? uhash_hashIChars_nid_postfix : uhash_hashLong_nid_postfix;
    UKeyComparator comparator = keys == 0 ? uhash_compareChars_nid_postfix : keys == 1 ? uhash_compareIChars_nid_postfix : uhash_compareLong_nid_postfix;
    std::int32_t status = Success;
    UHashtable* hash = uhash_openSize_nid_postfix(hasher, comparator, uhash_compareLong_nid_postfix, size, &status);
    ReferenceTable* reference = ReferenceOpen(keys, size);
    Require(hash != nullptr && reference != nullptr && status == Success, "both tables open");
    uhash_setResizePolicy_nid_postfix(hash, policy);
    ReferenceSetResizePolicy(reference, policy);
    Iteration(hash, reference);
    for (std::int32_t step = 0; step < 6000; ++step) {
        const std::string& text = pool[random() % pool.size()];
        const std::int32_t number = static_cast<std::int32_t>(random() % 600) - 300;
        const std::intptr_t key = keys == 2 ? number : reinterpret_cast<std::intptr_t>(text.c_str());
        const std::uint32_t operation = random() % 10;
        if (operation < 5) {
            const auto value = static_cast<std::int32_t>(1 + random() % 1000000);
            std::int32_t ours = Success;
            std::int32_t theirs = Success;
            const std::int32_t previous = keys == 2 ? uhash_iputi_nid_postfix(hash, number, value, &ours) : uhash_puti_nid_postfix(hash, const_cast<char*>(text.c_str()), value, &ours);
            Require(previous == ReferencePut(reference, key, value, &theirs) && ours == theirs, "put returns ICU's previous value and status");
        } else if (operation < 8) {
            const std::int32_t removed = keys == 2 ? uhash_iremovei_nid_postfix(hash, number) : uhash_removei_nid_postfix(hash, text.c_str());
            Require(removed == ReferenceRemove(reference, key), "remove returns ICU's value");
        } else {
            const std::int32_t found = keys == 2 ? uhash_igeti_nid_postfix(hash, number) : uhash_geti_nid_postfix(hash, text.c_str());
            Require(found == ReferenceGet(reference, key), "get returns ICU's value");
        }
        if (step % 97 == 0) Iteration(hash, reference);
    }
    Iteration(hash, reference);
    uhash_close_nid_postfix(hash);
    ReferenceClose(reference);
    Require(heap.live.empty() && heap.unknownReleases == 0, "close releases the table and its elements");
}

void Pointers() {
    UHashtable* hash = OpenStrings();
    Require(hash->allocated == 1 && hash->length == 127 && hash->count == 0 && Live(hash) && Live(hash->elements), "uhash_open allocates a table of 127 slots");
    int first = 1;
    int second = 2;
    std::int32_t status = Success;
    Require(uhash_put_nid_postfix(hash, const_cast<char*>("one"), &first, &status) == nullptr && status == Success, "a new key has no previous value");
    Require(uhash_put_nid_postfix(hash, const_cast<char*>("one"), &second, &status) == &first, "a replaced key returns its previous value");
    Require(uhash_get_nid_postfix(hash, "one") == &second && uhash_get_nid_postfix(hash, "two") == nullptr, "get returns the value or null");
    const UHashElement* element = uhash_find_nid_postfix(hash, "one");
    Require(element && element->value.pointer == &second && std::strcmp(static_cast<const char*>(element->key.pointer), "one") == 0, "find returns the element");
    Require(uhash_find_nid_postfix(hash, "two") == nullptr, "find returns null for an absent key");
    Require(uhash_put_nid_postfix(hash, const_cast<char*>("one"), nullptr, &status) == &second && uhash_count_nid_postfix(hash) == 0, "a null value removes the key");
    Require(uhash_setKeyHasher_nid_postfix(hash, uhash_hashLong_nid_postfix) == uhash_hashChars_nid_postfix, "setKeyHasher returns the previous function");
    Require(uhash_setKeyComparator_nid_postfix(hash, uhash_compareLong_nid_postfix) == uhash_compareChars_nid_postfix, "setters return the previous function");
    Require(uhash_iput_nid_postfix(hash, -9, &second, &status) == nullptr && uhash_iget_nid_postfix(hash, -9) == &second, "integer keys after switching functions");
    Require(uhash_iremove_nid_postfix(hash, -9) == &second && uhash_iget_nid_postfix(hash, -9) == nullptr, "iremove returns the value");
    Require(uhash_puti_nid_postfix(hash, reinterpret_cast<void*>(std::intptr_t{11}), 0, &status) == 0 && uhash_count_nid_postfix(hash) == 1, "puti stores a zero value, as ICU 68 does");
    Require(uhash_iputi_nid_postfix(hash, 12, 0, &status) == 0 && uhash_count_nid_postfix(hash) == 2, "iputi stores a zero value");
    Require(uhash_remove_nid_postfix(hash, reinterpret_cast<void*>(std::intptr_t{-9})) == nullptr, "removing an absent key returns null");
    void* high = reinterpret_cast<void*>(std::uintptr_t{0x123456789ABCull});
    Require(uhash_iput_nid_postfix(hash, 13, high, &status) == nullptr && uhash_iget_nid_postfix(hash, 13) == high, "a pointer value keeps all 64 bits");
    Require(uhash_iput_nid_postfix(hash, 12, high, &status) == nullptr && uhash_iget_nid_postfix(hash, 12) == high, "a pointer value replaces an integer value whole");
    uhash_close_nid_postfix(hash);
    Require(heap.live.empty(), "close releases the table");
}

void Deleters() {
    UHashtable* hash = OpenStrings();
    Require(uhash_setKeyDeleter_nid_postfix(hash, DeleteKey_nid_no_patch) == nullptr && uhash_setValueDeleter_nid_postfix(hash, DeleteValue_nid_no_patch) == nullptr, "new tables have no deleters");
    std::string keyA = "a";
    std::string keyB = "a";
    int one = 1;
    int two = 2;
    std::int32_t status = Success;
    Require(uhash_put_nid_postfix(hash, keyA.data(), &one, &status) == nullptr, "first put");
    Require(uhash_put_nid_postfix(hash, keyB.data(), &two, &status) == nullptr, "a replaced value is deleted, not returned");
    Require(deletedKeys == std::vector<void*>{keyA.data()} && deletedValues == std::vector<void*>{&one}, "replacing deletes the old key and value");
    Require(uhash_put_nid_postfix(hash, keyB.data(), &two, &status) == nullptr && deletedKeys.size() == 1 && deletedValues.size() == 1, "putting the same key and value deletes nothing");
    Require(uhash_remove_nid_postfix(hash, "a") == nullptr && deletedKeys.back() == keyB.data() && deletedValues.back() == &two, "remove deletes the key and the value");
    status = IllegalArgument;
    Require(uhash_put_nid_postfix(hash, keyA.data(), &one, &status) == nullptr && status == IllegalArgument, "a set error stores nothing");
    Require(deletedKeys.back() == keyA.data() && deletedValues.back() == &one && uhash_count_nid_postfix(hash) == 0, "a refused put deletes the adopted key and value");
    status = Success;
    uhash_put_nid_postfix(hash, keyA.data(), &one, &status);
    uhash_put_nid_postfix(hash, const_cast<char*>("b"), &two, &status);
    deletedKeys.clear();
    deletedValues.clear();
    uhash_removeAll_nid_postfix(hash);
    Require(uhash_count_nid_postfix(hash) == 0 && deletedKeys.size() == 2 && deletedValues.size() == 2, "removeAll deletes every key and value");
    uhash_put_nid_postfix(hash, keyA.data(), &one, &status);
    deletedKeys.clear();
    deletedValues.clear();
    uhash_close_nid_postfix(hash);
    Require(deletedKeys == std::vector<void*>{keyA.data()} && deletedValues == std::vector<void*>{&one} && heap.live.empty(), "close deletes the remaining keys and values");
    deletedKeys.clear();
    deletedValues.clear();
}

void Elements() {
    UHashtable* hash = Open(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix);
    std::int32_t status = Success;
    for (std::int32_t key = 0; key < 20; ++key) uhash_iputi_nid_postfix(hash, key, key + 100, &status);
    std::int32_t position = -1;
    std::int32_t seen = 0;
    std::int32_t last = -1;
    while (const UHashElement* element = uhash_nextElement_nid_postfix(hash, &position)) {
        Require(position > last && element == &hash->elements[position] && element->value.integer == element->key.integer + 100, "nextElement walks the slots in order");
        last = position;
        ++seen;
    }
    Require(seen == 20 && uhash_nextElement_nid_postfix(hash, &position) == nullptr, "nextElement visits every element once");
    position = -1;
    const UHashElement* element = uhash_nextElement_nid_postfix(hash, &position);
    const std::int32_t key = element->key.integer;
    Require(uhash_removeElement_nid_postfix(hash, element) == reinterpret_cast<void*>(std::intptr_t{key + 100}) && uhash_count_nid_postfix(hash) == 19, "removeElement removes it and returns the old value token");
    Require(element->hashcode == static_cast<std::int32_t>(0x80000000u) && uhash_igeti_nid_postfix(hash, key) == 0, "a removed slot is marked deleted");
    Require(uhash_removeElement_nid_postfix(hash, element) == nullptr && uhash_count_nid_postfix(hash) == 19, "removing a deleted slot does nothing");
    Require(uhash_iremovei_nid_postfix(hash, 3) == 103 && uhash_count_nid_postfix(hash) == 18, "iremovei returns the integer value");
    uhash_setResizePolicy_nid_postfix(hash, GrowAndShrink);
    Require(hash->lowWaterMark == 12 && hash->highWaterMark == 63, "the policy sets the water marks");
    for (std::int32_t index = 0; index < 20; ++index) uhash_iremovei_nid_postfix(hash, index);
    Require(hash->length == 7 && hash->primeIndex == 0 && uhash_count_nid_postfix(hash) == 0, "a shrinking table shrinks");
    uhash_close_nid_postfix(hash);

    hash = Open(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix);
    for (std::int32_t index = 0; index < 3; ++index) uhash_iputi_nid_postfix(hash, index, index + 1, &status);
    uhash_setResizePolicy_nid_postfix(hash, GrowAndShrink);
    Require(hash->length == 61 && uhash_count_nid_postfix(hash) == 3 && uhash_igeti_nid_postfix(hash, 2) == 3, "a new policy rehashes at once");
    uhash_close_nid_postfix(hash);

    hash = Open(ConstantHash_nid_no_patch, uhash_compareLong_nid_postfix);
    for (std::int32_t index = 0; index < 50; ++index) uhash_iputi_nid_postfix(hash, index, index + 1, &status);
    for (std::int32_t index = 0; index < 50; index += 2) uhash_iremovei_nid_postfix(hash, index);
    for (std::int32_t index = 0; index < 50; ++index) Require(uhash_igeti_nid_postfix(hash, index) == (index % 2 ? index + 1 : 0), "colliding keys are found past deleted slots");
    uhash_close_nid_postfix(hash);
    Require(heap.live.empty(), "close releases the tables");
}

void Sizes() {
    std::int32_t status = Success;
    const std::int32_t expected[][2] = {{0, 7}, {7, 7}, {8, 13}, {1000, 1021}, {1021, 1021}, {-5, 7}};
    for (const auto& pair : expected) {
        UHashtable* hash = uhash_openSize_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, pair[0], &status);
        Require(hash && hash->length == pair[1] && hash->highWaterMark == pair[1] / 2 && hash->lowWaterMark == 0, "openSize picks the smallest prime that fits");
        uhash_close_nid_postfix(hash);
    }
    UHashtable* hash = uhash_openSize_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, 7, &status);
    for (std::int32_t key = 0; key < 4; ++key) uhash_iputi_nid_postfix(hash, key, 1, &status);
    Require(hash->length == 7, "a table grows only when a put finds it above the high water mark");
    uhash_iputi_nid_postfix(hash, 4, 1, &status);
    Require(hash->length == 13 && hash->primeIndex == 1 && uhash_count_nid_postfix(hash) == 5, "the next put grows the table");
    uhash_setResizePolicy_nid_postfix(hash, Fixed);
    for (std::int32_t key = 5; key < 12; ++key) uhash_iputi_nid_postfix(hash, key, 1, &status);
    Require(status == Success && uhash_count_nid_postfix(hash) == 12 && hash->length == 13, "a fixed table fills to one slot short");
    uhash_iputi_nid_postfix(hash, 12, 1, &status);
    Require(status == MemoryAllocationError && uhash_count_nid_postfix(hash) == 12, "the last slot is never filled");
    status = Success;
    Require(uhash_iputi_nid_postfix(hash, 11, 9, &status) == 1 && status == Success, "a full table still replaces values");
    Require(Throws([&] { uhash_setResizePolicy_nid_postfix(hash, 3); }) && Throws([&] { uhash_setResizePolicy_nid_postfix(hash, -1); }), "an unknown resize policy");
    uhash_close_nid_postfix(hash);
    Require(heap.live.empty(), "close releases the tables");
}

void CallerStorage() {
    UHashtable storage;
    std::memset(&storage, 0x5A, sizeof(storage));
    std::int32_t status = Success;
    Require(uhash_init_nid_postfix(&storage, uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status) == &storage, "init fills the caller's table");
    Require(storage.allocated == 0 && storage.length == 127 && storage.count == 0 && storage.keyDeleter == nullptr && storage.valueDeleter == nullptr && storage.lowWaterRatio == 0.0F && storage.highWaterRatio == 0.5F, "init sets the defaults");
    uhash_iputi_nid_postfix(&storage, 1, 2, &status);
    uhash_close_nid_postfix(&storage);
    Require(storage.elements == nullptr && heap.live.empty() && heap.unknownReleases == 0, "close releases the elements and not the caller's table");
    Require(uhash_initSize_nid_postfix(&storage, uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, 30, &status) == &storage && storage.length == 31, "initSize picks the prime");
    uhash_close_nid_postfix(&storage);
    uhash_close_nid_postfix(&storage);
    Require(heap.live.empty() && heap.unknownReleases == 0, "a second close does nothing");

    auto* object = static_cast<UHashtableObject*>(uprv_malloc_nid_postfix(sizeof(UHashtableObject)));
    uhash_init_nid_postfix(&object->hashObj, uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status);
    object->hash = &object->hashObj;
    uhash_iputi_nid_postfix(object->hash, 4, 5, &status);
    uhash_deleteHashtable_nid_postfix(object);
    Require(heap.live.empty() && heap.unknownReleases == 0, "deleteHashtable closes the embedded table and releases the object");
    object = static_cast<UHashtableObject*>(uprv_malloc_nid_postfix(sizeof(UHashtableObject)));
    object->hash = nullptr;
    uhash_deleteHashtable_nid_postfix(object);
    uhash_deleteHashtable_nid_postfix(nullptr);
    Require(heap.live.empty() && heap.unknownReleases == 0, "deleteHashtable without a table releases the object");
}

void Equality() {
    std::int32_t status = Success;
    UHashtable* a = OpenStrings();
    UHashtable* b = uhash_openSize_nid_postfix(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix, uhash_compareLong_nid_postfix, 1000, &status);
    Require(uhash_equals_nid_postfix(a, a) == 1 && uhash_equals_nid_postfix(a, b) == 1, "empty tables are equal");
    std::string key = "k";
    uhash_puti_nid_postfix(a, key.data(), 3, &status);
    Require(uhash_equals_nid_postfix(a, b) == 0, "different counts");
    uhash_puti_nid_postfix(b, const_cast<char*>("k"), 4, &status);
    Require(uhash_equals_nid_postfix(a, b) == 0, "different values");
    uhash_puti_nid_postfix(b, const_cast<char*>("k"), 3, &status);
    Require(uhash_equals_nid_postfix(a, b) == 1 && uhash_equals_nid_postfix(b, a) == 1, "equal keys and values in different layouts");
    uhash_setValueComparator_nid_postfix(b, nullptr);
    Require(uhash_equals_nid_postfix(a, b) == 0, "different value comparators");
    uhash_setValueComparator_nid_postfix(a, nullptr);
    Require(uhash_equals_nid_postfix(a, b) == 0, "no value comparator");
    Require(uhash_equals_nid_postfix(a, nullptr) == 0 && uhash_equals_nid_postfix(nullptr, nullptr) == 1, "null tables");
    uhash_close_nid_postfix(a);
    uhash_close_nid_postfix(b);
    a = OpenStrings();
    b = OpenStrings();
    uhash_puti_nid_postfix(a, const_cast<char*>("zero"), 0, &status);
    Require(uhash_equals_nid_postfix(a, b) == 0 && uhash_equals_nid_postfix(b, a) == 0, "a zero value that a lookup of a missing key would also return");
    uhash_close_nid_postfix(a);
    a = Open(uhash_hashChars_nid_postfix, uhash_compareIChars_nid_postfix, uhash_compareLong_nid_postfix);
    Require(uhash_equals_nid_postfix(a, b) == 0, "different key comparators");
    uhash_close_nid_postfix(a);
    uhash_close_nid_postfix(b);
    Require(heap.live.empty(), "close releases the tables");
}

void Refusals() {
    const std::int32_t allocations = heap.allocations;
    for (std::int32_t incoming : {IllegalArgument, MemoryAllocationError}) {
        std::int32_t status = incoming;
        UHashtable storage{};
        Require(uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status) == nullptr && status == incoming, "a set error opens nothing");
        Require(uhash_init_nid_postfix(&storage, uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status) == nullptr && storage.elements == nullptr, "a set error initializes nothing");
        Require(uhash_init_nid_postfix(nullptr, uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status) == nullptr, "a set error does not touch a null table");
    }
    Require(heap.allocations == allocations, "a set error allocates nothing");
    std::int32_t status = Warning;
    UHashtable* hash = uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status);
    Require(hash != nullptr && status == Warning, "a warning does not stop open");
    Require(uhash_iputi_nid_postfix(hash, 1, 2, &status) == 0 && uhash_igeti_nid_postfix(hash, 1) == 2 && status == Warning, "a warning does not stop put");

    heap.exhausted = true;
    status = Success;
    Require(uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status) == nullptr && status == MemoryAllocationError, "a failed allocation reports a memory error");
    heap.exhausted = false;
    heap.allowed = 1;
    status = Success;
    Require(uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status) == nullptr && status == MemoryAllocationError, "a failed element allocation reports a memory error");
    heap.allowed = -1;
    Require(heap.live.size() == 2 && heap.unknownReleases == 0, "a failed open releases its table");

    uhash_setValueDeleter_nid_postfix(hash, DeleteValue_nid_no_patch);
    status = Success;
    for (std::int32_t key = 2; key < 65; ++key) uhash_iputi_nid_postfix(hash, key, key, &status);
    Require(status == Success && hash->length == 127 && uhash_count_nid_postfix(hash) == 64, "the table is at its high water mark");
    heap.exhausted = true;
    int value = 0;
    Require(uhash_iput_nid_postfix(hash, 500, &value, &status) == nullptr && status == MemoryAllocationError, "a failed growth refuses the put");
    Require(deletedValues == std::vector<void*>{&value} && hash->length == 127 && hash->primeIndex == 5 && uhash_igeti_nid_postfix(hash, 63) == 63, "the refused value is deleted and the old elements stay");
    heap.exhausted = false;
    deletedValues.clear();
    uhash_setValueDeleter_nid_postfix(hash, nullptr);
    status = Success;

    Require(Throws([&] { uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, nullptr); }), "open with a null status");
    Require(Throws([&] { uhash_iputi_nid_postfix(hash, 1, 1, nullptr); }), "put with a null status");
    Require(Throws([&] { uhash_init_nid_postfix(nullptr, uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status); }), "init of a null table");
    Require(Throws([&] { uhash_count_nid_postfix(nullptr); }), "count of a null table");
    Require(Throws([&] { uhash_get_nid_postfix(nullptr, "a"); }), "get from a null table");
    Require(Throws([&] { uhash_iputi_nid_postfix(nullptr, 1, 1, &status); }), "put into a null table");
    Require(Throws([&] { uhash_iremovei_nid_postfix(nullptr, 1); }), "remove from a null table");
    Require(Throws([&] { uhash_nextElement_nid_postfix(hash, nullptr); }), "nextElement with a null position");
    Require(Throws([&] { uhash_removeElement_nid_postfix(hash, nullptr); }), "removeElement of a null element");
    Require(Throws([&] { uhash_setKeyHasher_nid_postfix(nullptr, nullptr); }), "a setter on a null table");
    UHashFunction hasher = uhash_setKeyHasher_nid_postfix(hash, nullptr);
    Require(Throws([&] { uhash_igeti_nid_postfix(hash, 1); }), "a lookup without a hash function");
    uhash_setKeyHasher_nid_postfix(hash, hasher);
    uhash_setKeyComparator_nid_postfix(hash, nullptr);
    Require(Throws([&] { uhash_igeti_nid_postfix(hash, 1); }), "a lookup without a key comparator");
    uhash_close_nid_postfix(nullptr);
    uhash_close_nid_postfix(hash);
    Require(heap.live.empty() && heap.unknownReleases == 0, "every block is released once");
}

}

int main() {
    std::int32_t status = Success;
    u_setMemoryFunctions_nid_postfix(nullptr, Allocate_nid_no_patch, Reallocate_nid_no_patch, Release_nid_no_patch, &status);
    Require(status == Success, "the tracking allocator is installed");
    Strings();
    for (std::int32_t keys = 0; keys < 3; ++keys) {
        Differential(keys, Grow, 0);
        Differential(keys, GrowAndShrink, 0);
        Differential(keys, Fixed, 1000);
        Differential(keys, Grow, 7);
    }
    Pointers();
    Deleters();
    Elements();
    Sizes();
    CallerStorage();
    Equality();
    Refusals();
    u_cleanup_nid_postfix();
    std::puts("ICU hash tests passed");
    return 0;
}
