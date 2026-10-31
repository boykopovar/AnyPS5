#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace Icu;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU hash check failed at line %u\n", location.line());
    std::exit(EXIT_FAILURE);
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

UHashTok Token(const void* value) {
    return {.pointer = const_cast<void*>(value)};
}

std::vector<void*> deleted;

void APS5_VABI Delete_nid_no_patch(void* pointer) {
    deleted.push_back(pointer);
}

std::int32_t APS5_VABI Collision_nid_no_patch(UHashTok) {
    return -1;
}

std::int8_t APS5_VABI Noncanonical_nid_no_patch(UHashTok first, UHashTok second) {
    return first.integer == second.integer ? -128 : 0;
}

void Strings() {
    Require(ustr_hashCharsN_nid_postfix("abc", 3) == 136518);
    Require(ustr_hashUCharsN_nid_postfix(u"abc", 3) == 136518);
    Require(ustr_hashICharsN_nid_postfix("ABC", 3) == 136518);
    Require(ustr_hashCharsN_nid_postfix("a\0b", 3) == 132891);
    Require(ustr_hashUCharsN_nid_postfix(std::array<char16_t, 2>{0xffff, 0xd800}.data(), 2) == 2480091);
    Require(ustr_hashCharsN_nid_postfix("\xff", 1) == 255);
    Require(ustr_hashICharsN_nid_postfix("\xff", 1) == 255);
    Require(ustr_hashUCharsN_nid_postfix(nullptr, -2147483647) == 0);
    Require(ustr_hashCharsN_nid_postfix("a", -1) == 0);
    Require(ustr_hashICharsN_nid_postfix("a", 0) == 0);
    Invalid([] { ustr_hashCharsN_nid_postfix("a", std::numeric_limits<std::int32_t>::min()); });
    for (const auto length : {31, 32, 33, 63, 64, 65, 95, 96, 127, 128, 2048}) {
        std::string text(length, 'a');
        std::uint32_t expected = 0;
        const auto stride = (length - 32) / 32 + 1;
        for (int i = 0; i < length; i += stride) expected = expected * 37 + 97;
        Require(ustr_hashCharsN_nid_postfix(text.data(), length) == std::bit_cast<std::int32_t>(expected));
    }
    Require(uhash_hashChars_nid_postfix(Token("abc")) == 136518);
    Require(uhash_hashIChars_nid_postfix(Token("ABC")) == 136518);
    Require(uhash_hashUChars_nid_postfix(Token(u"abc")) == 136518);
    Require(uhash_hashChars_nid_postfix({}) == 0);
    Require(uhash_hashUChars_nid_postfix({}) == 0);
    Require(uhash_hashIChars_nid_postfix({}) == 0);
    Require(uhash_compareChars_nid_postfix({}, {}) == 1);
    Require(uhash_compareUChars_nid_postfix(Token(u"a"), {}) == 0);
    Require(uhash_compareChars_nid_postfix(Token("a"), Token("a")) == 1);
    Require(uhash_compareChars_nid_postfix(Token("a"), Token("A")) == 0);
    Require(uhash_compareUChars_nid_postfix(Token(u"\uffff"), Token(u"\uffff")) == 1);
    Require(uhash_compareIChars_nid_postfix(Token("aBc"), Token("AbC")) == 1);
    Require(uhash_compareIChars_nid_postfix(Token("a"), Token("aa")) == 0);
    Require(uhash_compareIChars_nid_postfix({}, Token("")) == 0);
    Require(uhash_hashLong_nid_postfix({.integer = -42}) == -42);
    Require(uhash_compareLong_nid_postfix({.integer = -1}, {.integer = -1}) == 1);
}

