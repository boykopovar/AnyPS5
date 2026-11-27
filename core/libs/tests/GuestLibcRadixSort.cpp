#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "radixsort check failed\n");
        std::abort();
    }
}

int main() {
    const unsigned char apple[] = "apple";
    const unsigned char fig[] = "fig";
    const unsigned char pear[] = "pear";
    const unsigned char* words[] = {pear, apple, fig};
    *__error_nid_postfix() = 0;
    Require(radixsort_nid_postfix(words, 3, nullptr, 0) == 0);
    Require(words[0] == apple && words[1] == fig && words[2] == pear);

    const unsigned char a1[] = "a";
    const unsigned char a2[] = "a";
    const unsigned char a3[] = "a";
    const unsigned char* tied[] = {a1, a2, a3};
    Require(sradixsort_nid_postfix(tied, 3, nullptr, 0) == 0);
    Require(tied[0] == a1 && tied[1] == a2 && tied[2] == a3);

    unsigned char table[256];
    for (int c = 0; c < 256; ++c) table[c] = static_cast<unsigned char>(c);
    table[0] = 5;
    const unsigned char* sample[] = {fig, apple};
    *__error_nid_postfix() = 0;
    Require(radixsort_nid_postfix(sample, 2, table, 0) == -1);
    Require(*__error_nid_postfix() == 22);
    return 0;
}
