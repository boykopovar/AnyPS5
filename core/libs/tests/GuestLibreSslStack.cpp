#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

struct TestStack {
    int num;
    char** data;
    int sorted;
    int numAlloc;
    void* comp;
};

extern "C" {
int APS5_VABI sk_num_nid_postfix(const TestStack*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL stack check failed at line %u\n", location.line());
    std::abort();
}

static void TestNum() {
    char first = 'a';
    Require(sk_num_nid_postfix(nullptr) == -1);
    TestStack stack {3, nullptr, 0, 0, nullptr};
    char* elements[3] {&first, nullptr, &first};
    stack.data = elements;
    Require(sk_num_nid_postfix(&stack) == 3);
    stack.num = 0;
    Require(sk_num_nid_postfix(&stack) == 0);
    stack.num = -2;
    Require(sk_num_nid_postfix(&stack) == -2);
    TestStack detached {2, nullptr, 0, 0, nullptr};
    Require(sk_num_nid_postfix(&detached) == 2);
}

int main() {
    TestNum();
    std::puts("libSceLibreSsl stack tests passed");
}