void Integers() {
    std::int32_t status = -128;
    UHashtable storage{};
    auto* hash = uhash_initSize_nid_postfix(&storage, uhash_hashLong_nid_postfix, Noncanonical_nid_no_patch,
                                          uhash_compareLong_nid_postfix, -1, &status);
    Require(hash == &storage && status == -128);
    Require(hash->length == 7 && hash->primeIndex == 0 && !hash->allocated);
    Require(hash->lowWaterMark == 0 && hash->highWaterMark == 3);
    Require(uhash_iputi_nid_postfix(hash, -2147483647, 0, &status) == 0);
    Require(uhash_count_nid_postfix(hash) == 1);
    Require(uhash_igeti_nid_postfix(hash, -2147483647) == 0);
    for (int i = 0; i < 200; ++i) Require(uhash_iputi_nid_postfix(hash, i, i + 1, &status) == 0);
    Require(hash->length == 509 && hash->count == 201);
    for (int i = 0; i < 200; ++i) Require(uhash_igeti_nid_postfix(hash, i) == i + 1);
    Require(uhash_iputi_nid_postfix(hash, 42, -42, &status) == 43);
    Require(uhash_iremovei_nid_postfix(hash, 42) == -42);
    Require(uhash_iremovei_nid_postfix(hash, 42) == 0);
    uhash_setResizePolicy_nid_postfix(hash, 1);
    for (int i = 0; i < 200; ++i) uhash_iremovei_nid_postfix(hash, i);
    Require(hash->length == 13 && hash->count == 1);
    Require(uhash_iremovei_nid_postfix(hash, -2147483647) == 0 && hash->count == 0 && hash->length == 7);
    uhash_close_nid_postfix(hash);
    Require(!storage.elements);
    uhash_close_nid_postfix(hash);
    hash = uhash_init_nid_postfix(&storage, Collision_nid_no_patch, uhash_compareLong_nid_postfix, nullptr, &status);
    Require(hash->length == 127 && hash->highWaterMark == 63);
    uhash_close_nid_postfix(hash);
}

void FixedAndIteration() {
    std::int32_t status = 0;
    auto* hash = uhash_openSize_nid_postfix(Collision_nid_no_patch, uhash_compareLong_nid_postfix, nullptr, 7, &status);
    uhash_setResizePolicy_nid_postfix(hash, 2);
    for (int i = 1; i <= 6; ++i) Require(uhash_iputi_nid_postfix(hash, i, i * 10, &status) == 0);
    Require(hash->length == 7 && hash->count == 6);
    Require(uhash_iputi_nid_postfix(hash, 7, 70, &status) == 0 && status == 7 && hash->count == 6);
    status = 0;
    Require(uhash_iremovei_nid_postfix(hash, 3) == 30);
    Require(uhash_igeti_nid_postfix(hash, 6) == 60);
    Require(uhash_iputi_nid_postfix(hash, 7, 70, &status) == 0 && !status);
    std::int32_t position = -1;
    std::vector<int> keys;
    while (const auto* element = uhash_nextElement_nid_postfix(hash, &position)) {
        keys.push_back(element->key.integer);
        Require(element->hashcode == 0x7fffffff);
    }
    Require(keys == std::vector<int>{6, 7, 4, 1, 5, 2});
    const auto last = position;
    Require(!uhash_nextElement_nid_postfix(hash, &position) && position == last);
    uhash_removeAll_nid_postfix(hash);
    Require(uhash_count_nid_postfix(hash) == 0 && hash->length == 7);
    position = -1;
    Require(!uhash_nextElement_nid_postfix(hash, &position) && position == -1);
    uhash_close_nid_postfix(hash);
}

