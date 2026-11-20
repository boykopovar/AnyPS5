#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu uhash: " + message);
}

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t EntryCount = 200;

int deletedValues = 0;

void APS5_VABI CountDelete(void* value) {
    (void)value;
    ++deletedValues;
}

UHashTok Integer(std::int32_t value) {
    UHashTok token;
    token.integer = value;
    return token;
}

// A hash function that ignores the key, so every key lands in the same bucket. This is what forces
// the probe loop to tell colliding keys apart with the comparator instead of the hash code.
std::int32_t APS5_VABI ConstantHash(const UHashTok) {
    return 0x5A5A;
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());

    std::int32_t status = ZeroError;
    UHashtable* table = uhash_open_nid_postfix(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix, nullptr,
        &status);
    Require(status == ZeroError && table != nullptr, "uhash_open returned no table");
    Require(uhash_count_nid_postfix(table) == 0, "the new table is not empty");

    Require(uhash_put_nid_postfix(table, const_cast<char*>("alpha"), reinterpret_cast<void*>(1), &status) == nullptr,
        "the first put returned a previous value");
    uhash_put_nid_postfix(table, const_cast<char*>("beta"), reinterpret_cast<void*>(2), &status);
    uhash_put_nid_postfix(table, const_cast<char*>("gamma"), reinterpret_cast<void*>(3), &status);
    Require(status == ZeroError, "put reported an error");
    Require(uhash_count_nid_postfix(table) == 3, "the table does not hold three entries");

    Require(uhash_get_nid_postfix(table, "beta") == reinterpret_cast<void*>(2), "get returned the wrong value");
    Require(uhash_get_nid_postfix(table, "delta") == nullptr, "get returned a value for a missing key");
    Require(uhash_find_nid_postfix(table, "gamma") != nullptr, "find did not locate a stored key");
    Require(uhash_find_nid_postfix(table, "delta") == nullptr, "find located a missing key");

    Require(uhash_puti_nid_postfix(table, const_cast<char*>("answer"), 42, &status) == 0,
        "puti returned a previous integer");
    Require(uhash_geti_nid_postfix(table, "answer") == 42, "geti returned the wrong integer");
    Require(uhash_count_nid_postfix(table) == 4, "puti changed the count wrongly");

    const void* previous = uhash_put_nid_postfix(table, const_cast<char*>("beta"), reinterpret_cast<void*>(22), &status);
    Require(previous == reinterpret_cast<void*>(2), "put did not return the replaced value");
    Require(uhash_get_nid_postfix(table, "beta") == reinterpret_cast<void*>(22), "the replaced value is not stored");

    Require(uhash_remove_nid_postfix(table, "beta") == reinterpret_cast<void*>(22), "remove returned the wrong value");
    Require(uhash_count_nid_postfix(table) == 3, "remove did not shrink the table");
    Require(uhash_get_nid_postfix(table, "beta") == nullptr, "the removed key is still present");

    std::int32_t position = -1;
    std::int32_t seen = 0;
    const UHashElement* element = nullptr;
    while ((element = uhash_nextElement_nid_postfix(table, &position)) != nullptr) {
        Require(element->key.pointer != nullptr, "an iterated element has no key");
        ++seen;
    }
    Require(seen == 3, "iteration did not visit every entry");

    for (std::int32_t index = 0; index < EntryCount; ++index) {
        static char keys[EntryCount][16];
        std::snprintf(keys[index], sizeof(keys[index]), "key-%d", static_cast<int>(index));
        uhash_put_nid_postfix(table, keys[index], reinterpret_cast<void*>(static_cast<std::uintptr_t>(index + 1)),
            &status);
        Require(status == ZeroError, "put failed while growing the table");
    }
    Require(uhash_count_nid_postfix(table) == EntryCount + 3, "the grown table has the wrong count");
    Require(uhash_get_nid_postfix(table, "key-199") == reinterpret_cast<void*>(200),
        "a key stored after the growth is missing");
    Require(uhash_get_nid_postfix(table, "key-0") == reinterpret_cast<void*>(1),
        "a key stored before the growth is missing");

    uhash_setValueDeleter_nid_postfix(table, CountDelete);
    uhash_removeAll_nid_postfix(table);
    Require(uhash_count_nid_postfix(table) == 0, "removeAll left entries behind");
    Require(deletedValues == EntryCount + 3, "removeAll did not delete every value");
    uhash_close_nid_postfix(table);

    // Every key below hashes to the same bucket, so a lookup can only succeed by comparing keys.
    status = ZeroError;
    UHashtable* collisions = uhash_openSize_nid_postfix(ConstantHash, uhash_compareChars_nid_postfix,
        uhash_compareLong_nid_postfix, 4, &status);
    Require(status == ZeroError && collisions != nullptr, "the constant-hash table did not open");

    Require(uhash_put_nid_postfix(collisions, const_cast<char*>("one"), reinterpret_cast<void*>(11),
            &status) == nullptr, "the first colliding put returned a previous value");
    Require(uhash_put_nid_postfix(collisions, const_cast<char*>("two"), reinterpret_cast<void*>(22),
            &status) == nullptr, "the second colliding put returned a previous value");
    Require(uhash_put_nid_postfix(collisions, const_cast<char*>("three"), reinterpret_cast<void*>(33),
            &status) == nullptr, "the third colliding put returned a previous value");
    Require(uhash_count_nid_postfix(collisions) == 3, "the constant-hash table does not hold three entries");
    Require(uhash_get_nid_postfix(collisions, "one") == reinterpret_cast<void*>(11),
        "the first colliding key is missing");
    Require(uhash_get_nid_postfix(collisions, "two") == reinterpret_cast<void*>(22),
        "the second colliding key is missing");
    Require(uhash_get_nid_postfix(collisions, "three") == reinterpret_cast<void*>(33),
        "the third colliding key is missing");
    Require(uhash_get_nid_postfix(collisions, "absent") == nullptr,
        "get returned a colliding key's value for an absent key");
    Require(uhash_find_nid_postfix(collisions, "absent") == nullptr, "find located an absent colliding key");

    Require(uhash_put_nid_postfix(collisions, const_cast<char*>("four"), reinterpret_cast<void*>(44),
            &status) == nullptr, "a colliding put replaced an existing entry");
    Require(uhash_get_nid_postfix(collisions, "one") == reinterpret_cast<void*>(11),
        "a colliding put destroyed an earlier entry");
    Require(uhash_get_nid_postfix(collisions, "four") == reinterpret_cast<void*>(44),
        "the fourth colliding key is missing");
    Require(uhash_count_nid_postfix(collisions) == 4, "a colliding put changed the count wrongly");

    uhash_puti_nid_postfix(collisions, const_cast<char*>("five"), 55, &status);
    Require(uhash_geti_nid_postfix(collisions, "five") == 55, "geti did not find a colliding integer value");
    Require(uhash_remove_nid_postfix(collisions, "four") == reinterpret_cast<void*>(44),
        "remove returned the wrong colliding value");
    Require(uhash_get_nid_postfix(collisions, "four") == nullptr, "the removed colliding key is still present");
    Require(uhash_get_nid_postfix(collisions, "one") == reinterpret_cast<void*>(11),
        "removing a colliding key destroyed another entry");
    Require(uhash_geti_nid_postfix(collisions, "five") == 55, "removing a colliding key destroyed another entry");
    uhash_close_nid_postfix(collisions);

    status = ZeroError;
    UHashtable* collidingNumbers = uhash_openSize_nid_postfix(ConstantHash, uhash_compareLong_nid_postfix, nullptr, 4,
        &status);
    Require(status == ZeroError && collidingNumbers != nullptr, "the integer constant-hash table did not open");
    uhash_iput_nid_postfix(collidingNumbers, 1, reinterpret_cast<void*>(10), &status);
    uhash_iput_nid_postfix(collidingNumbers, 2, reinterpret_cast<void*>(20), &status);
    Require(uhash_iget_nid_postfix(collidingNumbers, 1) == reinterpret_cast<void*>(10),
        "the first colliding integer key is missing");
    Require(uhash_iget_nid_postfix(collidingNumbers, 2) == reinterpret_cast<void*>(20),
        "the second colliding integer key is missing");
    Require(uhash_iget_nid_postfix(collidingNumbers, 3) == nullptr,
        "iget returned a colliding key's value for an absent key");
    uhash_close_nid_postfix(collidingNumbers);

    status = ZeroError;
    UHashtable* numbers = uhash_open_nid_postfix(uhash_hashLong_nid_postfix, uhash_compareLong_nid_postfix, nullptr,
        &status);
    Require(status == ZeroError && numbers != nullptr, "the integer-key table did not open");
    uhash_iput_nid_postfix(numbers, 7, reinterpret_cast<void*>(70), &status);
    uhash_iputi_nid_postfix(numbers, 8, 80, &status);
    Require(uhash_iget_nid_postfix(numbers, 7) == reinterpret_cast<void*>(70), "iget returned the wrong value");
    Require(uhash_igeti_nid_postfix(numbers, 8) == 80, "igeti returned the wrong integer");
    Require(uhash_iremove_nid_postfix(numbers, 7) == reinterpret_cast<void*>(70), "iremove returned the wrong value");
    Require(uhash_iget_nid_postfix(numbers, 7) == nullptr, "the removed integer key is still present");
    Require(uhash_iremovei_nid_postfix(numbers, 8) == 80, "iremovei returned the wrong integer");

    uhash_puti_nid_postfix(numbers, reinterpret_cast<void*>(const_cast<char*>("one")), 1, &status);
    uhash_setKeyComparator_nid_postfix(numbers, uhash_compareChars_nid_postfix);
    uhash_setValueComparator_nid_postfix(numbers, uhash_compareLong_nid_postfix);
    uhash_close_nid_postfix(numbers);

    status = ZeroError;
    UHashtable* first = uhash_openSize_nid_postfix(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix,
        uhash_compareLong_nid_postfix, 4, &status);
    UHashtable* second = uhash_openSize_nid_postfix(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix,
        uhash_compareLong_nid_postfix, 4, &status);
    Require(status == ZeroError && first != nullptr && second != nullptr, "openSize failed");
    Require(uhash_equals_nid_postfix(first, second) == 1, "two empty tables do not compare equal");
    uhash_puti_nid_postfix(first, const_cast<char*>("x"), 5, &status);
    Require(uhash_equals_nid_postfix(first, second) == 0, "different tables compare equal");
    uhash_puti_nid_postfix(second, const_cast<char*>("x"), 5, &status);
    Require(uhash_equals_nid_postfix(first, second) == 1, "identical tables do not compare equal");
    uhash_remove_nid_postfix(first, "x");
    uhash_puti_nid_postfix(first, const_cast<char*>("y"), 5, &status);
    Require(uhash_equals_nid_postfix(first, second) == 0, "tables with different keys compare equal");
    uhash_close_nid_postfix(first);
    uhash_close_nid_postfix(second);

    // The value comparator is an equality predicate: a nonzero result means the values match.
    status = ZeroError;
    UHashtable* valuesFirst = uhash_openSize_nid_postfix(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix,
        uhash_compareLong_nid_postfix, 4, &status);
    UHashtable* valuesSecond = uhash_openSize_nid_postfix(uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix,
        uhash_compareLong_nid_postfix, 4, &status);
    Require(status == ZeroError && valuesFirst != nullptr && valuesSecond != nullptr,
        "the value-comparison tables did not open");
    uhash_puti_nid_postfix(valuesFirst, const_cast<char*>("left"), 10, &status);
    uhash_puti_nid_postfix(valuesFirst, const_cast<char*>("right"), 20, &status);
    uhash_puti_nid_postfix(valuesSecond, const_cast<char*>("right"), 20, &status);
    uhash_puti_nid_postfix(valuesSecond, const_cast<char*>("left"), 10, &status);
    Require(uhash_equals_nid_postfix(valuesFirst, valuesSecond) == 1,
        "tables with identical key/value sets do not compare equal");
    uhash_puti_nid_postfix(valuesSecond, const_cast<char*>("right"), 30, &status);
    Require(uhash_equals_nid_postfix(valuesFirst, valuesSecond) == 0,
        "tables with a different value compare equal");
    uhash_close_nid_postfix(valuesFirst);
    uhash_close_nid_postfix(valuesSecond);

    status = ZeroError;
    UHashtable inlineTable;
    Require(uhash_init_nid_postfix(&inlineTable, uhash_hashChars_nid_postfix, uhash_compareChars_nid_postfix, nullptr,
        &status) == &inlineTable, "init did not return the supplied table");
    uhash_puti_nid_postfix(&inlineTable, const_cast<char*>("inline"), 9, &status);
    Require(uhash_geti_nid_postfix(&inlineTable, "inline") == 9, "the supplied table does not hold the value");
    uhash_close_nid_postfix(&inlineTable);

    Require(uhash_hashChars_nid_postfix(UHashTok{const_cast<char*>("abc")}) ==
        uhash_hashChars_nid_postfix(UHashTok{const_cast<char*>("abc")}), "the string hash is not stable");
    Require(uhash_compareChars_nid_postfix(UHashTok{const_cast<char*>("abc")}, UHashTok{const_cast<char*>("abc")}) == 1,
        "the string comparator rejects equal strings");
    Require(uhash_compareIChars_nid_postfix(UHashTok{const_cast<char*>("ABC")}, UHashTok{const_cast<char*>("abc")}) == 1,
        "the caseless comparator is case sensitive");
    Require(uhash_hashLong_nid_postfix(Integer(88)) == 88, "the integer hash does not return the key");
    Require(uhash_hashChars_nid_postfix(UHashTok{nullptr}) == 0, "the string hash does not accept a null key");
    Require(uhash_hashIChars_nid_postfix(UHashTok{nullptr}) == 0, "the caseless hash does not accept a null key");
    Require(uhash_hashUChars_nid_postfix(UHashTok{nullptr}) == 0, "the wide hash does not accept a null key");
    Require(uhash_compareLong_nid_postfix(Integer(88), Integer(88)) == 1, "the integer comparator rejects equal keys");
    return 0;
}
