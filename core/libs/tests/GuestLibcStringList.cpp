#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "stringlist check failed\n");
        std::abort();
    }
}

int main() {
    StringList* list = sl_init_nid_postfix();
    Require(list != nullptr);
    Require(list->sl_cur == 0 && list->sl_max == 20);
    Require(sl_find_nid_postfix(list, "absent") == nullptr);

    char* stored[45];
    for (int i = 0; i < 45; ++i) {
        const std::string text = "entry" + std::to_string(i);
        stored[i] = strdup_nid_postfix(text.c_str());
        Require(stored[i] != nullptr);
        Require(sl_add_nid_postfix(list, stored[i]) == 0);
    }
    Require(list->sl_cur == 45);
    Require(list->sl_max == 60);
    for (int i = 0; i < 45; ++i) {
        const std::string text = "entry" + std::to_string(i);
        Require(sl_find_nid_postfix(list, text.c_str()) == stored[i]);
    }
    Require(sl_find_nid_postfix(list, "entry45") == nullptr);
    sl_free_nid_postfix(list, 1);
    sl_free_nid_postfix(nullptr, 1);

    StringList* borrowed = sl_init_nid_postfix();
    Require(borrowed != nullptr);
    char name[] = "borrowed";
    Require(sl_add_nid_postfix(borrowed, name) == 0);
    Require(sl_find_nid_postfix(borrowed, "borrowed") == name);
    sl_free_nid_postfix(borrowed, 0);
    return 0;
}
