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
void* APS5_VABI sk_value_nid_postfix(const TestStack*, int);
void* APS5_VABI sk_pop_nid_postfix(TestStack*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL stack check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
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

static void TestValue() {
    char first = 'a';
    char second = 'b';
    char third = 'c';
    Require(sk_value_nid_postfix(nullptr, 0) == nullptr);
    char* elements[4] {&first, nullptr, &third, &second};
    TestStack stack {3, elements, 0, 4, nullptr};
    Require(sk_value_nid_postfix(&stack, 0) == &first);
    Require(sk_value_nid_postfix(&stack, 1) == nullptr);
    Require(sk_value_nid_postfix(&stack, 2) == &third);
    Require(sk_value_nid_postfix(&stack, 3) == nullptr);
    Require(sk_value_nid_postfix(&stack, -1) == nullptr);
    stack.num = -2;
    Require(sk_value_nid_postfix(&stack, 0) == nullptr);
    TestStack detached {2, nullptr, 0, 0, nullptr};
    Require(sk_value_nid_postfix(&detached, 2) == nullptr);
    RequireThrows([&] { sk_value_nid_postfix(&detached, 1); });
}

static void TestPop() {
    char first = 'a';
    char third = 'c';
    Require(sk_pop_nid_postfix(nullptr) == nullptr);
    char* elements[8] {&first, nullptr, &third};
    TestStack stack {3, elements, 0, 8, nullptr};
    Require(sk_pop_nid_postfix(&stack) == &third);
    Require(sk_num_nid_postfix(&stack) == 2 && stack.numAlloc == 8 && stack.data == elements && elements[2] == &third);
    Require(sk_value_nid_postfix(&stack, 2) == nullptr);
    Require(sk_pop_nid_postfix(&stack) == nullptr && sk_num_nid_postfix(&stack) == 1);
    Require(sk_pop_nid_postfix(&stack) == &first && sk_num_nid_postfix(&stack) == 0);
    Require(sk_pop_nid_postfix(&stack) == nullptr && sk_num_nid_postfix(&stack) == 0);
    stack.num = -2;
    Require(sk_pop_nid_postfix(&stack) == nullptr && sk_num_nid_postfix(&stack) == -2);
    TestStack detached {2, nullptr, 0, 0, nullptr};
    RequireThrows([&] { sk_pop_nid_postfix(&detached); });
    Require(detached.num == 2);
}

int main() {
    TestNum();
    TestValue();
    TestPop();
    std::puts("libSceLibreSsl stack tests passed");
}