void PointersAndOwnership() {
    std::int32_t status = 0;
    auto* hash = uhash_open_nid_postfix(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix, nullptr, &status);
    char key1[] = "key";
    char key2[] = "key";
    int value1 = 1;
    int value2 = 2;
    Require(!uhash_put_nid_postfix(hash, key1, &value1, &status));
    Require(uhash_get_nid_postfix(hash, key2) == &value1);
    Require(uhash_setKeyDeleter_nid_postfix(hash, Delete_nid_no_patch) == nullptr);
    Require(uhash_setValueDeleter_nid_postfix(hash, Delete_nid_no_patch) == nullptr);
    Require(!uhash_put_nid_postfix(hash, key2, &value2, &status));
    Require(deleted == std::vector<void*>{key1, &value1});
    deleted.clear();
    Require(!uhash_put_nid_postfix(hash, key2, &value2, &status) && deleted.empty());
    status = 1;
    Require(!uhash_put_nid_postfix(hash, key1, &value1, &status));
    Require(deleted == std::vector<void*>{key1, &value1});
    deleted.clear();
    Require(uhash_get_nid_postfix(hash, key1) == &value2);
    status = 0;
    const auto* element = uhash_find_nid_postfix(hash, key1);
    Require(element && element->key.pointer == key2);
    Require(!uhash_removeElement_nid_postfix(hash, element));
    Require(deleted == std::vector<void*>{key2, &value2});
    deleted.clear();
    Require(!uhash_removeElement_nid_postfix(hash, element));
    Require(!uhash_find_nid_postfix(hash, key1));
    Require(!uhash_remove_nid_postfix(hash, key1));
    Require(uhash_setKeyDeleter_nid_postfix(hash, nullptr) == Delete_nid_no_patch);
    Require(uhash_setValueDeleter_nid_postfix(hash, nullptr) == Delete_nid_no_patch);
    Require(uhash_puti_nid_postfix(hash, key1, 123, &status) == 0);
    Require(uhash_geti_nid_postfix(hash, key2) == 123);
    Require(uhash_removei_nid_postfix(hash, key2) == 123);
    Require(uhash_put_nid_postfix(hash, key1, &value1, &status) == nullptr);
    Require(uhash_put_nid_postfix(hash, key2, nullptr, &status) == &value1);
    Require(hash->count == 0);
    uhash_close_nid_postfix(hash);
    hash = uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr, &status);
    Require(!uhash_iput_nid_postfix(hash, 3, &value1, &status));
    Require(uhash_iget_nid_postfix(hash, 3) == &value1);
    Require(uhash_iremove_nid_postfix(hash, 3) == &value1);
    uhash_close_nid_postfix(hash);
}

void EqualityAndErrors() {
    std::int32_t status = 1;
    Require(!uhash_open_nid_postfix(nullptr, nullptr, nullptr, &status));
    Require(!uhash_init_nid_postfix(nullptr, nullptr, nullptr, nullptr, &status));
    status = 0;
    auto* first = uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, uhash_compareLong_nid_postfix, &status);
    auto* second = uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, uhash_compareLong_nid_postfix, &status);
    Require(uhash_equals_nid_postfix(first, second));
    Require(uhash_equals_nid_postfix(nullptr, nullptr));
    Require(!uhash_equals_nid_postfix(first, nullptr));
    uhash_iputi_nid_postfix(first, 1, 10, &status);
    Require(!uhash_equals_nid_postfix(first, second));
    uhash_iputi_nid_postfix(second, 1, 11, &status);
    Require(!uhash_equals_nid_postfix(first, second));
    uhash_iputi_nid_postfix(second, 1, 10, &status);
    Require(uhash_equals_nid_postfix(first, second));
    Require(uhash_setKeyHasher_nid_postfix(first, Collision_nid_no_patch) == uhash_hashLong_nid_postfix);
    Require(uhash_setKeyComparator_nid_postfix(first, Noncanonical_nid_no_patch) == uhash_compareLong_nid_postfix);
    Require(!uhash_equals_nid_postfix(first, second));
    Require(uhash_setValueComparator_nid_postfix(first, nullptr) == uhash_compareLong_nid_postfix);
    Invalid([&] { uhash_setResizePolicy_nid_postfix(first, 3); });
    Invalid([] { uhash_count_nid_postfix(nullptr); });
    Invalid([] { uhash_open_nid_postfix(nullptr, nullptr, nullptr, nullptr); });
    Invalid([&] { uhash_nextElement_nid_postfix(first, nullptr); });
    std::int32_t position = -2;
    Invalid([&] { uhash_nextElement_nid_postfix(first, &position); });
    uhash_close_nid_postfix(first);
    uhash_close_nid_postfix(second);
    uhash_close_nid_postfix(nullptr);
}

}

int main() {
    Strings();
    Integers();
    FixedAndIteration();
    PointersAndOwnership();
    EqualityAndErrors();
    std::puts("ICU hash tests passed");
}
