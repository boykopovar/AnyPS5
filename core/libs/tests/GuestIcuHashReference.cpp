#include "uhash.h"
#include "ustr_imp.h"

#include <cstdint>

struct ReferenceTable {
    UHashtable* hash;
    bool integerKeys;
};

ReferenceTable* ReferenceOpen(std::int32_t keys, std::int32_t size) {
    UErrorCode status = U_ZERO_ERROR;
    UHashFunction* hasher = keys == 0 ? uhash_hashChars : keys == 1 ? uhash_hashIChars : uhash_hashLong;
    UKeyComparator* comparator = keys == 0 ? uhash_compareChars : keys == 1 ? uhash_compareIChars : uhash_compareLong;
    UHashtable* hash = uhash_openSize(hasher, comparator, uhash_compareLong, size, &status);
    if (U_FAILURE(status)) return nullptr;
    return new ReferenceTable{hash, keys == 2};
}

void ReferenceClose(ReferenceTable* table) {
    uhash_close(table->hash);
    delete table;
}

void ReferenceSetResizePolicy(ReferenceTable* table, std::int32_t policy) {
    uhash_setResizePolicy(table->hash, static_cast<UHashResizePolicy>(policy));
}

std::int32_t ReferencePut(ReferenceTable* table, std::intptr_t key, std::int32_t value, std::int32_t* status) {
    UErrorCode code = static_cast<UErrorCode>(*status);
    std::int32_t result = table->integerKeys ? uhash_iputi(table->hash, static_cast<std::int32_t>(key), value, &code)
                                             : uhash_puti(table->hash, reinterpret_cast<void*>(key), value, &code);
    *status = code;
    return result;
}

std::int32_t ReferenceGet(ReferenceTable* table, std::intptr_t key) {
    return table->integerKeys ? uhash_igeti(table->hash, static_cast<std::int32_t>(key)) : uhash_geti(table->hash, reinterpret_cast<void*>(key));
}

std::int32_t ReferenceRemove(ReferenceTable* table, std::intptr_t key) {
    return table->integerKeys ? uhash_iremovei(table->hash, static_cast<std::int32_t>(key)) : uhash_removei(table->hash, reinterpret_cast<void*>(key));
}

std::int32_t ReferenceCount(ReferenceTable* table) {
    return uhash_count(table->hash);
}

std::int32_t ReferenceLength(ReferenceTable* table) {
    return table->hash->length;
}

std::int32_t ReferenceNext(ReferenceTable* table, std::int32_t position, std::int32_t* hashcode, std::int32_t* value) {
    const UHashElement* element = uhash_nextElement(table->hash, &position);
    if (!element) return -2;
    *hashcode = element->hashcode;
    *value = element->value.integer;
    return position;
}

std::int32_t ReferenceHashChars(const char* string) {
    UHashTok key;
    key.pointer = const_cast<char*>(string);
    return uhash_hashChars(key);
}

std::int32_t ReferenceHashIChars(const char* string) {
    UHashTok key;
    key.pointer = const_cast<char*>(string);
    return uhash_hashIChars(key);
}

std::int32_t ReferenceHashUChars(const char16_t* string) {
    UHashTok key;
    key.pointer = const_cast<char16_t*>(string);
    return uhash_hashUChars(key);
}

std::int32_t ReferenceHashUCharsN(const char16_t* string, std::int32_t length) {
    return ustr_hashUCharsN(string, length);
}

std::int32_t ReferenceHashCharsN(const char* string, std::int32_t length) {
    return ustr_hashCharsN(string, length);
}

std::int32_t ReferenceHashICharsN(const char* string, std::int32_t length) {
    return ustr_hashICharsN(string, length);
}

bool ReferenceCompareIChars(const char* left, const char* right) {
    UHashTok a;
    UHashTok b;
    a.pointer = const_cast<char*>(left);
    b.pointer = const_cast<char*>(right);
    return uhash_compareIChars(a, b);
}

bool ReferenceCompareChars(const char* left, const char* right) {
    UHashTok a;
    UHashTok b;
    a.pointer = const_cast<char*>(left);
    b.pointer = const_cast<char*>(right);
    return uhash_compareChars(a, b);
}

bool ReferenceCompareUChars(const char16_t* left, const char16_t* right) {
    UHashTok a;
    UHashTok b;
    a.pointer = const_cast<char16_t*>(left);
    b.pointer = const_cast<char16_t*>(right);
    return uhash_compareUChars(a, b);
}
