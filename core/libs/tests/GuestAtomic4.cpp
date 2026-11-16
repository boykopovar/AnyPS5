#include "prx/libc/include/Api.hpp"
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    volatile unsigned int value = 5;
    Require(_Atomic_fetch_add_4_nid_postfix(&value, 3u, 0) == 5u);
    Require(value == 8u);
    volatile unsigned int value = 8;
    Require(_Atomic_fetch_sub_4_nid_postfix(&value, 3u, 0) == 8u);
    Require(value == 5u);
    volatile unsigned int value = 42;
    Require(_Atomic_load_4_nid_postfix(&value, 0) == 42u);
    volatile unsigned int value = 7;
    unsigned int expected = 7;
    Require(_Atomic_compare_exchange_weak_4_nid_postfix(&value, &expected, 9u, 0, 0) != 0);
    Require(value == 9u);
    expected = 7;
    Require(_Atomic_compare_exchange_weak_4_nid_postfix(&value, &expected, 1u, 0, 0) == 0);
    Require(value == 9u);
    Require(expected == 9u);
    volatile std::uint32_t value = 11;
    std::uint32_t expected = 11;
    Require(__atomic_compare_exchange_4_nid_postfix(&value, &expected, 12u, 0, 0) != 0);
    Require(value == 12u);
    expected = 11;
    Require(__atomic_compare_exchange_4_nid_postfix(&value, &expected, 13u, 0, 0) == 0);
    Require(value == 12u);
    Require(expected == 12u);
    return 0;
}
