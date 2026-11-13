#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU list check failed at line %u\n", location.line());
    std::exit(EXIT_FAILURE);
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

}

int main() {
    std::int32_t status = 1;
    Require(!ulist_createEmptyList_nid_postfix(&status));
    Invalid([] { ulist_createEmptyList_nid_postfix(nullptr); });
    status = -128;
    auto* list = ulist_createEmptyList_nid_postfix(&status);
    Require(list && status == -128 && ulist_getListSize_nid_postfix(list) == 0);
    Require(ulist_getListSize_nid_postfix(nullptr) == -1);
    Require(!ulist_containsString_nid_postfix(nullptr, nullptr, 10));
    Require(!ulist_removeString_nid_postfix(nullptr, nullptr));
    Require(!ulist_getNext_nid_postfix(nullptr));
    Require(!ulist_containsString_nid_postfix(list, nullptr, 0));
    ulist_addItemEndList_nid_postfix(list, "tail", 0, &status);
    ulist_addItemBeginList_nid_postfix(list, "head", 0, &status);
    ulist_addItemBeginList_nid_postfix(list, "head", 0, &status);
    ulist_addItemEndList_nid_postfix(list, nullptr, 0, &status);
    Require(ulist_getListSize_nid_postfix(list) == 3);
    Require(!ulist_getNext_nid_postfix(list));
    Require(ulist_containsString_nid_postfix(list, "head!", 4));
    Require(!ulist_containsString_nid_postfix(list, "head", 3));
    Require(!ulist_containsString_nid_postfix(list, nullptr, -1));
    Invalid([&] { ulist_containsString_nid_postfix(list, nullptr, 4); });
    Invalid([&] { ulist_removeString_nid_postfix(list, nullptr); });
    auto* enumeration = static_cast<Icu::UEnumeration*>(std::malloc(sizeof(Icu::UEnumeration)));
    *enumeration = {};
    enumeration->context = list;
    Require(ulist_getListFromEnum_nid_postfix(enumeration) == list);
    Require(ulist_count_keyword_values_nid_postfix(enumeration, &status) == 3);
    std::int32_t length = -7;
    Require(!ulist_next_keyword_value_nid_postfix(enumeration, &length, &status) && length == -7);
    ulist_reset_keyword_values_iterator_nid_postfix(enumeration, &status);
    Require(ulist_removeString_nid_postfix(list, "head"));
    Require(ulist_next_keyword_value_nid_postfix(enumeration, &length, &status) && length == 4);
    Require(ulist_removeString_nid_postfix(list, "tail"));
    Require(!ulist_next_keyword_value_nid_postfix(enumeration, &length, &status) && length == 4);
    Require(ulist_removeString_nid_postfix(list, "head"));
    Require(!ulist_removeString_nid_postfix(list, "head"));
    Require(ulist_getListSize_nid_postfix(list) == 0);
    auto* adopted = static_cast<char*>(std::malloc(2));
    adopted[0] = 'a';
    adopted[1] = 0;
    ulist_addItemEndList_nid_postfix(list, adopted, -128, &status);
    Require(list->head->forceDelete == -128);
    ulist_resetList_nid_postfix(list);
    Require(ulist_getNext_nid_postfix(list) == adopted);
    status = 1;
    Require(ulist_count_keyword_values_nid_postfix(nullptr, &status) == -1);
    Require(!ulist_next_keyword_value_nid_postfix(nullptr, &length, &status) && length == 4);
    ulist_reset_keyword_values_iterator_nid_postfix(nullptr, &status);
    ulist_addItemEndList_nid_postfix(list, std::malloc(1), 1, &status);
    Require(list->size == 1);
    ulist_addItemBeginList_nid_postfix(nullptr, std::malloc(1), 1, &status);
    status = 0;
    Invalid([&] { ulist_count_keyword_values_nid_postfix(nullptr, &status); });
    Invalid([&] { ulist_getListFromEnum_nid_postfix(nullptr); });
    ulist_close_keyword_values_iterator_nid_postfix(enumeration);
    ulist_close_keyword_values_iterator_nid_postfix(nullptr);
    ulist_deleteList_nid_postfix(nullptr);
    ulist_resetList_nid_postfix(nullptr);
    std::puts("ICU list tests passed");
}
